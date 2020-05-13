// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../libblik/libblik.hh"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <io.h>
#else
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <unistd.h>
    #include <signal.h>
#endif

namespace RG {

struct LogEntry {
    LogLevel level;
    const char *ctx;
    const char *msg;
};

class LogTrace {
    HeapArray<LogEntry> entries;
    BlockAllocator str_alloc;

public:
    void Store(LogLevel level, const char *ctx, const char *msg)
    {
        LogEntry entry = {};

        entry.level = level;
        entry.ctx = DuplicateString(ctx, &str_alloc).ptr;
        entry.msg = DuplicateString(msg, &str_alloc).ptr;

        entries.Append(entry);
    }

    void Dump()
    {
        for (const LogEntry &entry: entries) {
            DefaultLogHandler(entry.level, entry.ctx, entry.msg);
        }

        entries.Clear();
        str_alloc.ReleaseAll();
    }
};

static bool input_is_raw;
#ifdef _WIN32
static HANDLE stdin_handle;
static DWORD input_orig_mode;
#else
static struct termios input_orig_tio;
#endif

static bool EnableRawMode()
{
    static bool init_atexit = false;

    if (!input_is_raw) {
#ifdef _WIN32
        stdin_handle = (HANDLE)_get_osfhandle(_fileno(stdin));

        if (GetConsoleMode(stdin_handle, &input_orig_mode)) {
            DWORD new_mode = input_orig_mode;
            new_mode &= ~(ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT);
            new_mode |= ENABLE_WINDOW_INPUT;

            input_is_raw = SetConsoleMode(stdin_handle, new_mode);

            if (input_is_raw && !init_atexit) {
                atexit([]() { SetConsoleMode(stdin_handle, input_orig_mode); });
                init_atexit = true;
            }
        }
#else
        if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &input_orig_tio) >= 0) {
            struct termios raw = input_orig_tio;

            raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            raw.c_oflag &= ~(OPOST);
            raw.c_cflag |= (CS8);
            raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;

            input_is_raw = (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) >= 0);

            if (input_is_raw && !init_atexit) {
                atexit([]() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &input_orig_tio); });
                init_atexit = true;
            }
        }
#endif
    }

    return input_is_raw;
}

static void DisableRawMode()
{
    if (input_is_raw) {
#ifdef _WIN32
        input_is_raw = !SetConsoleMode(stdin_handle, input_orig_mode);
#else
        input_is_raw = !(tcsetattr(STDIN_FILENO, TCSAFLUSH, &input_orig_tio) >= 0);
#endif
    }
}

class ConsolePrompter {
    const char *prompt = ">>> ";
    int prompt_columns = 4;

    HeapArray<HeapArray<char>> entries;
    Size entry_idx = 0;
    Size str_offset = 0;

    int columns = 0;
    int rows = 0;
    int rows_with_extra = 0;
    int x = 0;
    int y = 0;

    const char *fake_input = "";

public:
    HeapArray<char> str;

    ConsolePrompter();

    bool Read();
    void Commit();

private:
    void ChangeEntry(Size new_idx);

    void Prompt();

    int GetColumns();
    int GetCursorX();

    int ReadChar();
};

ConsolePrompter::ConsolePrompter()
{
    entries.AppendDefault();
}

