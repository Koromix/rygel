// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "src/core/libcc/libcc.hh"
#include "src/core/libnet/libnet.hh"
extern "C" {
    #include "vendor/cmark-gfm/src/cmark-gfm.h"
    #include "vendor/cmark-gfm/extensions/cmark-gfm-core-extensions.h"
}
#include "vendor/libsodium/src/libsodium/include/sodium/crypto_hash_sha256.h"

namespace RG {

enum class UrlFormat {
    Pretty,
    PrettySub,
    Ugly
};
static const char *const UrlFormatNames[] = {
    "Pretty",
    "PrettySub",
    "Ugly"
};

struct FileHash {
    const char *path;
    uint8_t sha256[32];

    RG_HASHTABLE_HANDLER(FileHash, path);
};

struct PageSection {
    const char *id;
    const char *title;
    int level = 0;
};

struct PageData {
    const char *name;

    const char *src_filename;
    const char *template_filename;
    const char *title;
    const char *menu;
    const char *description;

    const char *url;

    HeapArray<PageSection> sections;
    std::shared_ptr<const char> html_buf;
    Span<const char> html;
};

static int32_t DecodeUtf8Unsafe(const char *str);

static const HashMap<int32_t, const char *> replacements = {
    { DecodeUtf8Unsafe("Ç"), "c" },
    { DecodeUtf8Unsafe("È"), "e" },
    { DecodeUtf8Unsafe("É"), "e" },
    { DecodeUtf8Unsafe("Ê"), "e" },
    { DecodeUtf8Unsafe("Ë"), "e" },
    { DecodeUtf8Unsafe("À"), "a" },
    { DecodeUtf8Unsafe("Å"), "a" },
    { DecodeUtf8Unsafe("Â"), "a" },
    { DecodeUtf8Unsafe("Ä"), "a" },
    { DecodeUtf8Unsafe("Î"), "i" },
    { DecodeUtf8Unsafe("Ï"), "i" },
    { DecodeUtf8Unsafe("Ù"), "u" },
    { DecodeUtf8Unsafe("Ü"), "u" },
    { DecodeUtf8Unsafe("Û"), "u" },
    { DecodeUtf8Unsafe("Ú"), "u" },
    { DecodeUtf8Unsafe("Ñ"), "n" },
    { DecodeUtf8Unsafe("Ô"), "o" },
    { DecodeUtf8Unsafe("Ó"), "o" },
    { DecodeUtf8Unsafe("Ö"), "o" },
    { DecodeUtf8Unsafe("Œ"), "oe" },
    { DecodeUtf8Unsafe("Ÿ"), "y" },

    { DecodeUtf8Unsafe("ç"), "c" },
    { DecodeUtf8Unsafe("è"), "e" },
    { DecodeUtf8Unsafe("é"), "e" },
    { DecodeUtf8Unsafe("ê"), "e" },
    { DecodeUtf8Unsafe("ë"), "e" },
    { DecodeUtf8Unsafe("à"), "a" },
    { DecodeUtf8Unsafe("å"), "a" },
    { DecodeUtf8Unsafe("â"), "a" },
    { DecodeUtf8Unsafe("ä"), "a" },
    { DecodeUtf8Unsafe("î"), "i" },
    { DecodeUtf8Unsafe("ï"), "i" },
    { DecodeUtf8Unsafe("ù"), "u" },
    { DecodeUtf8Unsafe("ü"), "u" },
    { DecodeUtf8Unsafe("û"), "u" },
    { DecodeUtf8Unsafe("ú"), "u" },
    { DecodeUtf8Unsafe("ñ"), "n" },
    { DecodeUtf8Unsafe("ô"), "o" },
    { DecodeUtf8Unsafe("ó"), "o" },
    { DecodeUtf8Unsafe("ö"), "o" },
    { DecodeUtf8Unsafe("œ"), "oe" },
    { DecodeUtf8Unsafe("ÿ"), "y" }
};

static int32_t DecodeUtf8Unsafe(const char *str)
{
    int32_t uc = -1;
    Size bytes = DecodeUtf8(str, &uc);

    RG_ASSERT(bytes > 0);
    RG_ASSERT(!str[bytes]);

    return uc;
}

static const char *SectionToPageName(Span<const char> section, Allocator *alloc)
{
    // File name and extension
    SplitStrReverse(section, '.', &section);

    const char *name = DuplicateString(section, alloc).ptr;
    return name;
}

static const char *TextToID(Span<const char> text, Allocator *alloc)
{
    Span<char> id = AllocateSpan<char>(alloc, text.len + 1);

    Size offset = 0;
    Size len = 0;
    bool skip_special = false;

    while (offset < text.len) {
        int32_t uc;
        Size bytes = DecodeUtf8(text, offset, &uc);

        if (bytes == 1) {
            if (IsAsciiAlphaOrDigit((char)uc)) {
                id[len++] = LowerAscii((char)uc);
                skip_special = false;
            } else if (!skip_special) {
                id[len++] = '_';
                skip_special = true;
            }
        } else if (bytes > 1) {
            const char *ptr = replacements.FindValue(uc, nullptr);
            Size expand = bytes;

            if (ptr) {
                expand = strlen(ptr);
            } else {
                ptr = text.ptr + offset;
            }

            memcpy_safe(id.ptr + len, ptr, (size_t)expand);
            len += expand;

            skip_special = false;
        } else {
            LogError("Illegal UTF-8 sequence");
            return nullptr;
        }

        offset += bytes;
    }

    while (len > 1 && id[len - 1] == '_') {
        len--;
    }
    if (!len)
        return nullptr;

    id.ptr[len] = 0;

    return id.ptr;
}

static bool SpliceWithChecksum(StreamReader *reader, StreamWriter *writer, uint8_t out_hash[32])
{
    if (!reader->IsValid())
        return false;

    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);

