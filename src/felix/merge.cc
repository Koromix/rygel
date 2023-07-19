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
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "merge.hh"

namespace RG {

enum class MergeMode {
    Naive,
    CSS,
    JS
};

struct MergeRule {
    const char *name;

    HeapArray<const char *> sources;

    bool override_compression;
    CompressionType compression_type;

    MergeMode merge_mode;
};

static const char *StripDirectoryComponents(Span<const char> filename, int strip_count)
{
    const char *name = filename.ptr;
    for (int i = 0; filename.len && i <= strip_count; i++) {
        name = SplitStrAny(filename, RG_PATH_SEPARATORS, &filename).ptr;
    }

    return name;
}

static MergeMode FindDefaultMergeMode(const char *filename)
{
    Span<const char> extension = GetPathExtension(filename);

    if (extension == ".css") {
        return MergeMode::CSS;
    } else if (extension == ".js") {
        return MergeMode::JS;
    } else {
        return MergeMode::Naive;
    }
}

static bool LoadMergeRules(const char *filename, BlockAllocator *alloc, HeapArray<MergeRule> *out_rules)
{
    RG_DEFER_NC(out_guard, len = out_rules->len) { out_rules->RemoveFrom(len); };

    StreamReader st(filename);
    if (!st.IsValid())
        return false;

    IniParser ini(&st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            MergeRule *rule = out_rules->AppendDefault();
            rule->name = DuplicateString(prop.section, alloc).ptr;
            rule->merge_mode = FindDefaultMergeMode(rule->name);

            do {
                if (prop.key == "CompressionType") {
                    if (OptionToEnum(CompressionTypeNames, prop.value, &rule->compression_type)) {
                        rule->override_compression = true;
                    } else {
                        LogError("Unknown compression type '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "MergeMode") {
                    if (prop.value == "Naive") {
                        rule->merge_mode = MergeMode::Naive;
                    } else if (prop.value == "CSS") {
                        rule->merge_mode = MergeMode::CSS;
                    } else if (prop.value == "JS") {
                        rule->merge_mode = MergeMode::JS;
                    } else {
                        LogError("Invalid MergeMode value '%1'", prop.value);
                        valid = false;
                    }
                } else if (prop.key == "File") {
                    while (prop.value.len) {
                        Span<const char> part = TrimStr(SplitStrAny(prop.value, " ,", &prop.value));

                        if (part.len) {
                            const char *copy = DuplicateString(part, alloc).ptr;
                            rule->sources.Append(copy);
                        }
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    out_guard.Disable();
    return true;
}

static void InitSourceMergeData(PackSource *src, MergeMode merge_mode, Allocator *alloc)
{
    RG_ASSERT(alloc);

    switch (merge_mode) {
        case MergeMode::Naive: {
            src->prefix = "";
            src->suffix = "";
        } break;
        case MergeMode::CSS: {
            src->prefix = Fmt(alloc, "/* %1\n   ------------------------------------ */\n\n", src->filename).ptr;
            src->suffix = "\n";
        } break;
        case MergeMode::JS: {
            src->prefix = Fmt(alloc, "// %1\n// ------------------------------------\n\n", src->filename).ptr;
            src->suffix = "\n";
        } break;
    }
}

bool ResolveAssets(Span<const char *const> filenames, int strip_count,
                   CompressionType compression_type, PackAssetSet *out_set)
{
    // Reuse for performance
    HeapArray<MergeRule> rules;

    for (const char *filename: filenames) {
        if (StartsWith(filename, "@")) {
            rules.RemoveFrom(0);

            if (!LoadMergeRules(filename + 1, &out_set->str_alloc, &rules))
                return false;

            for (const MergeRule &rule: rules) {
                PackAsset *asset = out_set->assets.AppendDefault();

                asset->name = StripDirectoryComponents(rule.name, strip_count);
                asset->compression_type = rule.override_compression ? rule.compression_type : compression_type;

                for (const char *src_filename: rule.sources) {
                    PackSource *src = asset->sources.AppendDefault();

                    src->filename = src_filename;
                    InitSourceMergeData(src, rule.merge_mode, &out_set->str_alloc);
                }
            }
        } else {
            PackAsset *asset = out_set->assets.AppendDefault();
            PackSource *src = asset->sources.AppendDefault();

            asset->name = StripDirectoryComponents(filename, strip_count);
            asset->compression_type = compression_type;

            src->filename = filename;
        }
    }

    return true;
}

}
