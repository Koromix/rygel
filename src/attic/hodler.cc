// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "src/core/libcc/libcc.hh"
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
    const char *name;
    const char *filename;
    uint8_t sha256[32];

    RG_HASHTABLE_HANDLER(FileHash, name);
};

struct AssetCopy {
    const char *dest_directory;
    const char *src_directory;
    HeapArray<const char *> ignore;
};

struct AssetBundle {
    const char *name;
    const char *dest_filename;
    const char *src_filename;
    const char *options;
};

struct AssetSet {
    BucketArray<FileHash> hashes;
    HashTable<const char *, const FileHash *> map;
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
    Span<const char> basename = SplitStrReverseAny(section, RG_PATH_SEPARATORS);

    // Strip extension
    SplitStrReverse(basename, '.', &basename);

    const char *name = DuplicateString(basename, alloc).ptr;
    return name;
}

static const char *TextToID(Span<const char> text, char replace_char, Allocator *alloc)
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
                id[len++] = replace_char;
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

    while (len > 1 && id[len - 1] == replace_char) {
        len--;
    }
    if (!len)
        return nullptr;

    id.ptr[len] = 0;

    return id.ptr;
}

static const char *FindEsbuild(const char *path, Allocator *alloc)
{
    if (!path) {
        const char *str = getenv("ESBUILD_PATH");
        path = (str && str[0]) ? str : ".";
    }

    FileInfo file_info;
    StatResult stat = StatFile(path, (int)StatFlag::IgnoreMissing, &file_info);

    if (stat == StatResult::MissingPath) {
        goto missing;
    } else if (stat != StatResult::Success) {
        return nullptr;
    }

    if (file_info.type == FileType::Directory) {
#if defined(_WIN64)
        const char *binary = Fmt(alloc, "%1%/esbuild_windows_x64.exe", path).ptr;
#elif defined(__linux__) && defined(__x86_64__)
        const char *binary = Fmt(alloc, "%1%/esbuild_linux_x64", path).ptr;
#elif defined(__linux__) && defined(__aarch64__)
        const char *binary = Fmt(alloc, "%1%/esbuild_linux_arm64", path).ptr;
#elif defined(__APPLE__) && defined(__x86_64__)
        const char *binary = Fmt(alloc, "%1%/esbuild_macos_x64", path).ptr;
#elif defined(__APPLE__) && defined(__aarch64__)
        const char *binary = Fmt(alloc, "%1%/esbuild_macos_arm64", path).ptr;
#else
        const char *binary = nullptr;
#endif

        if (!binary || !TestFile(binary))
            goto missing;

        path = binary;
    }

    return path;

missing:
    LogError("Cannot find esbuild, please set ESBUILD_PATH");
    return nullptr;
}

static bool BundleScript(const AssetBundle &bundle, const char *esbuild_binary, uint8_t out_hash[32])
{
    char cmd[4096];

    // Prepare command
    if (bundle.options) {
        Fmt(cmd, "\"%1\" \"%2\" --bundle --log-level=warning --allow-overwrite --outfile=\"%3\""
                 "  --minify --platform=browser %4",
            esbuild_binary, bundle.src_filename, bundle.dest_filename, bundle.options);
    } else {
        Fmt(cmd, "\"%1\" \"%2\" --bundle --log-level=warning --allow-overwrite --outfile=\"%3\""
                 "  --minify --platform=browser",
            esbuild_binary, bundle.src_filename, bundle.dest_filename);
    }

    // Run esbuild
    {
        HeapArray<char> output_buf;
        int exit_code;
        bool started = ExecuteCommandLine(cmd, {}, {}, Megabytes(4), &output_buf, &exit_code);

        if (!started) {
            return false;
        } else if (exit_code) {
            LogError("Failed to run esbuild %!..+(exit code %1)%!0", exit_code);
            stderr_st.Write(output_buf);

            return false;
        }
    }

    // Compute destination hash
    {
        StreamReader reader(bundle.dest_filename);

        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);

        do {
            LocalArray<uint8_t, 16384> buf;
            buf.len = reader.Read(buf.data);
            if (buf.len < 0)
                return false;

            crypto_hash_sha256_update(&state, buf.data, buf.len);
        } while (!reader.IsEOF());

        crypto_hash_sha256_final(&state, out_hash);
    }

    return true;
}

