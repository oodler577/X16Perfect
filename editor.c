#include <ascii_charmap.h>
#include "document.h"
#include <cbm.h>
#include <cx16.h>
#include <string.h>

#define ROWS 26
#define WIDTH 80
#define NORMAL 0x61
#define HEADER 0xb1
#define CODES 0xb7
#define CURSOR 0x1b
#define TAB 0x09
#define ESC 0x1b

extern void screen_setup(void);
extern unsigned bank_count(void);
extern unsigned char modifiers(void);
extern void __fastcall__ draw_row(unsigned char row, const unsigned char *cells);
extern void __fastcall__ draw_color(unsigned char row, unsigned char column, unsigned char color);

static unsigned char rows[ROWS][WIDTH * 2];
static unsigned char work[WIDTH * 2];
static unsigned char previous_status[WIDTH * 2];
static unsigned char line_bytes[192];
static Pos line_start[ROWS + 1];
static unsigned line_attr[ROWS + 1];
static Pos cursor;
static Pos mark;
static bool blocked, reveal, running = true;
static bool cursor_visible;
static unsigned char cursor_row, cursor_column;
static char filename[21];
static char query[49];
static unsigned char clip[2048];
static unsigned clip_len;
static signed char shift_pending;

static bool insert(const unsigned char *bytes, unsigned count);

static const char banner[] = " X16Perfect | Document";
static const char shortcuts[] = " F3 Commands   F5 Files   F6 Bold   F8 Underline   F10 Save   Alt+F3 Codes";

static void paint(unsigned char *buffer, unsigned char color)
{
    unsigned char x;
    for (x = 0; x < WIDTH * 2; x += 2) {
        buffer[x] = ' ';
        buffer[x + 1] = color;
    }
}

static void text_row(unsigned char row, const char *text, unsigned char color)
{
    unsigned char x;
    paint(work, color);
    for (x = 0; x < WIDTH && text[x]; ++x) work[x * 2] = text[x];
    draw_row(row, work);
}

static void message(const char *text)
{
    text_row(27, text, HEADER);
}

static unsigned char key_wait(void)
{
    unsigned char key;
    do {
        key = cbm_k_getin();
    } while (!key);
    return key;
}

static void put_cell(unsigned char x, unsigned char c, unsigned char color)
{
    work[x * 2] = c;
    work[x * 2 + 1] = color;
}

static const char *code_label(unsigned char c, unsigned char value)
{
    switch (c) {
        case 10: return "[HRt]";
        case 11: return "[SPg]";
        case 12: return "[HPg]";
        case 13: return "[SRt]";
        case 0xc1: return "[Tab/Align]";
        case 0xc3:
            if (value == 12) return "[Bold]";
            if (value == 14) return "[Under]";
            return "[Attr On]";
        case 0xc4:
            if (value == 12) return "[bold]";
            if (value == 14) return "[under]";
            return "[Attr Off]";
        case 0xd0: return "[Layout]";
        case 0xd1: return "[Font]";
        case 0xd5: return "[Header/Footer]";
        case 0xd6: return "[Note]";
        default: return "[Code]";
    }
}

static void cursor_off(void)
{
    if (cursor_visible)
        draw_color(cursor_row + 1, cursor_column, rows[cursor_row][cursor_column * 2 + 1]);
    cursor_visible = false;
}