bool ConsolePrompter::Read()
{
#ifndef _WIN32
    struct sigaction old_sa;
    {
        struct sigaction sa;

        sa.sa_handler = [](int) {};
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        sigaction(SIGWINCH, &sa, &old_sa);
    }
    RG_DEFER { sigaction(SIGWINCH, &old_sa, nullptr); };
#endif

    EnableRawMode();
    RG_DEFER { DisableRawMode(); };

    // Don't overwrite current line
    if (GetCursorX() > 0) {
        fputs("\r\n", stdout);
    }

    str_offset = str.len;
    Prompt();

    int c;
    while ((c = ReadChar()) >= 0) {
        // Fix display if terminal is resized
        if (GetColumns() != columns) {
            Prompt();
        }

        switch (c) {
            case 0x1B: {
                LocalArray<char, 16> buf;

                const auto match_escape = [&](const char *seq) {
                    RG_ASSERT(strlen(seq) < RG_SIZE(buf.data));

                    for (Size i = 0; seq[i]; i++) {
                        if (i >= buf.len) {
                            buf.Append(ReadChar());
                        }
                        if (buf[i] != seq[i])
                            return false;
                    }

                    return true;
                };

                if (match_escape("[1;5D")) { // Ctrl-Left
                    if (str_offset > 0) {
                        str_offset--;
                        while (str_offset > 0 && strchr(" \t\r\n", str[str_offset])) {
                            str_offset--;
                        }
                        while (str_offset > 0 && !strchr(" \t\r\n", str[str_offset - 1])) {
                            str_offset--;
                        }
                    }

                    Prompt();
                } else if (match_escape("[1;5C")) { // Ctrl-Right
                    if (str_offset < str.len) {
                        while (str_offset < str.len && strchr(" \t\r\n", str[str_offset])) {
                            str_offset++;
                        }
                        while (str_offset < str.len && !strchr(" \t\r\n", str[str_offset])) {
                            str_offset++;
                        }
                    }

                    Prompt();
                } else if (match_escape("[3~")) { // Delete
                    if (str_offset < str.len) {
                        memmove(str.ptr + str_offset, str.ptr + str_offset + 1, str.len - str_offset - 1);
                        str.len--;

                        Prompt();
                    }
                } else if (match_escape("[A")) { // Up
                    fake_input = "\x10";
                } else if (match_escape("[B")) { // Down
                    fake_input = "\x0E";
                } else if (match_escape("[D")) { // Left
                    fake_input = "\x02";
                } else if (match_escape("[C")) { // Right
                    fake_input = "\x06";
                } else if (match_escape("[H")) { // Home
                    fake_input = "\x01";
                } else if (match_escape("[F")) { // End
                    fake_input = "\x05";
                }
            } break;

            case 0x2: { // Left
                if (str_offset > 0) {
                    str_offset--;
                    Prompt();
                }
            } break;
            case 0x6: { // Right
                if (str_offset < str.len) {
                    str_offset++;
                    Prompt();
                }
            } break;
            case 0xE: { // Down
                Span<const char> remain = str.Take(str_offset, str.len - str_offset);
                SplitStr(remain, '\n', &remain);

                if (remain.len) {
                    Span<const char> line = SplitStr(remain, '\n', &remain);

                    Size line_offset = std::min(line.len, (Size)x - prompt_columns);
                    str_offset = std::min(line.ptr - str.ptr + line_offset, str.len);

                    Prompt();
                } else if (entry_idx < entries.len - 1) {
                    ChangeEntry(entry_idx + 1);
                    Prompt();
                }
            } break;
            case 0x10: { // Up
                Span<const char> remain = str.Take(0, str_offset);
                SplitStrReverse(remain, '\n', &remain);

                if (remain.len) {
                    Span<const char> line = SplitStrReverse(remain, '\n', &remain);

                    Size line_offset = std::min(line.len, (Size)x - prompt_columns);
                    str_offset = std::min(line.ptr - str.ptr + line_offset, str.len);

                    Prompt();
                } else if (entry_idx > 0) {
                    ChangeEntry(entry_idx - 1);
                    Prompt();
                }
            } break;

            case 0x1: { // Home
                while (str_offset > 0 && str[str_offset - 1] != '\n') {
                    str_offset--;
                }

                Prompt();
            } break;
            case 0x5: { // End
                while (str_offset < str.len && str[str_offset] != '\n') {
                    str_offset++;
                }

                Prompt();
            } break;

            case 0x8:
            case 0x7F: { // Backspace
                if (str_offset) {
                    memmove(str.ptr + str_offset - 1, str.ptr + str_offset, str.len - str_offset);
                    str_offset--;
                    str.len--;

                    if (str_offset == str.len && x > prompt_columns) {
                        fputs("\x1B[1D\x1B[0K", stdout);
                        x--;
                    } else {
                        Prompt();
                    }
                }
            } break;
            case 0x3: {
                fputs("\r\n", stdout);
                return false;
            } break;
            case 0x4: { // Ctrl-D
                if (!str.len) {
                    return false;
                } else if (str_offset < str.len) {
                    memmove(str.ptr + str_offset, str.ptr + str_offset + 1, str.len - str_offset - 1);
                    str.len--;

                    Prompt();
                }
            } break;
            case 0x14: { // Ctrl-T
                if (str_offset >= 2) {
                    std::swap(str[str_offset - 1], str[str_offset - 2]);
                    Prompt();
                }
            } break;
            case 0xB: { // Ctrl-K
                Span<const char> remain = str.Take(str_offset, str.len - str_offset);
                Size end_idx = str_offset + SplitStr(remain, '\n').len;

                if (end_idx > str_offset) {
                    memmove(str.ptr + str_offset, str.ptr + end_idx, str.len - end_idx);
                    str.len -= end_idx - str_offset;

                    Prompt();
                }
            } break;
            case 0x15: { // Ctrl-U
                Span<const char> remain = str.Take(0, str_offset);
                Size start_idx = SplitStrReverse(remain, '\n').ptr - str.ptr;

                if (start_idx < str_offset) {
                    memmove(str.ptr + start_idx, str.ptr + str_offset, str.len - str_offset);
                    str.len -= str_offset - start_idx;
                    str_offset = start_idx;

                    Prompt();
                }
            } break;
            case 0xC: { // Ctrl-L
                fputs("\x1B[2J\x1B[999A", stdout);
                Prompt();
            } break;

            case '\r':
            case '\n': {
                str.Append('\n');

                if (rows > y) {
                    fprintf(stdout, "\x1B[%dB", rows - y);
                }
                fputs("\r\n", stdout);
                y = rows + 1;

                return true;
            } break;

            default: {
                Span<const char> frag;
                if (c == '\t') {
                    frag = "    ";
                } else if (c >= 32 && (uint8_t)c < 128) {
                    frag = c;
                } else {
                    continue;
                }

                str.Grow(frag.len);
                memmove(str.ptr + str_offset + frag.len, str.ptr + str_offset, (size_t)(str.len - str_offset));
                memcpy(str.ptr + str_offset, frag.ptr, (size_t)frag.len);
                str.len += frag.len;
                str_offset += frag.len;

                if (str_offset == str.len && x < columns) {
                    fwrite(frag.ptr, 1, (size_t)frag.len, stdout);
                    x++;
                } else {
                    Prompt();
                }
            } break;
        }
    }

    return true;
}

