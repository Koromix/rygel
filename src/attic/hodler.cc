// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "src/core/libcc/libcc.hh"
#include "src/core/libnet/libnet.hh"
extern "C" {
    #include "vendor/libsoldout/soldout.h"
}

namespace RG {

struct PageSection {
    const char *id;
    const char *title;
    int level = 0;
};

struct PageData {
    const char *src_filename;

    const char *title;
    const char *menu;
    const char *created;
    const char *modified;
    HeapArray<PageSection> sections;

    std::shared_ptr<const char> html_buf;
    Span<const char> html;

    const char *name;
    const char *url;
};

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

static const char *FileNameToPageName(const char *filename, Allocator *alloc)
{
    // File name and extension
    Span<const char> name = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);
    SplitStrReverse(name, '.', &name);

    // Remove leading number and underscore if any
    {
        const char *after_number;
        strtol(name.ptr, const_cast<char **>(&after_number), 10);
        if (after_number && after_number[0] == '_') {
            name = MakeSpan(after_number + 1, name.end());
        }
    }

    // Filter out unwanted characters
    char *name2 = DuplicateString(name, alloc).ptr;
    for (Size i = 0; name2[i]; i++) {
        name2[i] = IsAsciiAlphaOrDigit(name2[i]) ? name2[i] : '_';
    }

    return name2;
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

// XXX: Resolve page links in content
static bool RenderPageContent(PageData *page, Allocator *alloc)
{
    buf *ib = bufnew(1024);
    RG_DEFER { bufrelease(ib); };

    // Load the file, struct buf is used by libsoldout
    {
        StreamReader st(page->src_filename);

        Size bytes_read;
        bufgrow(ib, 1024);
        while ((bytes_read = st.Read(ib->asize - ib->size, ib->data + ib->size)) > 0) {
            ib->size += bytes_read;
            bufgrow(ib, ib->size + 1024);
        }

        if (!st.IsValid())
            return false;
    }

    struct RenderContext {
        Allocator *alloc;
        PageData *page;
    };

    mkd_renderer renderer = discount_html;
    RenderContext ctx = {};
    ctx.page = page;
    ctx.alloc = alloc;
    renderer.opaque = &ctx;

    // Get page sections from the parser
    renderer.header = [](buf *ob, buf *text, int level, void *udata) {
        RenderContext *ctx = (RenderContext *)udata;

        if (level < 3) {
            PageSection sec = {};

            sec.level = level;
            sec.title = DuplicateString(MakeSpan(text->data, text->size), ctx->alloc).ptr;
            sec.id = TextToID(sec.title, ctx->alloc);

            if (sec.id) {
                // XXX: Detect duplicate sections
                ctx->page->sections.Append(sec);
                bufprintf(ob, "<h%d id=\"%s\">%s</h%d>", level, sec.id, sec.title, level);
            } else {
                bufprintf(ob, "<h%d>%.*s</h%d>", level, (int)text->size, text->data, level);
            }
        } else {
            bufprintf(ob, "<h%d>%.*s</h%d>", level, (int)text->size, text->data, level);
        }
    };

    // We use HTML comments for metadata (creation date, etc.),
    // for example '<!-- Title: foobar -->' or '<!-- Created: 2016-01-12 -->'.
    renderer.blockhtml = [](buf *ob, buf *text, void *udata) {
        RenderContext *ctx = (RenderContext *)udata;

        Size size = text->size;
        while (size && text->data[size - 1] == '\n') {
            size--;
        }
        if (size >= 7 && !memcmp(text->data, "<!--", 4) &&
                         !memcmp(text->data + size - 3, "-->", 3)) {
            Span<const char> comment = MakeSpan(text->data + 4, size - 7);

            while (comment.len) {
                Span<const char> line = SplitStr(comment, '\n', &comment);

                Span<const char> value;
                Span<const char> name = TrimStr(SplitStr(line, ':', &value));
                value = TrimStr(value);

                if (value.ptr == name.end())
                    break;

                const char **attr_ptr;
                if (name == "Title") {
                    attr_ptr = &ctx->page->title;
                } else if (name == "Menu") {
                    attr_ptr = &ctx->page->menu;
                } else if (name == "Created") {
                    attr_ptr = &ctx->page->created;
                } else if (name == "Modified") {
                    attr_ptr = &ctx->page->modified;
                } else {
                    LogError("%1: Unknown attribute '%2'", ctx->page->src_filename, name);
                    continue;
                }

                if (*attr_ptr) {
                    LogError("%1: Overwriting attribute '%2' (already set)",
                            ctx->page->src_filename, name);
                }
                *attr_ptr = DuplicateString(value, ctx->alloc).ptr;
            }
        } else {
            discount_html.blockhtml(ob, text, udata);
        }
    };

    // We need <span> tags around code lines for CSS line numbering
    renderer.blockcode = [](buf *ob, buf *text, void *) {
        if (ob->size) {
            bufputc(ob, '\n');
        }

        BUFPUTSL(ob, "<pre>");
        if (text) {
            size_t start, end = 0;
            for (;;) {
                start = end;
                while (end < text->size && text->data[end] != '\n') {
                    end++;
                }
                if (end == text->size)
                    break;

                BUFPUTSL(ob, "<span class=\"line\">");
                lus_body_escape(ob, text->data + start, end - start);
                BUFPUTSL(ob, "</span>\n");

                end++;
            }
        }
        BUFPUTSL(ob, "</pre>\n");
    };

    // Convert Markdown to HTML
    {
        buf *ob = bufnew(64);
        RG_DEFER { free(ob); };

        markdown(ob, ib, &renderer);
        page->html_buf.reset(ob->data, free);
        page->html = MakeSpan(ob->data, ob->size);
    }

    return true;
}

static bool RenderFullPage(Span<const uint8_t> html, Span<const PageData> pages,
                           Size page_idx, const char *dest_filename)
{
    StreamWriter st(dest_filename, (int)StreamWriterFlag::Atomic);

    const PageData &page = pages[page_idx];

    bool success = PatchFile(html, &st, [&](Span<const char> key, StreamWriter *writer) {
        if (key == "BUSTER") {
            Print(writer, "%1", FmtRandom(8));
        } else if (key == "TITLE") {
            writer->Write(page.title);
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
                    PrintLn(writer, "<li><a%1>%2</a><div>", active ? " class=\"active\"" : "", category);

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
                PrintLn(writer, "<nav id=\"side\"><ul>");

                for (const PageSection &sec: page.sections) {
                    PrintLn(writer, "<li><a href=\"#%1\" class=\"lv%2\">%3</a></li>",
                                    sec.id, sec.level, sec.title);
                }

                PrintLn(writer, "</ul></nav>");
            }
        } else if (key == "CONTENT") {
            writer->Write(page.html);
        } else {
            Print(writer, "{%1}", key);
        }
    });