static void render_from(unsigned char first)
{
    Pos p, first_selected, last_selected, size = doc_size();
    Pos cache_first = size;
    unsigned n = 0, i, length, attr;
    Pos old_next, wrap;
    unsigned wrap_attr;
    unsigned char wrap_x;
    unsigned old_attr;
    signed char shift = shift_pending;
    unsigned char y, x, c, param, color, xstart;
    const char *label;
    bool ends_line;
    shift_pending = 0;
    first_selected = mark < cursor ? mark : cursor;
    last_selected = mark > cursor ? mark : cursor;
    p = line_start[first];
    attr = line_attr[first];
    for (y = first; y < ROWS; ++y) {
        old_next = line_start[y + 1];
        old_attr = line_attr[y + 1];
        line_start[y] = p;
        line_attr[y] = attr;
        paint(work, NORMAL);
        x = wrap_x = 0;
        wrap = p;
        wrap_attr = attr;
        while (p < size && x < WIDTH) {
            if (p < cache_first || p - cache_first >= n) {
                n = doc_copy(p, line_bytes, sizeof(line_bytes));
                cache_first = p;
            }
            i = (unsigned)(p - cache_first);
            c = line_bytes[i];
            param = 0;
            if (c >= 0xc0) {
                length = doc_token(p);
                if (!length) length = 1;
                if (length > 1) param = doc_at(p + 1);
            } else {
                length = 1;
            }
            xstart = x;
            ends_line = c == 10 || c == 12;
            color = attr & (1u << 12) ? 0x67 : NORMAL;
            if (attr & (1u << 14)) color = 0x6d;
            if (blocked && p >= first_selected && p < last_selected) color = 0x16;
            if (c >= 32 && c <= 126) {
                put_cell(x++, c, color);
            } else if (reveal) {
                label = code_label(c, param);
                if (strlen(label) > WIDTH - x && x) break;
                while (*label && x < WIDTH) put_cell(x++, *label++, CODES);
            } else if (c == 0xc1) {
                x = (x | 7) + 1;
            } else if (c == 13 || c == 11 || c == 0xa0) {
                put_cell(x++, ' ', color);
            } else if (c == 0xc0) {
                put_cell(x++, '?', color);
            }
            if (xstart == x && ends_line && p == cursor) put_cell(x, ' ', color);
            if ((c == 0xc3 || c == 0xc4) && length == 3 && param < 16) {
                if (c == 0xc3) attr |= 1u << param;
                else attr &= ~(1u << param);
            }
            p += length;
            if (ends_line) break;
            if (c == ' ' || (!reveal && c == 13)) {
                wrap = p;
                wrap_attr = attr;
                wrap_x = x;
            }
        }
        if (!reveal && x == WIDTH && p < size && wrap > line_start[y] &&
            doc_at(p) > ' ' && doc_at(p - 1) > ' ') {
            x = wrap_x;
            p = wrap;
            attr = wrap_attr;
            while (wrap_x < WIDTH) put_cell(wrap_x++, ' ', NORMAL);
        }
        if (memcmp(rows[y], work, sizeof(work))) {
            memcpy(rows[y], work, sizeof(work));
            draw_row(y + 1, work);
        }
        if (shift && !blocked && old_attr == attr && p == old_next + shift) {
            for (++y; y <= ROWS; ++y) line_start[y] += shift;
            return;
        }
    }
    line_start[ROWS] = p;
    line_attr[ROWS] = attr;
}

static unsigned char locate_row(Pos p)
{
    unsigned char row = 0;
    while (row < ROWS - 1 && line_start[row + 1] <= p &&
           line_start[row + 1] > line_start[row]) ++row;
    if (p == doc_size() && row && line_start[row] == p &&
        doc_at(p - 1) != 10 && doc_at(p - 1) != 12)
        --row;
    return row;
}

static unsigned char column_at(unsigned char row, Pos target)
{
    Pos p = line_start[row];
    Pos end = line_start[row + 1] < target ? line_start[row + 1] : target;
    unsigned char c, x = 0;
    unsigned n, index = 0, count = 0;
    while (p < end && x < WIDTH) {
        if (index >= count) {
            count = doc_copy(p, line_bytes, sizeof(line_bytes));
            index = 0;
        }
        c = line_bytes[index];
        n = c >= 0xc0 ? doc_token(p) : 1;
        if (!n) n = 1;
        if (c >= 32 && c <= 126) ++x;
        else if (reveal) x += strlen(code_label(c, n > 1 ? doc_at(p + 1) : 0));
        else if (c == 0xc1) x = (x | 7) + 1;
        else if (c == 0xc0 || c == 13 || c == 11 || c == 0xa0) ++x;
        p += n;
        if (n >= count - index) index = count;
        else index += n;
    }
    return x < WIDTH ? x : WIDTH - 1;
}

static void show_cursor_at(unsigned char row, unsigned char column)
{
    cursor_row = row;
    cursor_column = column;
    draw_color(cursor_row + 1, cursor_column, CURSOR);
    cursor_visible = true;
}

static void status(void)
{
    static char bar[80];
    strcpy(bar, " ");
    strcat(bar, filename[0] ? filename : "Untitled");
    if (doc_changed) strcat(bar, "   Modified");
    if (reveal) strcat(bar, "   Codes");
    if (doc_readonly) strcat(bar, "   Read only");
    if (blocked) strcat(bar, "   Block");
    if (strcmp((char *)previous_status, bar)) {
        strcpy((char *)previous_status, bar);
        text_row(28, bar, HEADER);
    }
}

static void restore(void)
{
    memset(rows, 0, sizeof(rows));
    previous_status[0] = 0;
    render_from(0);
    text_row(0, banner, HEADER);
    message("");
    status();
    text_row(29, shortcuts, HEADER);
}

static void show_position(void)
{
    if (cursor < line_start[0] || cursor > line_start[ROWS] ||
        (cursor == line_start[ROWS] && cursor < doc_size())) {
        if (cursor > line_start[0] && cursor == line_start[ROWS]) {
            line_start[0] = line_start[ROWS / 2];
            line_attr[0] = line_attr[ROWS / 2];
        } else {
            line_start[0] = cursor;
            line_attr[0] = 0;
            /* Recover inline attributes when jumping outside the viewport. */
            {
                Pos p = 0;
                unsigned n;
                unsigned char code, value;
                while (p < cursor) {
                    code = doc_at(p);
                    n = doc_token(p);
                    if (!n) n = 1;
                    if ((code == 0xc3 || code == 0xc4) && n == 3) {
                        value = doc_at(p + 1);
                        if (value < 16) {
                            if (code == 0xc3) line_attr[0] |= 1u << value;
                            else line_attr[0] &= ~(1u << value);
                        }
                    }
                    p += n;
                }
            }
        }
        render_from(0);
    }
    {
        unsigned char row = locate_row(cursor);
        show_cursor_at(row, column_at(row, cursor));
    }
}

static bool prompt_edit(const char *caption, char *input, unsigned char limit, bool replace_default)
{
    char bar[96];
    unsigned char length = strlen(input);
    unsigned char key;
    for (;;) {
        strcpy(bar, " ");
        strcat(bar, caption);
        strcat(bar, " ");
        strcat(bar, input);
        message(bar);
        key = key_wait();
        if (key == ESC || key == CH_STOP || key == CH_F1) {
            message("");
            return false;
        }
        if (key == CH_ENTER) {
            message("");
            return true;
        }
        if (key == CH_DEL && length) {
            input[--length] = 0;
            replace_default = false;
        } else if (key >= 32 && key <= 126) {
            if (replace_default) length = 0;
            replace_default = false;
            if (length < limit) {
                input[length++] = key;
                input[length] = 0;
            }
        }
    }
}

static bool prompt(const char *caption, char *input, unsigned char limit)
{
    return prompt_edit(caption, input, limit, false);
}

static signed char decision(const char *question)
{
    unsigned char key;
    message(question);
    do {
        key = key_wait();
    } while (key != 'Y' && key != 'y' && key != 'N' && key != 'n' &&
             key != ESC && key != CH_STOP && key != CH_F1 && key != CH_ENTER);
    message("");
    if (key == ESC || key == CH_STOP || key == CH_F1) return -1;
    return key == 'Y' || key == 'y';
}

static bool confirm(const char *question)
{
    return decision(question) == 1;
}

static bool valid_name(const char *name)
{
    unsigned char c;
    if (!*name || !strcmp(name, ".") || !strcmp(name, "..")) return false;
    while ((c = *name++) != 0)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return false;
    return true;
}

static bool save(void)
{
    char name[21], question[40];
    DocSaveResult result;
    strcpy(name, filename);
    if (!prompt("Document to be saved:", name, 20)) return false;
    if (!valid_name(name)) {
        message("Use letters, numbers, dots, underscores or hyphens");
        return false;
    }
    result = doc_save(name, false);
    if (result == DOC_SAVE_EXISTS) {
        strcpy(question, "Replace ");
        strcat(question, name);
        strcat(question, "? No (Yes)");
        if (!confirm(question)) return false;
        result = doc_save(name, true);
    }
    if (result == DOC_SAVE_OK) {
        strcpy(filename, name);
        if (doc_error) message(doc_error);
        else message("Document saved");
        return true;
    } else {
        message(doc_error);
        return false;
    }
}

static void request_exit(void)
{
    signed char answer;
    if (doc_changed) {
        answer = decision("Save document? (N)");
        if (answer < 0 || (answer && !save())) return;
    }
    if (confirm("Exit X16Perfect? (N)")) running = false;
}

static bool retrieve(const char *name)
{
    if (!valid_name(name)) {
        message("Invalid filename");
        return false;
    }
    if (doc_size()) {
        if (!doc_insert_file(name, cursor)) {
            message(doc_error);
            return false;
        }
        render_from(locate_row(cursor));
        return true;
    }
    if (!doc_load(name)) {
        message(doc_error);
        return false;
    }
    strcpy(filename, name);
    cursor = mark = line_start[0] = line_attr[0] = 0;
    blocked = false;
    restore();
    if (doc_readonly) message("Unsupported formatting. Viewing is supported in read-only mode.");
    return true;
}

static void retrieve_prompt(void)
{
    char name[21] = "";
    if (!blocked && prompt("Document to be retrieved:", name, 20) && name[0])
        retrieve(name);
}

static bool valid_pattern(const char *pattern)
{
    char name[21];
    unsigned char i;
    strcpy(name, pattern);
    for (i = 0; name[i]; ++i)
        if (name[i] == '*' || name[i] == '?') name[i] = 'a';
    return valid_name(name);
}

static void list_files(void)
{
    struct cbm_dirent entry;
    static char names[24][17];
    char pattern[21] = "*.*";
    char path[24], title[52];
    unsigned char count = 0, n = 0, k, at;
    if (blocked || !prompt_edit("Dir", pattern, 20, true)) return;
    if (!valid_pattern(pattern)) {
        message("Enter a filename or wildcard for device 8");
        return;
    }
    strcpy(path, "$:");
    strcat(path, strcmp(pattern, "*.*") ? pattern : "*");
    if (cbm_opendir(3, 8, path)) {
        message("Unable to read device 8");
        return;
    }
    while (count < 24 && !cbm_readdir(3, &entry)) {
        if (entry.type == CBM_T_PRG || entry.type == CBM_T_SEQ) {
            at = count++;
            while (at && strcmp(names[at - 1], entry.name) > 0) {
                strcpy(names[at], names[at - 1]);
                --at;
            }
            memcpy(names[at], entry.name, 16);
            names[at][16] = 0;
        }
    }
    cbm_closedir(3);
    strcpy(title, " X16Perfect | Directory ");
    strcat(title, pattern);
    for (;;) {
        text_row(0, title, HEADER);
        for (k = 0; k < ROWS; ++k)
            text_row(k + 1, k < count ? names[k] : "", k == n && count ? CURSOR : NORMAL);
        message(" 1 Retrieve   F1 Cancel");
        k = key_wait();
        if ((k == CH_CURS_UP || k == CH_CURS_LEFT) && n) --n;
        if ((k == CH_CURS_DOWN || k == CH_CURS_RIGHT) && n + 1 < count) ++n;
        if (k == CH_F1 || k == CH_STOP || k == ESC) {
            restore();
            return;
        }
        if (k == '1' && count &&
            (!doc_size() || confirm("Retrieve into current document? No (Yes)"))) {
            restore();
            retrieve(names[n]);
            return;
        }
    }
}

static bool find_forward(void)
{
    unsigned char n, matching;
    Pos p, q, top = doc_size();

    n = strlen(query);
    if (!n) return false;
    message("Searching. F1 cancels.");
    for (p = cursor; p < top;) {
        q = p;
        matching = 0;
        while (matching < n && q < top && doc_at(q) == query[matching]) {
            ++matching;
            ++q;
        }
        if (matching == n) {
            cursor = p;
            message("Found");
            return true;
        }
        matching = doc_token(p);
        p += matching ? matching : 1;
        if (cbm_k_getin() == CH_F1) {
            message("Search canceled");
            return false;
        }
    }
    message("No match");
    return false;
}

static bool search(bool replace)
{
    char replacement[49] = "";
    if (!prompt(replace ? "Find text to replace" : "Search forward", query, 48))
        return false;
    if (replace && !prompt("Replace with", replacement, 48)) return false;
    if (!find_forward() || !replace) return false;
    if (doc_replace(cursor, cursor + strlen(query), replacement, strlen(replacement))) {
        message("Replaced one match");
        return true;
    }
    message("Replacement could not be applied");
    return false;
}

static void output_menu(bool setup)
{
    unsigned char key, selected = 0, i, count;
    char argument[21];
    static const char *const printing[] = {
        "1  Full Document", "2  Page", "3  Document on Disk", "4  Control Printer",
        "5  Multiple Pages", "6  View Document", "7  Initialize Printer",
        "S  Select Printer", "B  Binding Offset", "N  Number of Copies",
        "U  Multiple Copies", "G  Graphics Quality", "T  Text Quality"
    };
    static const char *const printer[] = {
        "1  Select Printer", "2  Add Printer", "3  Edit Printer",
        "4  Delete Printer", "5  Set Port"
    };
    static const char print_keys[] = "1234567sbnugt";
    const char *const *choices = setup ? printer : printing;
    count = setup ? sizeof(printer) / sizeof(printer[0]) :
                    sizeof(printing) / sizeof(printing[0]);
    for (;;) {
        text_row(0, setup ? " X16Perfect | Printer setup" : " X16Perfect | Print", HEADER);
        for (i = 0; i < ROWS; ++i)
            text_row(i + 1, i < count ? choices[i] : (const char *)"", i == selected ? CURSOR : NORMAL);
        message(" Select with a key or the arrows. F1 or 0 exits.");
        key = key_wait();
        if (key == CH_F1 || key == CH_STOP || key == ESC || key == '0') break;
        if (key == CH_CURS_UP && selected) --selected;
        if (key == CH_CURS_DOWN && selected + 1 < count) ++selected;
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
        if (key != CH_ENTER) {
            for (i = 0; i < count; ++i)
                if (key == (setup ? '1' + i : print_keys[i])) break;
            if (i == count) continue;
            selected = i;
        }
        if (!setup && (selected == 7 || selected == 3)) {
            output_menu(true);
        } else if (!setup && (selected == 2 || selected == 4 ||
                             (selected >= 8 && selected <= 9))) {
            argument[0] = 0;
            if (prompt(choices[selected], argument, 20) &&
                confirm("Confirm this choice? (N)")) break;
        } else if (confirm(setup ? "Confirm setup choice? (N)" : "Confirm print choice? (N)")) {
            break;
        }
    }
    restore();
}

static void put_word(unsigned char *where, unsigned value)
{
    where[0] = (unsigned char)value;
    where[1] = (unsigned char)(value >> 8);
}

static bool margin_value(const char *label, unsigned *value)
{
    char input[5] = "";
    unsigned result = 0;
    const char *digit;
    if (!prompt(label, input, 4) || !*input) return false;
    for (digit = input; *digit; ++digit) {
        if (*digit < '0' || *digit > '9') {
            message("Enter a whole number from 0 to 6000");
            return false;
        }
        result = result * 10 + *digit - '0';
    }
    if (result > 6000) {
        message("Margin exceeds 5 inches");
        return false;
    }
    *value = result;
    return true;
}

static void layout(void)
{
    unsigned first, second;
    unsigned char key;
    unsigned char code[16] = {0xd0, 1, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 0, 1, 0xd0};
    unsigned char page = 12;
    message(" H Left/right margins   V Top/bottom margins   B Hard page break   F1 Cancel");
    key = key_wait();
    message("");
    if (key == 'b' || key == 'B') {
        insert(&page, 1);
    } else if (key == 'h' || key == 'H' || key == 'v' || key == 'V') {
        bool vertical = key == 'v' || key == 'V';
        if (!margin_value(vertical ? "Top margin (1200ths of an inch)" :
                                     "Left margin (1200ths of an inch)", &first) ||
            !margin_value(vertical ? "Bottom margin (1200ths of an inch)" :
                                     "Right margin (1200ths of an inch)", &second))
            return;
        if (vertical) code[1] = code[14] = 5;
        put_word(code + 8, first);
        put_word(code + 10, second);
        if (insert(code, sizeof(code))) message("Margins recorded in document");
    }
}

