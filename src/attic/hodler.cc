// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "src/core/wrap/json.hh"
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

struct BuildSettings {
    UrlFormat urls = UrlFormat::Pretty;
    bool gzip = false;
    bool sourcemap = false;
};

struct BundleObject {
    const char *dest_filename;
    const char *src_filename;
    bool unique;
};

struct FileHash {
    const char *name;
    const char *filename;
    const char *url;

    bool unique;
    uint8_t sha256[32];
};

struct AssetCopy {
    const char *dest_filename;
    const char *src_filename;
    HeapArray<const char *> ignore;
};

struct AssetBundle {
    const char *name;
    const char *dest_filename;
    const char *gzip_filename;
    const char *src_filename;
    const char *options;
};

struct AssetSet {
    BucketArray<FileHash> hashes;
    HashMap<const char *, const FileHash *> map;
};

struct PageSection {
    const char *id;
    const char *title;
    int level = 0;
};

struct PageData {
    const char *name;

    const char *url;
    const char *src_filename;
    const char *template_filename;
    const char *title;
    const char *menu;
    const char *description;
    bool toc;

    HeapArray<PageSection> sections;
    std::shared_ptr<const char> html_buf;
    Span<const char> html;
};

static RG_CONSTINIT ConstMap<128, int32_t, const char *> replacements = {
    { DecodeUtf8("Ç"), "c" },
    { DecodeUtf8("È"), "e" },
    { DecodeUtf8("É"), "e" },
    { DecodeUtf8("Ê"), "e" },
    { DecodeUtf8("Ë"), "e" },
    { DecodeUtf8("À"), "a" },
    { DecodeUtf8("Å"), "a" },
    { DecodeUtf8("Â"), "a" },
    { DecodeUtf8("Ä"), "a" },
    { DecodeUtf8("Î"), "i" },
    { DecodeUtf8("Ï"), "i" },
    { DecodeUtf8("Ù"), "u" },
    { DecodeUtf8("Ü"), "u" },
    { DecodeUtf8("Û"), "u" },
    { DecodeUtf8("Ú"), "u" },
    { DecodeUtf8("Ñ"), "n" },
    { DecodeUtf8("Ô"), "o" },
    { DecodeUtf8("Ó"), "o" },
    { DecodeUtf8("Ö"), "o" },
    { DecodeUtf8("Œ"), "oe" },
    { DecodeUtf8("Ÿ"), "y" },

    { DecodeUtf8("ç"), "c" },
    { DecodeUtf8("è"), "e" },
    { DecodeUtf8("é"), "e" },
    { DecodeUtf8("ê"), "e" },
    { DecodeUtf8("ë"), "e" },
    { DecodeUtf8("à"), "a" },
    { DecodeUtf8("å"), "a" },
    { DecodeUtf8("â"), "a" },
    { DecodeUtf8("ä"), "a" },
    { DecodeUtf8("î"), "i" },
    { DecodeUtf8("ï"), "i" },
    { DecodeUtf8("ù"), "u" },
    { DecodeUtf8("ü"), "u" },
    { DecodeUtf8("û"), "u" },
    { DecodeUtf8("ú"), "u" },
    { DecodeUtf8("ñ"), "n" },
    { DecodeUtf8("ô"), "o" },
    { DecodeUtf8("ó"), "o" },
    { DecodeUtf8("ö"), "o" },
    { DecodeUtf8("œ"), "oe" },
    { DecodeUtf8("ÿ"), "y" }
};

static const char *SectionToPageName(Span<const char> section, Allocator *alloc)
{
    Span<const char> basename = SplitStrReverseAny(section, RG_PATH_SEPARATORS);

    Span<const char> simple;
    SplitStrReverse(basename, '.', &simple);

    const char *name = DuplicateString(simple.len ? simple : basename, alloc).ptr;
    return name;
}

static const char *TextToID(Span<const char> text, char replace_char, Allocator *alloc)
{
    Span<char> id = AllocateSpan<char>(alloc, text.len + 1);

    Size offset = 0;
    bool skip_special = false;

    // Reset length
    id.len = 0;

    while (offset < text.len) {
        int32_t uc;
        Size bytes = DecodeUtf8(text, offset, &uc);

        if (bytes == 1) {
            if (IsAsciiAlphaOrDigit((char)uc)) {
                id[id.len++] = LowerAscii((char)uc);
                skip_special = false;
            } else if (!skip_special) {
                id[id.len++] = replace_char;
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

            MemCpy(id.end(), ptr, expand);
            id.len += expand;

            skip_special = false;
        } else {
            LogError("Illegal UTF-8 sequence");
            return nullptr;
        }

        offset += bytes;
    }

    id = TrimStr(id, replace_char);
    if (!id.len)
        return nullptr;

    id.ptr[id.len] = 0;

    return id.ptr;
}

static const char *FindEsbuild(const char *path, Allocator *alloc)
{
    // Try environment first
    {
        const char *str = GetEnv("ESBUILD_PATH");

        if (str && str[0])
            return str;
    }

    FileInfo file_info;
    StatResult stat = StatFile(path, (int)StatFlag::SilentMissing, &file_info);

    if (stat == StatResult::MissingPath) {
        goto missing;
    } else if (stat != StatResult::Success) {
        return nullptr;
    }

    if (file_info.type == FileType::Directory) {
#if defined(_WIN32)
        const char *os = "win32";
#elif defined(__linux__)
        const char *os = "linux";
#elif defined(__APPLE__)
        const char *os = "darwin";
#elif defined(__FreeBSD__)
        const char *os = "freebsd";
#elif defined(__OpenBSD__)
        const char *os = "openbsd";
#else
        const char *os = nullptr;
#endif

#if defined(__i386__) || defined(_M_IX86)
        const char *arch = "ia32";
#elif defined(__x86_64__) || defined(_M_AMD64)
        const char *arch = "x64";
#elif defined(__aarch64__) || defined(_M_ARM64)
        const char *arch = "arm64";
#else
        const char *arch = nullptr;
#endif

        if (os && arch) {
            char suffix[64];
            Fmt(suffix, "%1-%2/bin/esbuild%3", os, arch, RG_EXECUTABLE_EXTENSION);

            const char *binary = NormalizePath(suffix, path, alloc).ptr;

            if (TestFile(binary)) {
                path = binary;
            } else {
                goto missing;
            }
        } else {
            goto missing;
        }
    }

    return path;

missing:
    LogError("Cannot find esbuild, please set ESBUILD_PATH");
    return nullptr;
}

static bool ParseEsbuildMeta(const char *filename, Allocator *alloc, HeapArray<BundleObject> *out_objects)
{
    RG_ASSERT(alloc);

    BlockAllocator temp_alloc;

    Size prev_len = out_objects->len;
    RG_DEFER_N(err_guard) { out_objects->RemoveFrom(prev_len); };

    StreamReader reader(filename);
    if (!reader.IsValid())
        return false;
    json_Parser parser(&reader, &temp_alloc);

    parser.ParseObject();
    while (parser.InObject()) {
        Span<const char> key = {};
        parser.ParseKey(&key);

        if (key == "outputs") {
            parser.ParseObject();
            while (parser.InObject()) {
                Span<const char> output = {};
                HeapArray<const char *> inputs;
                const char *js = nullptr;
                const char *css = nullptr;

                parser.ParseKey(&output);

                parser.ParseObject();
                while (parser.InObject()) {
                    Span<const char> key = {};
                    parser.ParseKey(&key);

                    if (key == "entryPoint") {
                        parser.ParseString(&js);
                    } else if (key == "cssBundle") {
                        parser.ParseString(&css);
                    } else if (key == "inputs") {
                        parser.ParseObject();
                        while (parser.InObject()) {
                            const char *input = nullptr;
                            parser.ParseKey(&input);

                            inputs.Append(input);

                            parser.Skip();
                        }
                    } else {
                        parser.Skip();
                    }
                }

                if (js) {
                    out_objects->Append({
                        .dest_filename = NormalizePath(output, alloc).ptr,
                        .src_filename = DuplicateString(js, alloc).ptr,
                        .unique = false
                    });

                    if (css) {
                        Span<const char> prefix;
                        SplitStrReverse(js, '.', &prefix);

                        out_objects->Append({
                            .dest_filename = NormalizePath(css, alloc).ptr,
                            .src_filename = Fmt(alloc, "%1.css", prefix).ptr,
                            .unique = false
                        });
                    }
                } else if (inputs.len == 1) {
                    out_objects->Append({
                        .dest_filename = NormalizePath(output, alloc).ptr,
                        .src_filename = DuplicateString(inputs[0], alloc).ptr,
                        .unique = true
                    });
                }
            }
        } else {
            parser.Skip();
        }
    }
    if (!parser.IsValid())
        return false;
    reader.Close();

    err_guard.Disable();
    return true;
}

static bool BundleScript(const AssetBundle &bundle, const char *esbuild_binary,
                         bool sourcemap, bool gzip, Allocator *alloc, HeapArray<FileHash> *out_hashes)
{
    Span<const char> basename = SplitStrReverseAny(bundle.name, RG_PATH_SEPARATORS);
    Span<const char> prefix = MakeSpan(bundle.name, basename.ptr - bundle.name);

    const char *meta_filename = Fmt(alloc, "%1.meta", bundle.dest_filename).ptr;
    RG_DEFER { UnlinkFile(meta_filename); };

    // Prepare command
    const char *cmd;
    {
        HeapArray<char> buf(alloc);

        Fmt(&buf, "\"%1\" \"%2\" --bundle --log-level=warning --allow-overwrite --outfile=\"%3\""
                  "  --minify --platform=browser --target=es6 --metafile=\"%4\"",
                  esbuild_binary, bundle.src_filename, bundle.dest_filename, meta_filename);

        if (sourcemap) {
            Fmt(&buf, " --sourcemap=inline");
        }
        if (bundle.options) {
            Fmt(&buf, " %1", bundle.options);
        }

        cmd = buf.TrimAndLeak(1).ptr;
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
            StdErr->Write(output_buf);

            return false;
        }
    }

    // List output files
    HeapArray<BundleObject> bundle_objects;
    if (!ParseEsbuildMeta(meta_filename, alloc, &bundle_objects))
        return false;

    // Handle output files
    for (const BundleObject &obj: bundle_objects) {
        FileHash *hash = out_hashes->AppendDefault();

        Span<const char> basename = SplitStrReverseAny(obj.dest_filename, RG_PATH_SEPARATORS);
        const char *gzip_filename = Fmt(alloc, "%1.gz", obj.dest_filename).ptr;

        hash->name = obj.src_filename;
        hash->filename = obj.dest_filename;
        hash->url = Fmt(alloc, "%1%2", prefix, basename).ptr;
        hash->unique = obj.unique;

        StreamReader reader(obj.dest_filename);

        // Compute destination hash
        if (!obj.unique) {
            crypto_hash_sha256_state state;
            crypto_hash_sha256_init(&state);

            do {
                LocalArray<uint8_t, 16384> buf;
                buf.len = reader.Read(buf.data);
                if (buf.len < 0)
                    return false;

                crypto_hash_sha256_update(&state, buf.data, buf.len);
            } while (!reader.IsEOF());

            crypto_hash_sha256_final(&state, hash->sha256);
        }

        // Precompress file
        if (gzip) {
            reader.Rewind();
            StreamWriter writer(gzip_filename, (int)StreamWriterFlag::Atomic, CompressionType::Gzip);

            if (!SpliceStream(&reader, -1, &writer))
                return false;
            if (!writer.Close())
                return false;
        } else {
            UnlinkFile(gzip_filename);
        }
    }

    return true;
}

static void RenderAsset(Span<const char> path, const FileHash *hash, StreamWriter *writer)
{
    if (hash) {
        if (hash->unique) {
            Print(writer, "/%1", hash->url);
        } else {
            FmtArg suffix = FmtSpan(MakeSpan(hash->sha256, 8), FmtType::BigHex, "").Pad0(-2);
            Print(writer, "/%1?%2", hash->url, suffix);
        }
    } else {
        LogWarning("Unknown asset '%1'", path);
        Print(writer, "/%1", path);
    }
}

// XXX: Resolve page links in content
static bool RenderMarkdown(PageData *page, const AssetSet &assets, Allocator *alloc)
{
    HeapArray<char> content;
    if (page->src_filename && ReadFile(page->src_filename, Mebibytes(8), &content) < 0)
        return false;
    Span<const char> remain = TrimStr(content.As());

    cmark_gfm_core_extensions_ensure_registered();

    // Prepare markdown parser
    cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT | CMARK_OPT_FOOTNOTES);
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

                if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                    const char *literal = cmark_node_get_literal(child);
                    RG_ASSERT(literal);

                    Span<const char> toc;
                    Span<const char> title = SplitStr(literal, '^', &toc);

                    if (toc.len) {
                        toc = DuplicateString(toc, alloc);
                        title = DuplicateString(title, alloc);

                        cmark_node_set_literal(child, title.ptr);
                    } else {
                        toc = DuplicateString(title, alloc);
                    }

                    const char *id = TextToID(title, '-', alloc);

                    if (level < 3) {
                        PageSection sec = {};

                        sec.level = level;
                        sec.title = toc.ptr;
                        sec.id = id;

                        page->sections.Append(sec);
                    }

                    cmark_node *frag = cmark_node_new(CMARK_NODE_HTML_INLINE);
                    if (strchr(id, '-')) {
                        const char *old_id = TextToID(title, '_', alloc);
                        cmark_node_set_literal(frag, Fmt(alloc, "<a id=\"%1\"></a><a id=\"%2\"></a>", id, old_id).ptr);
                    } else {
                        cmark_node_set_literal(frag, Fmt(alloc, "<a id=\"%1\"></a>", id).ptr);
                    }
                    cmark_node_prepend_child(node, frag);
                }
            }

            // Support GitHub-like alerts
            if (event == CMARK_EVENT_EXIT && type == CMARK_NODE_BLOCK_QUOTE) {
                cmark_node *child = cmark_node_first_child(node);
                cmark_node *text = child;

                if (cmark_node_get_type(text) == CMARK_NODE_PARAGRAPH) {
                    text = cmark_node_first_child(text);
                }

                if (cmark_node_get_type(text) == CMARK_NODE_TEXT) {
                    const char *literal = cmark_node_get_literal(text);
                    RG_ASSERT(literal);

                    const char *cls = nullptr;

                    if (TestStr(literal, "[!NOTE]")) {
                        cls = "note";
                    } else if (TestStr(literal, "[!TIP]")) {
                        cls = "tip";
                    } else if (TestStr(literal, "[!IMPORTANT]")) {
                        cls = "important";
                    } else if (TestStr(literal, "[!WARNING]")) {
                        cls = "warning";
                    } else if (TestStr(literal, "[!CAUTION]")) {
                        cls = "caution";
                    }

                    if (cls) {
                        RG_DEFER {
                            cmark_node_free(node);
                            cmark_node_free(text);
                        };

                        const char *tag = Fmt(alloc, "<div class=\"alert %1\">", cls).ptr;

                        cmark_node *block = cmark_node_new(CMARK_NODE_CUSTOM_BLOCK);
                        cmark_node *title = cmark_node_new(CMARK_NODE_HTML_INLINE);

                        cmark_node_set_on_enter(block, tag);
                        cmark_node_set_on_exit(block, "</div>");
                        cmark_node_set_literal(title, "<div class=\"title\"></div>");

                        cmark_node_replace(node, block);

                        cmark_node_append_child(block, title);

                        do {
                            cmark_node *next = cmark_node_next(child);

                            cmark_node_unlink(child);
                            cmark_node_append_child(block, child);

                            child = next;
                        } while (child);
                    }
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
        Size i = idx;
        Size j = i + 1;

        while (j < end) {
            Span<const char> menu = pages[j].menu;

            for (Size i = 0; i <= depth; i++) {
                Span<const char> remain = menu;
                TrimStr(SplitStr(remain, '/', &remain));

                if (!remain.len)
                    break;

                menu = remain;
            }

            if (!TestStr(menu, title))
                break;

            j++;
        }

        bool active = (active_idx >= i && active_idx < j);
        int margin = std::max(0, depth - 1);

        Print(writer, "<a href=\"%1\"%2 style=\"margin-left: %3em;\">%4</a>", page->url, active ? " class=\"active\"" : "", margin, title);
        PrintLn(writer, "%1", depth ? "" : "</li>");

        return j;
    }
}