    do {
        LocalArray<uint8_t, 16384> buf;
        buf.len = reader->Read(buf.data);
        if (buf.len < 0)
            return false;

        if (!writer->Write(buf))
            return false;
        crypto_hash_sha256_update(&state, buf.data, buf.len);
    } while (!reader->IsEOF());

    if (!writer->Close())
        return false;
    crypto_hash_sha256_final(&state, out_hash);

    return true;
}

// XXX: Resolve page links in content
static bool RenderMarkdown(PageData *page, const HashTable<const char *, const FileHash *> &assets, Allocator *alloc)
{
    HeapArray<char> content;
    if (ReadFile(page->src_filename, Mebibytes(8), &content) < 0)
        return false;
    Span<const char> remain = TrimStr(content.As());

    cmark_gfm_core_extensions_ensure_registered();

    // Prepare markdown parser
    cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    RG_DEFER { cmark_parser_free(parser); };

    // Enable syntax extensions
    {
        static const char *const extensions[] = {
            "table",
            "strikethrough"
        };

        for (const char *name: extensions) {
            cmark_syntax_extension *ext = cmark_find_syntax_extension(name);

            if (!ext) {
                LogError("Cannot find Markdown extension '%1'", name);
                return false;
            }
            if (!cmark_parser_attach_syntax_extension(parser, ext)) {
                LogError("Failed to enable Markdown extension '%1'", name);
                return false;
            }
        }
    }

    // Parse markdown
    {
        const auto write = [&](Span<const uint8_t> buf) {
            cmark_parser_feed(parser, (const char *)buf.ptr, buf.len);
            return true;
        };

        StreamWriter writer(write, "<buffer>");

        bool success = PatchFile(remain.As<const uint8_t>(), &writer,
                                 [&](Span<const char> expr, StreamWriter *writer) {
            Span<const char> key = TrimStr(expr);

            if (key == "RANDOM") {
                Print(writer, "%1", FmtRandom(8));
            } else if (StartsWith(key, "ASSET ")) {
                Span<const char> path = TrimStr(key.Take(6, key.len - 6));
                const FileHash *hash = assets.FindValue(path, nullptr);

                if (hash) {
                    FmtArg suffix = FmtSpan(MakeSpan(hash->sha256, 8), FmtType::BigHex, "").Pad0(-2);
                    Print(writer, "/static/%1?%2", path, suffix);
                } else {
                    Print(writer, "/static/%1", path);
                }
            } else {
                Print(writer, "{{%1}}", expr);
            }
        });

        if (!success)
            return false;
        if (!writer.Close())
            return false;
    }

    // Finalize parsing
    cmark_node *root = cmark_parser_finish(parser);
    RG_DEFER { cmark_node_free(root); };

    // Customize rendered tree
    {
        cmark_iter *iter = cmark_iter_new(root);
        RG_DEFER { cmark_iter_free(iter); };

        cmark_event_type event;
        while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
            cmark_node *node = cmark_iter_get_node(iter);
            cmark_node_type type = cmark_node_get_type(node);

            // List sections and add anchors
            if (event == CMARK_EVENT_EXIT && type == CMARK_NODE_HEADING) {
                int level = cmark_node_get_heading_level(node);
                cmark_node *child = cmark_node_first_child(node);

                if (level < 3 && cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                    PageSection sec = {};

                    sec.level = level;
                    sec.title = DuplicateString(cmark_node_get_literal(child), alloc).ptr;
                    sec.id = TextToID(sec.title, alloc);

                    page->sections.Append(sec);

                    cmark_node *frag = cmark_node_new(CMARK_NODE_HTML_INLINE);
                    cmark_node_set_literal(frag, Fmt(alloc, "<a id=\"%1\"></a>", sec.id).ptr);
                    cmark_node_prepend_child(node, frag);
                }
            }
        }
    }

    // Render to HTML
    page->html = cmark_render_html(root, CMARK_OPT_UNSAFE, nullptr);
    page->html_buf.reset((char *)page->html.ptr, free);

    return true;
}