void ConsolePrompter::Commit()
{
    str.len = TrimStrRight((Span<const char>)str, "\r\n").len;

    if (str.len) {
        std::swap(str, entries[entries.len - 1]);
        entries.AppendDefault();
    }
    entry_idx = entries.len - 1;
    str.RemoveFrom(0);
    str_offset = 0;

    rows = 0;
    rows_with_extra = 0;
    x = 0;
    y = 0;
}

void ConsolePrompter::ChangeEntry(Size new_idx)
{
    if (str.len) {
        std::swap(str, entries[entry_idx]);
    }

    str.RemoveFrom(0);
    str.Append(entries[new_idx]);
    str_offset = str.len;
    entry_idx = new_idx;
}

void ConsolePrompter::Prompt()
{
    columns = GetColumns();

    // Hide cursor during refresh
    fprintf(stdout, "\x1B[?25l");
    if (y) {
        fprintf(stdout, "\x1B[%dA", y);
    }

    // Output prompt(s) and string
    {
        Span<const char> remain = str;
        Span<const char> line;

        rows = -1;
        do {
            line = TrimStrRight(SplitStr(remain, '\n', &remain), "\r\n");

            for (Size i = 0; i <= line.len; i += columns - prompt_columns) {
                Span<const char> part = line.Take(i, std::min((Size)(columns - prompt_columns), line.len - i));

                if (i) {
                    FmtArg prefix = FmtArg(' ').Repeat(prompt_columns - 1);
                    Print("\r\n%!D.+%1%!0 %2\x1B[0K", prefix, part);
                } else if (rows >= 0) {
                    FmtArg prefix = FmtArg('.').Repeat(prompt_columns - 1);
                    Print("\r\n%!D.+%1%!0 %2\x1B[0K", prefix, part);
                } else {
                    Print("\r%!D.+%1%!0%2\x1B[0K", prompt, part);
                }
                rows++;

                Size part_offset = (str.ptr + str_offset) - part.ptr;

                if (part_offset >= 0) {
                    x = (int)(prompt_columns + part_offset);
                    y = rows;
                }
            }
        } while (remain.ptr > line.end());
    }

    // Clear remaining rows
    for (int i = rows; i < rows_with_extra; i++) {
        fprintf(stdout, "\r\n\x1B[0K");
    }
    rows_with_extra = std::max(rows_with_extra, rows);

    // Fix up cursor and show it
    if (rows_with_extra > y) {
        fprintf(stdout, "\x1B[%dA", rows_with_extra - y);
    }
    fprintf(stdout, "\r\x1B[%dC", x);
    fprintf(stdout, "\x1B[?25h");
}

int ConsolePrompter::GetColumns()
{
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(stdout));

    CONSOLE_SCREEN_BUFFER_INFO screen;
    if (GetConsoleScreenBufferInfo(h, &screen))
        return screen.srWindow.Right - screen.srWindow.Left;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) >= 0 && ws.ws_col)
        return ws.ws_col;
#endif

    // Give up!
    return 80;
}

int ConsolePrompter::GetCursorX()
{
    fputs("\x1B[6n", stdout);
    fflush(stdout);

#ifdef _WIN32
    int fd = _fileno(stdin);
#else
    int fd = STDIN_FILENO;
#endif

    char c;
    if (read(fd, &c, 1) != 1 || c != 0x1B)
        return 0;
    if (read(fd, &c, 1) != 1 || c != '[')
        return 0;

    LocalArray<char, 64> buf;
    while (buf.Available() > 1 && read(fd, buf.end(), 1) == 1 && buf.data[buf.len] != 'R') {
        buf.len++;
    }
    buf.Append(0);

    int v, h = 1;
    sscanf(buf.data, "%d;%d", &v, &h);

    return h - 1;
}

int ConsolePrompter::ReadChar()
{
    if (fake_input[0]) {
        int c = fake_input[0];
        fake_input++;
        return c;
    }

#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(stdin));

    for (;;) {
        INPUT_RECORD ev;
        DWORD ev_len;
        if (!ReadConsoleInputA(h, &ev, 1, &ev_len))
            return -1;
        if (!ev_len)
            return -1;

        if (ev.EventType == KEY_EVENT && ev.Event.KeyEvent.bKeyDown) {
            bool ctrl = ev.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
            bool alt = ev.Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);

            if (ctrl && !alt) {
                switch (ev.Event.KeyEvent.wVirtualKeyCode) {
                    case 'A': { return 0x1; } break;
                    case 'B': { return 0x2; } break;
                    case 'C': { return 0x3; } break;
                    case 'D': { return 0x4; } break;
                    case 'E': { return 0x5; } break;
                    case 'F': { return 0x6; } break;
                    case 'H': { return 0x8; } break;
                    case 'K': { return 0xB; } break;
                    case 'L': { return 0xC; } break;
                    case 'N': { return 0xE; } break;
                    case 'P': { return 0x10; } break;
                    case 'T': { return 0x14; } break;
                    case 'U': { return 0x15; } break;
                    case VK_LEFT: {
                        fake_input = "[1;5D";
                        return 0x1B;
                    } break;
                    case VK_RIGHT: {
                        fake_input = "[1;5C";
                        return 0x1B;
                    } break;
                }
            } else {
                switch (ev.Event.KeyEvent.wVirtualKeyCode) {
                    case VK_UP: { return 0x10; } break;
                    case VK_DOWN: { return 0xE; } break;
                    case VK_LEFT: { return 0x2; } break;
                    case VK_RIGHT: { return 0x6; } break;
                    case VK_HOME: { return 0x1; } break;
                    case VK_END: { return 0x5; } break;
                    case VK_RETURN: { return '\r'; } break;
                    case VK_BACK: { return 0x8; } break;
                    case VK_DELETE: {
                        fake_input = "[3~";
                        return 0x1B;
                    } break;

                    default: {
                        if (ev.Event.KeyEvent.uChar.AsciiChar > 0)
                            return ev.Event.KeyEvent.uChar.AsciiChar;
                    } break;
                }
            }
        } else if (ev.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            return 0;
        }
    }
#else
    int c = getc(stdin);
    if (c < 0) {
        if (errno == EINTR) {
            // Could be SIGWINCH, react immediately
            return 0;
        } else {
            return -1;
        }
    }
    return c;
#endif
}

int RunInteractive()
{
    LogInfo("%!R.+blik%!0 %1", FelixVersion);

    WaitForInterruption(0);
    EnableAnsiOutput();

    Program program;
    Parser parser(&program);
    VirtualMachine vm(&program);

    static bool run = true;
    parser.AddFunction("exit()", [](VirtualMachine *vm, Span<const Value> args) {
        run = false;
        vm->SetInterrupt();
        return Value();
    });
    parser.AddFunction("quit()", [](VirtualMachine *vm, Span<const Value> args) {
        run = false;
        vm->SetInterrupt();
        return Value();
    });

    ConsolePrompter prompter;
    ParseReport report;

    while (run && prompter.Read()) {
        // We need to intercept errors in order to hide them in some cases, such as
        // for unexpected EOF because we want to allow the user to add more lines!
        LogTrace trace;
        SetLogHandler([&](LogLevel level, const char *ctx, const char *msg) {
            if (level == LogLevel::Debug) {
                DefaultLogHandler(level, ctx, msg);
            } else {
                trace.Store(level, ctx, msg);
            }
        });
        RG_DEFER_N(try_guard) {
            prompter.Commit();
            trace.Dump();
        };

        TokenizedFile file;
        if (!Tokenize(prompter.str, "<interactive>", &file))
            continue;

        if (!parser.Parse(file, &report)) {
            if (report.unexpected_eof) {
                prompter.str.len = TrimStrRight((Span<const char>)prompter.str).len;
                Fmt(&prompter.str, "\n%1", FmtArg("    ").Repeat(report.depth + 1));

                try_guard.Disable();
            }

            continue;
        }

        if (!vm.Run())
            return 1;

        // Delete the exit for the next iteration :)
        program.ir.RemoveLast(1);
    }

    return 0;
}

}