static void commands(void)
{
    unsigned char key, row;
    static const char *const items[] = {
        " F2 Search                 Shift+F10 Retrieve",
        " F4 Indent                 Alt+F4 Block",
        " F5 List Files             F6 Bold",
        " F7 Exit                   F8 Underline",
        " F10 Save                  Alt+F3 Reveal Codes",
        " Ctrl+C Copy block         Ctrl+X Cut block",
        " Ctrl+V Paste block",
        " P Print                   S Printer Setup",
        " N New Document            L Layout"
    };
    text_row(0, " X16Perfect | Commands", HEADER);
    for (row = 0; row < ROWS; ++row)
        text_row(row + 1, row < sizeof(items) / sizeof(items[0]) ? items[row] : (const char *)"", NORMAL);
    message(" P Print   S Printer setup   R Retrieve   N New   F1 Cancel");
    key = key_wait();
    restore();
    if (key == 'P' || key == 'p') output_menu(false);
    if (key == 'L' || key == 'l') { layout(); render_from(0); }
    if (key == 'S' || key == 's') output_menu(true);
    if (key == 'R' || key == 'r') retrieve_prompt();
    if ((key == 'N' || key == 'n') &&
        (!doc_changed || confirm("Discard edits to current document? (N)"))) {
        doc_clear();
        cursor = mark = line_start[0] = line_attr[0] = 0;
        blocked = false;
        filename[0] = 0;
        restore();
    }
}

static bool insert(const unsigned char *bytes, unsigned count)
{
    if (!doc_insert(cursor, bytes, count)) {
        message(doc_readonly ? "Document is read-only" : "Insufficient document memory");
        return false;
    }
    cursor += count;
    return true;
}

static bool append_visible(unsigned char key, unsigned char row)
{
    unsigned char col = cursor_column;
    unsigned char i, color;
    unsigned attr;
    if (blocked || reveal || doc_readonly || cursor != doc_size() ||
        col >= WIDTH - 2 || row != cursor_row)
        return false;
    if (!doc_insert(cursor, &key, 1)) return false;
    ++cursor;
    for (i = row + 1; i <= ROWS; ++i) ++line_start[i];
    attr = line_attr[ROWS];
    color = NORMAL;
    if (attr & (1u << 12)) color = 0x67;
    if (attr & (1u << 14)) color = 0x6d;
    rows[row][2 * col] = key;
    rows[row][2 * col + 1] = color;
    draw_row(row + 1, rows[row]);
    show_cursor_at(row, col + 1);
    status();
    return true;
}

static void attribute(unsigned char value)
{
    unsigned char code[] = {0xc3, 0, 0xc3};
    Pos p = line_start[locate_row(cursor)];
    unsigned n;
    bool active = (line_attr[locate_row(cursor)] & (1u << value)) != 0;
    while (p < cursor) {
        n = doc_token(p);
        if (n == 3 && doc_at(p + 1) == value)
            active = doc_at(p) == 0xc3;
        p += n ? n : 1;
    }
    if (active) code[0] = code[2] = 0xc4;
    code[1] = value;
    insert(code, sizeof(code));
}

static void block_copy(bool cut)
{
    Pos start, end, p;
    unsigned n;
    if (!blocked) return;
    start = mark < cursor ? mark : cursor;
    end = mark > cursor ? mark : cursor;
    if (end - start > sizeof(clip)) {
        message("Block exceeds clipboard capacity");
        return;
    }
    p = start;
    while (p < end) {
        n = doc_copy(p, clip + (unsigned)(p - start), (unsigned)(end - p));
        if (!n) return;
        p += n;
    }
    clip_len = (unsigned)(end - start);
    if (cut && !doc_readonly) {
        doc_delete(start, end);
        cursor = start;
    }
    blocked = false;
}