    if (!success)
        return false;
    if (!st.Close())
        return false;

    return true;
}

static bool BuildAll(const char *input_dir, const char *template_dir, UrlFormat urls,
                     const char *output_dir, bool gzip)
{
    BlockAllocator temp_alloc;

    // List input files
    HeapArray<const char *> page_filenames;
    if (!EnumerateFiles(input_dir, "*.md", 0, 1024, &temp_alloc, &page_filenames))
        return false;
    std::sort(page_filenames.begin(), page_filenames.end(),
              [](const char *filename1, const char *filename2) { return CmpStr(filename1, filename2) < 0; });

    // List pages
    HeapArray<PageData> pages;
    {
        HashMap<const char *, Size> pages_map;

        for (const char *filename: page_filenames) {
            PageData page = {};

            page.src_filename = filename;
            if (!RenderPageContent(&page, &temp_alloc))
                return false;
            page.name = FileNameToPageName(filename, &temp_alloc);

            if (TestStr(page.name, "index")) {
                page.url = "/";
            } else {
                switch (urls) {
                    case UrlFormat::Pretty:
                    case UrlFormat::PrettySub: { page.url = Fmt(&temp_alloc, "/%1", page.name).ptr; } break;
                    case UrlFormat::Ugly: { page.url = Fmt(&temp_alloc, "/%1.html", page.name).ptr; } break;
                }
            }

            bool valid = true;
            if (!page.name) {
                LogError("%1: Page with empty name", page.src_filename);
                valid = false;
            }
            if (!page.title) {
                LogError("%1: Ignoring page without title", page.src_filename);
                valid = false;
            }
            if (!page.created) {
                LogError("%1: Missing creation date", page.src_filename);
            }
            if (Size prev_idx = pages_map.FindValue(page.name, -1); prev_idx >= 0) {
                LogError("%1: Ignoring duplicate of '%2'",
                         page.src_filename, pages[prev_idx].src_filename);
                valid = false;
            }

            if (valid) {
                pages_map.Set(page.name, pages.len);
                pages.Append(page);
            }
        }
    }

    // Output directory
    if (!MakeDirectory(output_dir, false))
        return false;
    LogInfo("Template: %!..+%1%!0", template_dir);
    LogInfo("Output directory: %!..+%1%!0", output_dir);

    const char *static_directories[] = {
        Fmt(&temp_alloc, "%1%/static", input_dir).ptr,
        Fmt(&temp_alloc, "%1%/static", template_dir).ptr
    };

    HeapArray<uint8_t> page_html;
    HeapArray<uint8_t> index_html;
    {
        const char *page_filename = Fmt(&temp_alloc, "%1%/page.html", template_dir).ptr;
        const char *index_filename = Fmt(&temp_alloc, "%1%/index.html", template_dir).ptr;

        if (!ReadFile(page_filename, Mebibytes(1), &page_html))
            return false;
        if (TestFile(index_filename)) {
            if (!ReadFile(index_filename, Mebibytes(1), &index_html))
                return false;
        } else {
            index_html.Append(page_html);
        }
    }

    Async async;

    // Output fully-formed pages
    for (Size i = 0; i < pages.len; i++) {
        const PageData &page = pages[i];

        const char *dest_filename;
        if (urls == UrlFormat::PrettySub && !TestStr(pages[i].name, "index")) {
            dest_filename = Fmt(&temp_alloc, "%1%/%2%/index.html", output_dir, page.name).ptr;
            if (!EnsureDirectoryExists(dest_filename))
                return false;
        } else {
            dest_filename = Fmt(&temp_alloc, "%1%/%2.html", output_dir, page.name).ptr;
        }

        const char *gzip_filename = Fmt(&temp_alloc, "%1.gz", dest_filename).ptr;

        async.Run([=, &pages]() {
            Span<const uint8_t> html = TestStr(pages[i].name, "index") ? index_html : page_html;

            if (!RenderFullPage(html, pages, i, dest_filename))
                return false;

            if (gzip) {
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

    // Copy template assets
    for (const char *static_directory: static_directories) {
        if (TestFile(static_directory, FileType::Directory)) {
            HeapArray<const char *> static_filenames;
            if (!EnumerateFiles(static_directory, nullptr, 3, 1024, &temp_alloc, &static_filenames))
                return false;

            Size prefix_len = strlen(static_directory);

            for (const char *src_filename: static_filenames) {
                const char *basename = TrimStrLeft(src_filename + prefix_len, RG_PATH_SEPARATORS).ptr;

                const char *dest_filename = Fmt(&temp_alloc, "%1%/static%/%2", output_dir, basename).ptr;
                const char *gzip_filename = Fmt(&temp_alloc, "%1.gz", dest_filename).ptr;

                async.Run([=]() {
                    if (!EnsureDirectoryExists(dest_filename))
                        return false;

                    // Open ahead of time because src_filename won't stay valid
                    StreamReader reader(src_filename);

                    // Copy raw file
                    {
                        StreamWriter writer(dest_filename, (int)StreamWriterFlag::Atomic);

                        if (!SpliceStream(&reader, Megabytes(4), &writer))
                            return false;
                        if (!writer.Close())
                            return false;
                    }

                    // Create gzipped version
                    if (gzip && http_ShouldCompressFile(dest_filename)) {
                        reader.Rewind();

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
        }
    }

    return async.Sync();
}

int Main(int argc, char *argv[])
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    BlockAllocator temp_alloc;

    // Options
    const char *input_dir = nullptr;
    const char *template_dir = {};
    const char *output_dir = nullptr;
    bool gzip = false;
    UrlFormat urls = UrlFormat::Pretty;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <input_dir> -O <output_dir>%!0

Options:
    %!..+-T, --template_dir <dir>%!0     Set template directory

    %!..+-O, --output_dir <dir>%!0       Set output directory
        %!..+--gzip%!0                   Create static gzip files

    %!..+-u, --urls <FORMAT>%!0          Change URL format (%2)
                                 %!D..(default: %3)%!0)",
                FelixTarget, FmtSpan(UrlFormatNames), UrlFormatNames[(int)urls]);
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
            } else if (opt.Test("-T", "--template_dir", OptionType::Value)) {
                template_dir = opt.current_value;
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

        input_dir = opt.ConsumeNonOption();
    }

    // Check arguments
    {
        bool valid = true;

        if (!input_dir) {
            LogError("Missing input directory");
            valid = false;
        }
        if (!output_dir) {
            LogError("Missing output directory");
            valid = false;
        }

        if (!valid)
            return 1;
    }

    if (!template_dir) {
        const char *directory = Fmt(&temp_alloc, "%1%/template", input_dir).ptr;

        if (!TestFile(directory, FileType::Directory)) {
            LogError("Missing template directory");
            return 1;
        }

        template_dir = directory;
    }

    if (!BuildAll(input_dir, template_dir, urls, output_dir, gzip))
        return 1;

    LogInfo("Done!");
    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }