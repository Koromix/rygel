// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "src/core/wrap/json.hh"
#include "rekkord.hh"
#include "vendor/pugixml/src/pugixml.hpp"
#include <iostream> // for pugixml

namespace RG {

int RunSnapshots(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    OutputFormat format = OutputFormat::Plain;
    int verbose = 0;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 snapshots [-R <repo>]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password

    %!..+-j, --threads <threads>%!0      Change number of threads
                                 %!D..(default: automatic)%!0

    %!..+-f, --format <format>%!0        Change output format
                                 %!D..(default: %2)%!0
    %!..+-v, --verbose%!0                Enable verbose output (plain only)

Available output formats: %!..+%3%!0)", FelixTarget, OutputFormatNames[(int)format], FmtSpan(OutputFormatNames));
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                config.username = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                config.password = opt.current_value;
            } else if (opt.Test("-j", "--threads", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &config.threads))
                    return 1;
                if (config.threads < 1) {
                    LogError("Threads count cannot be < 1");
                    return 1;
                }
            } else if (opt.Test("-f", "--format", OptionType::Value)) {
                if (!OptionToEnumI(OutputFormatNames, opt.current_value, &format)) {
                    LogError("Unknown output format '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-v", "--verbose")) {
                verbose++;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::Full) {
        LogError("You must use the read-write password with this command");
        return 1;
    }
    LogInfo();

    HeapArray<rk_SnapshotInfo> snapshots;
    if (!rk_Snapshots(disk.get(), &temp_alloc, &snapshots))
        return 1;

    switch (format) {
        case OutputFormat::Plain: {
            if (snapshots.len) {
                for (const rk_SnapshotInfo &snapshot: snapshots) {
                    TimeSpec spec = DecomposeTime(snapshot.time);

                    if (snapshot.name) {
                        PrintLn("%1   %!..+%2%!0 [%3]", snapshot.hash, FmtArg(snapshot.name).Pad(24), FmtTimeNice(spec));
                    } else {
                        PrintLn("%1   %!D..(anonymous)%!0              [%2]", snapshot.hash, FmtTimeNice(spec));
                    }

                    if (verbose) {
                        PrintLn("+ Size: %!..+%1%!0", FmtDiskSize(snapshot.len));
                        PrintLn("+ Storage: %!..+%1%!0", FmtDiskSize(snapshot.stored));
                    }
                }
            } else {
                LogInfo("There does not seem to be any snapshot");
            }
        } break;

        case OutputFormat::JSON: {
            json_PrettyWriter json(&stdout_st);

            json.StartArray();
            for (const rk_SnapshotInfo &snapshot: snapshots) {
                json.StartObject();

                char hash[128];
                Fmt(hash, "%1", snapshot.hash);

                json.Key("hash"); json.String(hash);
                if (snapshot.name) {
                    json.Key("name"); json.String(snapshot.name);
                } else {
                    json.Key("name"); json.Null();
                }
                json.Key("time"); json.Int64(snapshot.time);
                json.Key("size"); json.Int64(snapshot.len);
                json.Key("storage"); json.Int64(snapshot.stored);

                json.EndObject();
            }
            json.EndArray();

            json.Flush();
            PrintLn();
        } break;

        case OutputFormat::XML: {
            pugi::xml_document doc;
            pugi::xml_node root = doc.append_child("Snapshots");

            for (const rk_SnapshotInfo &snapshot: snapshots) {
                pugi::xml_node element = root.append_child("Snapshot");

                char hash[128];
                Fmt(hash, "%1", snapshot.hash);

                element.append_attribute("hash") = hash;
                element.append_attribute("name") = snapshot.name ? snapshot.name : "";
                element.append_attribute("time") = snapshot.time;
                element.append_attribute("size") = snapshot.len;
                element.append_attribute("storage") = snapshot.stored;
            }

            doc.save(std::cout, "    ");
        } break;
    }

    return 0;
}

static void ListObjectPlain(const rk_ObjectInfo &obj, int start_depth, int verbose)
{
    TimeSpec mspec = DecomposeTime(obj.mtime);
    int indent = (start_depth + obj.depth) * 2;

    char suffix = (obj.type == rk_ObjectType::Directory) ? '/' : ' ';
    int align = std::max(60 - indent - strlen(obj.name), (size_t)0);
    bool size = (obj.readable && obj.type == rk_ObjectType::File);

    if (obj.type != rk_ObjectType::Link && obj.mode) {
        PrintLn("%1%!D..[%2] %!0%!..+%3%4%!0%5 %!D..(0%6)%!0 [%7] %!.._%8%!0",
                FmtArg(" ").Repeat(indent), rk_ObjectTypeNames[(int)obj.type][0],
                obj.name, suffix, FmtArg(" ").Repeat(align), FmtOctal(obj.mode).Pad0(-3),
                FmtTimeNice(mspec), size ? FmtDiskSize(obj.size) : FmtArg(""));
    } else {
        PrintLn("%1%!D..[%2] %!0%!..+%3%4%!0%5        [%6] %!.._%7%!0",
                FmtArg(" ").Repeat(indent), rk_ObjectTypeNames[(int)obj.type][0],
                obj.name, suffix, FmtArg(" ").Repeat(align),
                FmtTimeNice(mspec), size ? FmtDiskSize(obj.size) : FmtArg(""));
    }

    if (verbose >= 1) {
        PrintLn("%1    + Hash: %!..+%2%!0", FmtArg(" ").Repeat(indent), obj.hash);
    }
    if (obj.type != rk_ObjectType::Snapshot) {
        if (verbose >= 1) {
            PrintLn("%1    + UID/GID: %!..+%2:%3%!0", FmtArg(" ").Repeat(indent), obj.uid, obj.gid);
        }
        if (verbose > 1) {
            TimeSpec bspec = DecomposeTime(obj.btime);
            PrintLn("%1    + Birth time: %!..+%2%!0", FmtArg(" ").Repeat(indent), FmtTimeNice(bspec));
        }
    }
}

static void ListObjectJson(json_PrettyWriter *json, const rk_ObjectInfo &obj)
{
    char buf[128];

    json->Key("type"); json->String(rk_ObjectTypeNames[(int)obj.type]);
    if (obj.readable) {
        json->Key("hash"); json->String(Fmt(buf, "%1", obj.hash).ptr);
    } else {
        json->Key("hash"); json->Null();
    }
    if (obj.name) {
        json->Key("name"); json->String(obj.name);
    } else {
        json->Key("name"); json->Null();
    }

    if (obj.type == rk_ObjectType::Snapshot) {
        json->Key("time"); json->Int64(obj.mtime);
    } else {
        json->Key("mtime"); json->Int64(obj.mtime);
        json->Key("btime"); json->Int64(obj.btime);
        if (obj.type != rk_ObjectType::Link) {
            json->Key("mode"); json->String(Fmt(buf, "0o%1", FmtOctal(obj.mode)).ptr);
        }
        json->Key("uid"); json->Uint(obj.uid);
        json->Key("gid"); json->Uint(obj.gid);
    }

    if (obj.readable) {
        switch (obj.type) {
            case rk_ObjectType::Snapshot:
            case rk_ObjectType::Directory: { json->Key("children"); json->StartArray(); } break;
            case rk_ObjectType::File: { json->Key("size"); json->Int64(obj.size); } break;
            case rk_ObjectType::Link: { json->Key("target"); json->String(obj.target); } break;
            case rk_ObjectType::Unknown: {} break;
        }
    }
}

template <typename T>
pugi::xml_node ListObjectXml(T *ptr, const rk_ObjectInfo &obj)
{
    char buf[128];

    pugi::xml_node element = ptr->append_child(rk_ObjectTypeNames[(int)obj.type]);

    if (obj.readable) {
        element.append_attribute("hash") = Fmt(buf, "%1", obj.hash).ptr;
    } else {
        element.append_attribute("hash") = "";
    }
    element.append_attribute("name") = obj.name ? obj.name : "";

    if (obj.type == rk_ObjectType::Snapshot) {
        element.append_attribute("time") = obj.mtime;
    } else {
        element.append_attribute("mtime") = obj.mtime;
        element.append_attribute("btime") = obj.btime;
        if (obj.type != rk_ObjectType::Link) {
            element.append_attribute("mode") = Fmt(buf, "0o%1", FmtOctal(obj.mode)).ptr;
        }
        element.append_attribute("uid") = obj.uid;
        element.append_attribute("gid") = obj.gid;
    }

    if (obj.readable) {
        switch (obj.type) {
            case rk_ObjectType::Snapshot:
            case rk_ObjectType::Directory: {} break;
            case rk_ObjectType::File: { element.append_attribute("size") = obj.size; } break;
            case rk_ObjectType::Link: { element.append_attribute("target") = obj.target; } break;
            case rk_ObjectType::Unknown: {} break;
        }
    }

    return element;
}

int RunList(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    rk_ListSettings settings;
    OutputFormat format = OutputFormat::Plain;
    int verbose = 0;
    const char *name = nullptr;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 list [-R <repo>] <hash>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password

    %!..+-j, --threads <threads>%!0      Change number of threads
                                 %!D..(default: automatic)%!0

    %!..+-r, --recurse%!0                Show entire tree of children
                                 %!D..(same thing as --depth=All)%!0
        %!..+--depth <depth>%!0          Set maximum recursion depth
                                 %!D..(default: 0)%!0

    %!..+-f, --format <format>%!0        Change output format
                                 %!D..(default: %2)%!0
    %!..+-v, --verbose%!0                Enable verbose output (plain only)

Available output formats: %!..+%3%!0)",
                FelixTarget, OutputFormatNames[(int)format], FmtSpan(OutputFormatNames));
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                config.username = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                config.password = opt.current_value;
            } else if (opt.Test("-j", "--threads", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &config.threads))
                    return 1;
                if (config.threads < 1) {
                    LogError("Threads count cannot be < 1");
                    return 1;
                }
            } else if (opt.Test("-r", "--recurse")) {
                settings.max_depth = -1;
            } else if (opt.Test("--depth", OptionType::Value)) {
                if (TestStr(opt.current_value, "All")) {
                    settings.max_depth = -1;
                } else if (!ParseInt(opt.current_value, &settings.max_depth)) {
                    return 1;
                } else if (settings.max_depth < 0) {
                    LogInfo("Option --depth must be 0 or more (or 'All')");
                    return 1;
                }
            } else if (opt.Test("-f", "--format", OptionType::Value)) {
                if (!OptionToEnumI(OutputFormatNames, opt.current_value, &format)) {
                    LogError("Unknown output format '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-v", "--verbose")) {
                verbose++;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        name = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!name) {
        LogError("No hash provided");
        return 1;
    }

    if (!config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::Full) {
        LogError("You must use the read-write password with this command");
        return 1;
    }
    LogInfo();

    HeapArray<rk_ObjectInfo> objects;
    {
        rk_Hash hash = {};
        if (!rk_ParseHash(name, &hash))
            return 1;
        if (!rk_List(disk.get(), hash, settings, &temp_alloc, &objects))
            return 1;
    }

    switch (format) {
        case OutputFormat::Plain: {
            if (objects.len) {
                for (const rk_ObjectInfo &obj: objects) {
                    ListObjectPlain(obj, 0, verbose);
                }
            } else {
                LogInfo("There does not seem to be any object");
            }
        } break;

        case OutputFormat::JSON: {
            json_PrettyWriter json(&stdout_st);
            int depth = 0;

            json.StartArray();
            for (const rk_ObjectInfo &obj: objects) {
                while (obj.depth < depth) {
                    json.EndArray();
                    json.EndObject();

                    depth--;
                }

                json.StartObject();

                ListObjectJson(&json, obj);

                if (obj.type == rk_ObjectType::Snapshot || obj.type == rk_ObjectType::Directory) {
                    if (obj.children) {
                        depth++;
                        continue;
                    } else {
                        json.EndArray();
                    }
                }

                json.EndObject();
            }
            while (depth--) {
                json.EndArray();
                json.EndObject();
            }
            json.EndArray();

            json.Flush();
            PrintLn();
        } break;

        case OutputFormat::XML: {
            pugi::xml_document doc;
            pugi::xml_node root = doc.append_child("Tree");

            pugi::xml_node ptr = root;
            int depth = 0;

            for (const rk_ObjectInfo &obj: objects) {
                while (obj.depth < depth) {
                    ptr = ptr.parent();
                    depth--;
                }

                pugi::xml_node element = !ptr.empty() ? ListObjectXml(&ptr, obj) : ListObjectXml(&doc, obj);

                if ((obj.type == rk_ObjectType::Snapshot ||
                        obj.type == rk_ObjectType::Directory) && obj.children) {
                    depth++;
                    ptr = element;
                }
            }

            doc.save(std::cout, "    ");
        } break;
    }

    return 0;
}

}