static bool RenderTemplate(const char *template_filename, Span<const PageData> pages, Size page_idx,
                           const HashTable<const char *, const FileHash *> &assets,
                           const char *dest_filename)
{
    StreamReader reader(template_filename);
    StreamWriter writer(dest_filename, (int)StreamWriterFlag::Atomic);

    const PageData &page = pages[page_idx];

    bool success = PatchFile(&reader, &writer, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(page.title);
        } else if (key == "DESCRIPTION") {
            writer->Write(page.description);
        } else if (key == "RANDOM") {
            Print(writer, "%1", FmtRandom(8));
        } else if (StartsWith(key, "ASSET ")) {
            Span<const char> path = TrimStr(key.Take(6, key.len - 6));
            const FileHash *hash = assets.FindValue(path, nullptr);

            if (hash) {
                FmtArg suffix = FmtSpan(MakeSpan(hash->sha256, 8), FmtType::BigHex, "").Pad0(-2);
                Print(writer, "/static/%1?%2", path, suffix);
            } else {
                Print(writer, "/static/%1", path);
            }
        } else if (key == "LINKS") {
            for (Size i = 0; i < pages.len; i++) {
                const PageData *menu_page = &pages[i];

                if (!menu_page->menu)
                    continue;

                if (strchr(menu_page->menu, '/')) {
                    Span<const char> category = TrimStr(SplitStr(menu_page->menu, '/'));

                    Size j = i + 1;
                    while (j < pages.len) {
                        Span<const char> new_category = TrimStr(SplitStr(pages[j].menu, '/'));

                        if (new_category != category)
                            break;
                        j++;
                    }

                    bool active = (page_idx >= i && page_idx < j);
                    PrintLn(writer, "<li><a href=\"#\"%1>%2</a><div>", active ? " class=\"active\"" : "", category);

                    for (; i < j; i++) {
                        menu_page = &pages[i];

                        const char *item = TrimStrLeft(strchr(menu_page->menu, '/') + 1).ptr;
                        SplitStr(menu_page->menu, '/', &item);

                        bool active = (page_idx == i);
                        PrintLn(writer, "<a href=\"%1\"%2>%3</a>", menu_page->url, active ? " class=\"active\"" : "", item);
                    }
                    i = j - 1;

                    PrintLn(writer, "</div></li>");
                } else {
                    bool active = (page_idx == i);
                    PrintLn(writer, "<li><a href=\"%1\"%2>%3</a></li>", menu_page->url, active ? " class=\"active\"" : "", menu_page->menu);
                }
            }
        } else if (key == "TOC") {
            if (page.sections.len > 1) {
                PrintLn(writer, "<nav id=\"side\"><menu>");

                for (const PageSection &sec: page.sections) {
                    PrintLn(writer, "<li><a href=\"#%1\" class=\"lv%2\">%3</a></li>",
                                    sec.id, sec.level, sec.title);
                }

                PrintLn(writer, "</menu></nav>");
            }
        } else if (key == "CONTENT") {
            writer->Write(page.html);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    if (!success)
        return false;
    if (!writer.Close())
        return false;

    return true;
}

static bool BuildAll(const char *config_filename, UrlFormat urls, const char *output_dir, bool gzip)
{
    BlockAllocator temp_alloc;

    // Output directory
    if (!MakeDirectory(output_dir, false))
        return false;
    LogInfo("Configuration file: %!..+%1%!0", config_filename);
    LogInfo("Output directory: %!..+%1%!0", output_dir);

    Span<const char> config_dir = GetPathDirectory(config_filename);
    const char *asset_dir = Fmt(&temp_alloc, "%1%/assets", config_dir).ptr;

    // List pages
    HeapArray<PageData> pages;
    {
        StreamReader st(config_filename);
        if (!st.IsValid())
            return false;

        IniParser ini(&st);
        ini.PushLogFilter();
        RG_DEFER { PopLogFilter(); };

        bool valid = true;

        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            PageData page = {};

            page.name = SectionToPageName(prop.section, &temp_alloc);
            page.src_filename = NormalizePath(prop.section, config_dir, &temp_alloc).ptr;
            page.description = "";

            do {
                if (prop.key == "Title") {
                    page.title = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "Menu") {
                    page.menu = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "Description") {
                    page.description = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "Template") {
                    page.template_filename = NormalizePath(prop.value, config_dir, &temp_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (!page.title) {
                LogError("Missing title for page '%1'", SplitStrReverseAny(page.src_filename, RG_PATH_SEPARATORS));
                valid = false;
            }
            if (!page.menu) {
                LogError("Missing menu for page '%1'", SplitStrReverseAny(page.src_filename, RG_PATH_SEPARATORS));
                valid = false;
            }
            if (!page.template_filename) {
                LogError("Missing template for page '%1'", SplitStrReverseAny(page.src_filename, RG_PATH_SEPARATORS));
                valid = false;
            }

            if (TestStr(page.name, "index")) {
                page.url = "/";
            } else {
                switch (urls) {
                    case UrlFormat::Pretty:
                    case UrlFormat::PrettySub: { page.url = Fmt(&temp_alloc, "/%1", page.name).ptr; } break;
                    case UrlFormat::Ugly: { page.url = Fmt(&temp_alloc, "/%1.html", page.name).ptr; } break;
                }
            }

            pages.Append(page);
        }
        if (!ini.IsValid() || !valid)
            return false;
    }

    // Copy static assets
    BucketArray<FileHash> hashes;
    HashTable<const char *, const FileHash *> hashes_map;
    if (TestFile(asset_dir, FileType::Directory)) {
        Async async;

        HeapArray<const char *> asset_filenames;
        if (!EnumerateFiles(asset_dir, nullptr, 3, 1024, &temp_alloc, &asset_filenames))
            return false;

        Size prefix_len = strlen(asset_dir);

        for (const char *src_filename: asset_filenames) {
            const char *basename = TrimStrLeft(src_filename + prefix_len, RG_PATH_SEPARATORS).ptr;

            const char *dest_filename = Fmt(&temp_alloc, "%1%/%2", output_dir, basename).ptr;
            const char *gzip_filename = Fmt(&temp_alloc, "%1.gz", dest_filename).ptr;

            FileHash *hash = hashes.AppendDefault();
            hash->path = basename;

            async.Run([=]() {
                if (!EnsureDirectoryExists(dest_filename))
                    return false;

                // Open ahead of time because src_filename won't stay valid
                StreamReader reader(src_filename);

                // Copy raw file
                {
                    StreamWriter writer(dest_filename, (int)StreamWriterFlag::Atomic);

                    if (!SpliceWithChecksum(&reader, &writer, hash->sha256))
                        return false;
                    if (!writer.Close())
                        return false;
                }

                // Create gzipped version
                if (gzip && http_ShouldCompressFile(dest_filename)) {
                    reader.Rewind();

                    StreamWriter writer(gzip_filename, (int)StreamWriterFlag::Atomic, CompressionType::Gzip);

                    if (!SpliceStream(&reader, -1, &writer))
                        return false;
                    if (!writer.Close())
                        return false;
                } else {
                    UnlinkFile(gzip_filename);
                }

                return true;
            });
        }

        if (!async.Sync())
            return false;

        for (const FileHash &hash: hashes) {
            hashes_map.Set(&hash);
        }
    }

    // Render markdown
    for (PageData &page: pages) {
        if (!RenderMarkdown(&page, hashes_map, &temp_alloc))
            return false;
    }

    // Render templates
    {
        Async async;

        for (Size i = 0; i < pages.len; i++) {
            Span<const char> ext = GetPathExtension(pages[i].template_filename);

            const char *dest_filename;
            if (urls == UrlFormat::PrettySub && !TestStr(pages[i].name, "index")) {
                dest_filename = Fmt(&temp_alloc, "%1%/%2%/index%3", output_dir, pages[i].name, ext).ptr;
                if (!EnsureDirectoryExists(dest_filename))
                    return false;
            } else {
                dest_filename = Fmt(&temp_alloc, "%1%/%2%3", output_dir, pages[i].name, ext).ptr;
            }

            bool gzip_file = gzip && TestStr(ext, ".html");
            const char *gzip_filename = Fmt(&temp_alloc, "%1.gz", dest_filename).ptr;

            async.Run([=, &pages]() {
                if (!RenderTemplate(pages[i].template_filename, pages, i, hashes_map, dest_filename))
                    return false;

                if (gzip_file) {
                    StreamReader reader(dest_filename);
                    StreamWriter writer(gzip_filename, (int)StreamWriterFlag::Atomic, CompressionType::Gzip);

                    if (!SpliceStream(&reader, Megabytes(4), &writer))
                        return false;
                    if (!writer.Close())
                        return false;
                } else {
                    UnlinkFile(gzip_filename);
                }

                return true;
            });
        }

        if (!async.Sync())
            return false;
    }

    return true;
}

int Main(int argc, char *argv[])
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "HodlerSite.ini";
    const char *output_dir = nullptr;
    bool gzip = false;
    UrlFormat urls = UrlFormat::Pretty;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options] -O <output_dir>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration filename
                                 %!D..(default: %2)%!0

    %!..+-O, --output_dir <dir>%!0       Set output directory
        %!..+--gzip%!0                   Create static gzip files

    %!..+-u, --urls <FORMAT>%!0          Change URL format (%3)
                                 %!D..(default: %4)%!0)",
                FelixTarget, config_filename, FmtSpan(UrlFormatNames), UrlFormatNames[(int)urls]);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Parse options
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.Test("-O", "--output_dir", OptionType::Value)) {
                output_dir = opt.current_value;
            } else if (opt.Test("--gzip")) {
                gzip = true;
            } else if (opt.Test("-u", "--urls", OptionType::Value)) {
                if (!OptionToEnum(UrlFormatNames, opt.current_value, &urls)) {
                    LogError("Unknown URL format '%1'", opt.current_value);
                    return true;
                }
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    if (!output_dir) {
        LogError("Missing output directory");
        return 1;
    }

    if (!BuildAll(config_filename, urls, output_dir, gzip))
        return 1;

    LogInfo("Done!");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