static void RenderAsset(Span<const char> path, const FileHash *hash, StreamWriter *writer)
{
    if (hash) {
        FmtArg suffix = FmtSpan(MakeSpan(hash->sha256, 8), FmtType::BigHex, "").Pad0(-2);
        Print(writer, "/%1?%2", path, suffix);
    } else {
        LogWarning("Unknown asset '%1'", path);
        Print(writer, "/%1", path);
    }
}

// XXX: Resolve page links in content
static bool RenderMarkdown(PageData *page, const AssetSet &assets, Allocator *alloc)
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
            "autolink",
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
                const FileHash *hash = assets.map.FindValue(path, nullptr);

                RenderAsset(path, hash, writer);
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

                    const char *literal = cmark_node_get_literal(child);

                    Span<const char> toc;
                    Span<const char> title = SplitStr(literal, '^', &toc);

                    if (toc.len) {
                        toc = DuplicateString(toc, alloc);
                        title = DuplicateString(title, alloc);

                        cmark_node_set_literal(child, title.ptr);
                    } else {
                        toc = DuplicateString(title, alloc);
                    }

                    sec.level = level;
                    sec.title = toc.ptr;
                    sec.id = TextToID(title, '-', alloc);

                    page->sections.Append(sec);

                    cmark_node *frag = cmark_node_new(CMARK_NODE_HTML_INLINE);
                    if (strchr(sec.id, '-')) {
                        const char *old_id = TextToID(title, '_', alloc);
                        cmark_node_set_literal(frag, Fmt(alloc, "<a id=\"%1\"></a><a id=\"%2\"></a>", sec.id, old_id).ptr);
                    } else {
                        cmark_node_set_literal(frag, Fmt(alloc, "<a id=\"%1\"></a>", sec.id).ptr);
                    }
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

static Size RenderMenu(Span<const PageData> pages, Size active_idx,
                       Size idx, Size end, int depth, StreamWriter *writer)
{
    const PageData *page = &pages[idx];

    if (!page->menu) {
        RG_ASSERT(!depth);
        return idx + 1;
    }

    Span<const char> category = {};
    Span<const char> title = page->menu;

    for (Size i = 0; i <= depth; i++) {
        Span<const char> remain = title;
        Span<const char> frag = TrimStr(SplitStr(remain, '/', &remain));

        if (!remain.len) {
            category = {};
            break;
        }

        category = frag;
        title = remain;
    }
    title = TrimStr(title);

    Print(writer, "%1", depth ? "" : "<li>");

    if (category.len) {
        Size i = idx;
        Size j = i + 1;

        while (j < end) {
            Span<const char> remain = pages[j].menu;
            Span<const char> new_category = {};

            for (int k = 0; k <= depth; k++) {
                new_category = TrimStr(SplitStr(remain, '/', &remain));
            }

            if (new_category != category)
                break;
            j++;
        }

        bool active = (active_idx >= i && active_idx < j);
        int margin = std::max(0, depth - 1);

        Print(writer, "<a href=\"#\" class=\"category%1\" style=\"margin-left: %2em;\">%3</a>", active ? " active" : "", margin, category);
        PrintLn(writer, "%1", depth ? "" : "<div>");
        while (i < j) {
            i = RenderMenu(pages, active_idx, i, j, depth + 1, writer);
        }
        PrintLn(writer, "%1", depth ? "" : "</div></li>");

        return j;
    } else {
        bool active = (active_idx == idx);
        int margin = std::max(0, depth - 1);

        Print(writer, "<a href=\"%1\"%2 style=\"margin-left: %3em;\">%4</a>", page->url, active ? " class=\"active\"" : "", margin, title);
        PrintLn(writer, "%1", depth ? "" : "</li>");

        return idx + 1;
    }
}

static bool RenderTemplate(const char *template_filename, Span<const PageData> pages, Size page_idx,
                           const AssetSet &assets, const char *dest_filename)
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
            const FileHash *hash = assets.map.FindValue(path, nullptr);

            RenderAsset(path, hash, writer);
        } else if (key == "LINKS") {
            for (Size i = 0; i < pages.len;) {
                i = RenderMenu(pages, page_idx, i, pages.len, 0, writer);
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

static bool ShouldCompressFile(const char *filename)
{
    const char *mimetype = GetMimeType(GetPathExtension(filename));
    return StartsWith(mimetype, "text/");
}

static bool BuildAll(Span<const char> source_dir, UrlFormat urls, const char *output_dir, bool gzip)
{
    BlockAllocator temp_alloc;

    // Output directory
    if (!MakeDirectory(output_dir, false))
        return false;
    LogInfo("Source directory: %!..+%1%!0", source_dir);
    LogInfo("Output directory: %!..+%1%!0", output_dir);

    const char *pages_filename = Fmt(&temp_alloc, "%1%/pages.ini", source_dir).ptr;
    const char *assets_filename = Fmt(&temp_alloc, "%1%/assets.ini", source_dir).ptr;

    // List pages
    HeapArray<PageData> pages;
    {
        StreamReader st(pages_filename);
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
            page.src_filename = NormalizePath(prop.section, source_dir, &temp_alloc).ptr;
            page.description = "";

            do {
                if (prop.key == "Title") {
                    page.title = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "Menu") {
                    page.menu = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "Description") {
                    page.description = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "Template") {
                    page.template_filename = NormalizePath(prop.value, source_dir, &temp_alloc).ptr;
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

    // List asset settings and rules
    const char *esbuild_path = nullptr;
    HeapArray<AssetCopy> copies;
    HeapArray<AssetBundle> bundles;
    if (TestFile(assets_filename)) {
        StreamReader st(assets_filename);
        if (!st.IsValid())
            return false;

        IniParser ini(&st);
        ini.PushLogFilter();
        RG_DEFER { PopLogFilter(); };

        bool valid = true;

        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                if (prop.key == "EsbuildPath") {
                    esbuild_path = NormalizePath(prop.value, source_dir, &temp_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else {
                // Type property must be specified first
                if (prop.key != "Type") {
                    LogError("Property 'Type' must be specified first");
                    valid = false;

                    while (ini.NextInSection(&prop));
                    continue;
                }

                if (prop.value == "Copy") {
                    AssetCopy *copy = copies.AppendDefault();

                    copy->dest_directory = NormalizePath(prop.section, &temp_alloc).ptr;

                    while (ini.NextInSection(&prop)) {
                        if (prop.key == "From") {
                            copy->src_directory = NormalizePath(prop.value, source_dir, &temp_alloc).ptr;
                        } else if (prop.key == "Ignore") {
                            while (prop.value.len) {
                                Span<const char> part = TrimStr(SplitStrAny(prop.value, " ,", &prop.value));

                                if (part.len) {
                                    const char *pattern = DuplicateString(part, &temp_alloc).ptr;
                                    copy->ignore.Append(pattern);
                                }
                            }
                        } else {
                            LogError("Unknown attribute '%1'", prop.key);
                            valid = false;
                        }
                    }

                    if (!copy->src_directory) {
                        LogError("Missing copy source directory");
                        valid = false;
                    }
                } else if (prop.value == "Bundle") {
                    AssetBundle *bundle = bundles.AppendDefault();

                    bundle->name = DuplicateString(prop.section, &temp_alloc).ptr;
                    bundle->dest_filename = NormalizePath(prop.section, output_dir, &temp_alloc).ptr;

                    while (ini.NextInSection(&prop)) {
                        if (prop.key == "Source") {
                            bundle->src_filename = NormalizePath(prop.value, source_dir, &temp_alloc).ptr;
                        } else if (prop.key == "Options") {
                            bundle->options = DuplicateString(prop.value, &temp_alloc).ptr;
                        } else {
                            LogError("Unknown attribute '%1'", prop.key);
                            valid = false;
                        }
                    }

                    if (!bundle->src_filename) {
                        LogError("Missing bundle source");
                        valid = false;
                    }
                } else {
                    LogError("Unknown asset rule type '%1'", prop.value);
                    valid = false;

                    while (ini.NextInSection(&prop));
                }
            }
        }
        if (!ini.IsValid() || !valid)
            return false;
    }
    if (!copies.len) {
        AssetCopy copy = {};

        copy.dest_directory = ".";
        copy.src_directory = Fmt(&temp_alloc, "%1%/assets", source_dir).ptr;

        copies.Append(copy);
    }

    // Normalize settings
    if (bundles.len) {
        esbuild_path = FindEsbuild(esbuild_path, &temp_alloc);
        if (!esbuild_path)
            return false;
    }

    AssetSet assets;

    // Copy static assets
    for (const AssetCopy &copy: copies) {
        Async async;

        HeapArray<const char *> src_filenames;
        if (!EnumerateFiles(copy.src_directory, nullptr, 3, 1024, &temp_alloc, &src_filenames))
            return false;

        // Remove ignored patterns
        src_filenames.RemoveFrom(std::remove_if(src_filenames.begin(), src_filenames.end(),
                                                 [&](const char *filename) {
            return std::any_of(copy.ignore.begin(), copy.ignore.end(),
                               [&](const char *pattern) { return MatchPathSpec(filename, pattern); });
        }) - src_filenames.begin());

        Size prefix_len = strlen(copy.src_directory);

        for (const char *src_filename: src_filenames) {
            const char *basename = TrimStrLeft(src_filename + prefix_len, RG_PATH_SEPARATORS).ptr;

            const char *url = NormalizePath(basename, copy.dest_directory, &temp_alloc).ptr;
            const char *dest_filename = Fmt(&temp_alloc, "%1%/%2", output_dir, url).ptr;
            const char *gzip_filename = Fmt(&temp_alloc, "%1.gz", dest_filename).ptr;

            FileHash *hash = assets.hashes.AppendDefault();

            hash->name = url;
            hash->filename = dest_filename;

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
                if (gzip && ShouldCompressFile(dest_filename)) {
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

            assets.map.Set(hash);
        }

        if (!async.Sync())
            return false;
    }

    // Bundle JS files
    {
        Async async;

        for (const AssetBundle &bundle: bundles) {
            FileHash *hash = assets.hashes.AppendDefault();

            hash->name = bundle.name;
            hash->filename = bundle.dest_filename;

            async.Run([=] { return BundleScript(bundle, esbuild_path, hash->sha256); });

            assets.map.Set(hash);
        }

        if (!async.Sync())
            return false;
    }

    // Render markdown
    for (PageData &page: pages) {
        if (!RenderMarkdown(&page, assets, &temp_alloc))
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

            async.Run([=, &pages, &assets]() {
                if (!RenderTemplate(pages[i].template_filename, pages, i, assets, dest_filename))
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
    const char *source_dir = ".";
    const char *output_dir = nullptr;
    bool gzip = false;
    UrlFormat urls = UrlFormat::Pretty;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options] -O <output_dir>%!0

Options:
    %!..+-S, --source_dir <file>%!0      Set source directory
                                 %!D..(default: %2)%!0

    %!..+-O, --output_dir <dir>%!0       Set output directory
        %!..+--gzip%!0                   Create static gzip files

    %!..+-u, --urls <FORMAT>%!0          Change URL format (%3)
                                 %!D..(default: %4)%!0)",
                FelixTarget, source_dir, FmtSpan(UrlFormatNames), UrlFormatNames[(int)urls]);
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
            } else if (opt.Test("-S", "--source_dir", OptionType::Value)) {
                source_dir = opt.current_value;
            } else if (opt.Test("-O", "--output_dir", OptionType::Value)) {
                output_dir = opt.current_value;
            } else if (opt.Test("--gzip")) {
                gzip = true;
            } else if (opt.Test("-u", "--urls", OptionType::Value)) {
                if (!OptionToEnumI(UrlFormatNames, opt.current_value, &urls)) {
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

    if (!BuildAll(source_dir, urls, output_dir, gzip))
        return 1;

    LogInfo("Done!");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