static void vertical(signed char delta)
{
    unsigned char row = locate_row(cursor);
    unsigned char column = column_at(row, cursor);
    Pos next;
    unsigned n;
    if (delta < 0 && !row) {
        if (line_start[0]) cursor = doc_previous(line_start[0]);
        return;
    }
    if (delta > 0 && row == ROWS - 1) {
        cursor = line_start[ROWS];
        return;
    }
    row += delta;
    cursor = line_start[row];
    while (cursor < line_start[row + 1]) {
        n = doc_token(cursor);
        next = cursor + (n ? n : 1);
        if (next >= line_start[row + 1] || column_at(row, next) > column) break;
        cursor = next;
    }
}

int main(void)
{
    static const unsigned char tab[] = {0xc1, 2, 0, 0, 0, 0, 0, 0, 0xc1};
    unsigned char key, mod, repaint, first;
    unsigned n;
    screen_setup();
    doc_init(bank_count());
    restore();
    show_position();

    while (running) {
        key = key_wait();
        mod = modifiers();
        cursor_off();
        first = locate_row(cursor);
        repaint = blocked ? 1 : 0;
        switch (key) {
            case CH_F11:
                if (mod & 4) {
                    if (clip_len) insert(clip, clip_len);
                    repaint = 1;
                    break;
                }
                /* Fall through to Reveal Codes. */
            case CH_F3:
                if (key == CH_F11 || (mod & 2)) {
                    reveal = !reveal;
                    first = 0;
                    repaint = 1;
                } else commands();
                break;
            case CH_F5: list_files(); break;
            case CH_F6: attribute(12); repaint = 1; break;
            case CH_F8: attribute(14); repaint = 1; break;
            case CH_F10:
                if (mod & 1) retrieve_prompt();
                else save();
                break;
            case CH_F7:
                if (mod & 1) output_menu(false);
                else request_exit();
                break;
            case CH_F1:
                if (mod & 1) output_menu(true);
                break;
            case ESC: if (blocked) { blocked = false; first = 0; repaint = 1; } message(""); break;
            case CH_F2:
                if (search((mod & 1) != 0)) {
                    first = 0;
                    repaint = 1;
                }
                break;
            case CH_F4:
                if (mod & 2) {
                    blocked = !blocked;
                    mark = cursor;
                } else insert(tab, sizeof(tab));
                repaint = 1;
                break;
            case CH_F12: blocked = !blocked; mark = cursor; repaint = 1; break;
            case CH_CURS_LEFT: cursor = doc_previous(cursor); break;
            case CH_CURS_RIGHT:
                if (cursor < doc_size()) {
                    n = doc_token(cursor);
                    cursor += n ? n : 1;
                }
                break;
            case CH_CURS_UP:
                if (mod & 4) cursor = 0;
                else vertical(-1);
                break;
            case CH_CURS_DOWN:
                if (mod & 4) cursor = line_start[ROWS];
                else vertical(1);
                break;
            case CH_HOME: cursor = line_start[first]; break;
            case CH_DEL:
                if (cursor && !doc_readonly) {
                    n = (unsigned)(cursor - doc_previous(cursor));
                    doc_delete(cursor - n, cursor);
                    cursor -= n;
                    if (first) --first;
                    repaint = 1;
                }
                break;
            case CH_ENTER:
                key = mod & 4 ? 12 : 10;
                insert(&key, 1);
                repaint = 1;
                break;
            case TAB: insert(tab, sizeof(tab)); repaint = 1; break;
            case 3: if (blocked) block_copy(false); repaint = 1; break;
            case 24: if (blocked) block_copy(true); first = 0; repaint = 1; break;
            default:
                if (key >= 32 && key <= 126) {
                    if (append_visible(key, first)) continue;
                    if (insert(&key, 1)) shift_pending = 1;
                    repaint = 1;
                }
                break;
        }
        if (repaint) {
            if (cursor < line_start[0]) {
                line_start[0] = cursor;
                first = 0;
            }
            render_from(blocked ? 0 : first);
        }
        if (running) {
            status();
            show_position();
        }
    }
    cbm_k_bsout(0x93);
    return 0;
}