static bool RenderTemplate(const char *template_filename, Span<const PageData> pages, Size page_idx,
                           const AssetSet &assets, const char *dest_filename)
{
    const PageData &page = pages[page_idx];

    if (!template_filename) {
        bool success = WriteFile(page.html, dest_filename, (int)StreamWriterFlag::Atomic);
        return success;
    }

    StreamReader reader(template_filename);
    StreamWriter writer(dest_filename, (int)StreamWriterFlag::Atomic);

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
            if (page.toc && page.sections.len > 1) {
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

    if (StartsWith(mimetype, "text/"))
        return true;
    if (TestStr(mimetype, "application/javascript"))
        return true;
    if (TestStr(mimetype, "application/json"))
        return true;
    if (TestStr(mimetype, "application/xml"))
        return true;
    if (TestStr(mimetype, "image/svg+xml"))
        return true;

    return false;
}

static bool BuildAll(Span<const char> source_dir, const BuildSettings &build, const char *output_dir)
{
    BlockAllocator temp_alloc;

    // Output directory
    if (!MakeDirectory(output_dir, false))
        return false;

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
            page.title = page.name;
            page.description = "";
            page.toc = true;

            do {
                if (prop.key == "URL") {
                    page.url = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "SourceFile") {
                    page.src_filename = NormalizePath(prop.value, source_dir, &temp_alloc).ptr;
                } else if (prop.key == "Title") {
                    page.title = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "Menu") {
                    page.menu = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "Description") {
                    page.description = DuplicateString(prop.value, &temp_alloc).ptr;
                } else if (prop.key == "ToC") {
                    valid &= ParseBool(prop.value, &page.toc);
                } else if (prop.key == "Template") {
                    page.template_filename = NormalizePath(prop.value, source_dir, &temp_alloc).ptr;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (page.url) {
                page.src_filename = nullptr;
            } else {
                if (TestStr(page.name, "index")) {
                    page.url = "/";
                } else {
                    switch (build.urls) {
                        case UrlFormat::Pretty:
                        case UrlFormat::PrettySub: { page.url = Fmt(&temp_alloc, "/%1", page.name).ptr; } break;
                        case UrlFormat::Ugly: { page.url = Fmt(&temp_alloc, "/%1.html", page.name).ptr; } break;
                    }
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

                    copy->dest_filename = NormalizePath(prop.section, &temp_alloc).ptr;

                    while (ini.NextInSection(&prop)) {
                        if (prop.key == "From") {
                            copy->src_filename = NormalizePath(prop.value, source_dir, &temp_alloc).ptr;
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

                    if (!copy->src_filename) {
                        LogError("Missing copy source filename");
                        valid = false;
                    }
                } else if (prop.value == "Bundle") {
                    AssetBundle *bundle = bundles.AppendDefault();

                    bundle->name = DuplicateString(prop.section, &temp_alloc).ptr;
                    bundle->dest_filename = NormalizePath(prop.section, output_dir, &temp_alloc).ptr;
                    bundle->gzip_filename = Fmt(&temp_alloc, "%1.gz", bundle->dest_filename).ptr;

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

        copy.dest_filename = ".";
        copy.src_filename = Fmt(&temp_alloc, "%1%/assets", source_dir).ptr;

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
        {
            FileInfo file_info;
            if (StatFile(copy.src_filename, &file_info) != StatResult::Success)
                return false;

            switch (file_info.type) {
                case FileType::Directory: {
                    if (!EnumerateFiles(copy.src_filename, nullptr, 3, 1024, &temp_alloc, &src_filenames))
                        return false;
                } break;
                case FileType::File: {
                    src_filenames.Append(copy.src_filename);
                } break;

                case FileType::Link:
                case FileType::Device:
                case FileType::Pipe:
                case FileType::Socket: {
                    LogError("Cannot copy '%1' with unexpected file type '%2'", copy.src_filename, FileTypeNames[(int)file_info.type]);
                    return false;
                } break;
            }
        }

        // Remove ignored patterns
        src_filenames.RemoveFrom(std::remove_if(src_filenames.begin(), src_filenames.end(),
                                                 [&](const char *filename) {
            return std::any_of(copy.ignore.begin(), copy.ignore.end(),
                               [&](const char *pattern) { return MatchPathSpec(filename, pattern); });
        }) - src_filenames.begin());

        Size prefix_len = strlen(copy.src_filename);

        for (const char *src_filename: src_filenames) {
            const char *basename = TrimStrLeft(src_filename + prefix_len, RG_PATH_SEPARATORS).ptr;

            Span<const char> url = NormalizePath(basename, copy.dest_filename, &temp_alloc);
            const char *dest_filename = Fmt(&temp_alloc, "%1%/%2", output_dir, url).ptr;
            const char *gzip_filename = Fmt(&temp_alloc, "%1.gz", dest_filename).ptr;

#if defined(_WIN32)
            for (char &c: url.As<char>()) {
                c = (c == '\\') ? '/' : c;
            }
#endif

            FileHash *hash = assets.hashes.AppendDefault();

            hash->name = src_filename;
            hash->filename = dest_filename;
            hash->url = url.ptr;

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
                if (build.gzip && ShouldCompressFile(dest_filename)) {
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

            assets.map.Set(hash->name, hash);
            assets.map.Set(hash->url, hash);
        }

        if (!async.Sync())
            return false;
    }

    // Bundle JS files
    {
        Async async;
        std::mutex mutex;

        for (const AssetBundle &bundle: bundles) {
            async.Run([=, &assets, &mutex, &temp_alloc] {
                BlockAllocator thread_alloc;

                HeapArray<FileHash> hashes;
                if (!BundleScript(bundle, esbuild_path, build.sourcemap, build.gzip,
                                  &thread_alloc, &hashes))
                    return false;

                std::lock_guard<std::mutex> lock(mutex);

                for (const FileHash &hash: hashes) {
                    FileHash *copy = assets.hashes.AppendDefault();

                    copy->name = DuplicateString(hash.name, &temp_alloc).ptr;
                    copy->filename = DuplicateString(hash.filename, &temp_alloc).ptr;
                    copy->url = DuplicateString(hash.url, &temp_alloc).ptr;
                    copy->unique = hash.unique;
                    MemCpy(copy->sha256, hash.sha256, RG_SIZE(hash.sha256));

                    assets.map.Set(copy->name, copy);
                    assets.map.Set(copy->url, copy);
                }

                return true;
            });
        }

        if (!async.Sync())
            return false;
    }

    // Render pages
    for (PageData &page: pages) {
        if (!page.src_filename)
            continue;

        Span<const char> ext = GetPathExtension(page.src_filename);

        if (ext == ".html") {
            page.template_filename = page.src_filename;
        } else if (ext == ".md") {
            if (!RenderMarkdown(&page, assets, &temp_alloc))
                return false;
        } else {
            LogError("Cannot render pages with '%1' extension", ext);
            return false;
        }
    }

    // Render templates
    {
        Async async;

        for (Size i = 0; i < pages.len; i++) {
            const char *template_filename = pages[i].template_filename;
            Span<const char> ext = template_filename ? GetPathExtension(pages[i].template_filename) : ".html";

            const char *dest_filename;
            if (build.urls == UrlFormat::PrettySub && !TestStr(pages[i].name, "index")) {
                dest_filename = Fmt(&temp_alloc, "%1%/%2%/index%3", output_dir, pages[i].name, ext).ptr;
                if (!EnsureDirectoryExists(dest_filename))
                    return false;
            } else {
                dest_filename = Fmt(&temp_alloc, "%1%/%2%3", output_dir, pages[i].name, ext).ptr;
            }

            bool gzip_file = build.gzip && TestStr(ext, ".html");
            const char *gzip_filename = Fmt(&temp_alloc, "%1.gz", dest_filename).ptr;

            async.Run([=, &pages, &assets]() {
                if (!RenderTemplate(template_filename, pages, i, assets, dest_filename))
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

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Options
    const char *source_dir = ".";
    const char *output_dir = nullptr;
    BuildSettings build;
    bool loop = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...] -O output_dir%!0

Options:

    %!..+-S, --source_dir filename%!0      Set source directory
                                   %!D..(default: %2)%!0

    %!..+-O, --output_dir directory%!0     Set output directory
    %!..+-u, --urls format%!0              Change URL format
                                   %!D..(default: %3)%!0
        %!..+--gzip%!0                     Create static gzip files

        %!..+--sourcemap%!0                Add inline sourcemaps to bundles
    %!..+-l, --loop%!0                     Build repeatedly until interrupted

Available URL formats: %!..+%4%!0)",
                FelixTarget, source_dir, UrlFormatNames[(int)build.urls], FmtSpan(UrlFormatNames));
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
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-S", "--source_dir", OptionType::Value)) {
                source_dir = opt.current_value;
            } else if (opt.Test("-O", "--output_dir", OptionType::Value)) {
                output_dir = opt.current_value;
            } else if (opt.Test("-u", "--urls", OptionType::Value)) {
                if (!OptionToEnumI(UrlFormatNames, opt.current_value, &build.urls)) {
                    LogError("Unknown URL format '%1'", opt.current_value);
                    return true;
                }
            } else if (opt.Test("--gzip")) {
                build.gzip = true;
            } else if (opt.Test("--sourcemap")) {
                build.sourcemap = true;
            } else if (opt.Test("-l", "--loop")) {
                loop = true;
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

    LogInfo("Source directory: %!..+%1%!0", source_dir);
    LogInfo("Output directory: %!..+%1%!0", output_dir);

    if (loop) {
        for (;;) {
            if (BuildAll(source_dir, build, output_dir)) {
                LogInfo("Build successful");
            } else {
                LogError("Build failed");
            }

            WaitForResult ret = WaitForInterrupt(1000);

            if (ret == WaitForResult::Exit) {
                break;
            } else if (ret == WaitForResult::Interrupt) {
                return 1;
            }
        }
    } else {
        if (!BuildAll(source_dir, build, output_dir))
            return 1;
    }

    LogInfo("Done!");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
