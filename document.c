#include <ascii_charmap.h>
#include "document.h"
#include <cbm.h>
#include <string.h>

#define BANK (*(volatile unsigned char *)0)
#define WINDOW ((unsigned char *)0xa000)
#define BANK_SIZE 8192u
#define MIN_FLOOR (2ul * BANK_SIZE)
#define CHANNEL 2
#define DRIVE 8

static Pos floor, ceiling, left, right;
static unsigned char io[512];
static unsigned char header[16];
static unsigned prefix_size;
static const unsigned char minimal_prefix[76] = {
    0xff, 'W', 'P', 'C', 76, 0, 0, 0, 1, 10, 0, 0, 0, 0, 0, 0,
    0xfb, 0xff, 5, 0, 50, 0, 0, 0, 0, 0, 6, 0, 8, 0, 0, 0,
    66, 0, 0, 0, 8, 0, 2, 0, 0, 0, 74, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 8, 0, 0x7c, 0, 0x78, 0, 0, 0, 0, 0
};

bool doc_readonly;
bool doc_changed;
const char *doc_error;

extern int __fastcall__ read_block(unsigned char count, void *target);
extern int __fastcall__ write_block(unsigned char count, const void *source);

static int read_data(void *data, unsigned count)
{
    int n;
    unsigned char flags;
    if (cbm_k_chkin(CHANNEL)) return -1;
    flags = cbm_k_readst();
    if (flags) {
        cbm_k_clrch();
        return flags == 64 ? 0 : -1;
    }
    if (count > 255 && count < 512) count = 255;
    n = read_block((unsigned char)count, data);
    flags = cbm_k_readst();
    cbm_k_clrch();
    if (n < 0) return cbm_read(CHANNEL, data, count);
    if (flags & 0xbf || n > (int)count) return -1;
    return n;
}

static bool write_data(const unsigned char *data, unsigned count)
{
    unsigned chunk;
    unsigned char flags;
    int n;
    if (cbm_k_ckout(CHANNEL)) return false;
    while (count) {
        chunk = count > 256 ? 256 : count;
        n = write_block((unsigned char)chunk, data);
        flags = cbm_k_readst();
        if (n < 0) {
            cbm_k_clrch();
            return cbm_write(CHANNEL, data, count) == (int)count;
        }
        if (flags || !n || n > (int)chunk) {
            cbm_k_clrch();
            return false;
        }
        data += n;
        count -= n;
    }
    cbm_k_clrch();
    return true;
}

static void file_name(char *target, const char *name, char mode)
{
    target[0] = '0';
    target[1] = ':';
    strcpy(target + 2, name);
    strcat(target, ",P,");
    target[strlen(target) + 1] = 0;
    target[strlen(target)] = mode;
}

static unsigned char disk_command(const char *command)
{
    char status[32];
    int count;
    BANK = 1;
    if (cbm_open(15, DRIVE, 15, command)) return 99;
    count = cbm_read(15, status, sizeof(status));
    cbm_close(15);
    if (count < 2 || status[0] < '0' || status[0] > '9' ||
        status[1] < '0' || status[1] > '9')
        return 99;
    return (status[0] - '0') * 10 + status[1] - '0';
}

static unsigned bank_span(Pos address)
{
    return BANK_SIZE - ((unsigned)address & (BANK_SIZE - 1));
}

static Pos body_floor(unsigned prefix)
{
    Pos prefix_banks_end = BANK_SIZE + (((Pos)prefix + BANK_SIZE - 1) & ~(Pos)(BANK_SIZE - 1));
    return prefix_banks_end > MIN_FLOOR ? prefix_banks_end : MIN_FLOOR;
}

static void read_mem(Pos source, void *target, unsigned count)
{
    BANK = (unsigned char)(source >> 13);
    memcpy(target, WINDOW + ((unsigned)source & (BANK_SIZE - 1)), count);
}

static void write_mem(Pos target, const void *source, unsigned count)
{
    BANK = (unsigned char)(target >> 13);
    memcpy(WINDOW + ((unsigned)target & (BANK_SIZE - 1)), source, count);
}

static void move_mem(Pos target, Pos source, unsigned count)
{
    if ((source >> 13) == (target >> 13)) {
        BANK = (unsigned char)(source >> 13);
        memmove(WINDOW + ((unsigned)target & (BANK_SIZE - 1)),
                WINDOW + ((unsigned)source & (BANK_SIZE - 1)), count);
    } else {
        read_mem(source, io, count);
        write_mem(target, io, count);
    }
}

static void move_gap(Pos position)
{
    Pos destination;
    Pos source;
    unsigned count;
    unsigned limit;
    while (left < position) {
        count = sizeof(io);
        if (position - left < count) count = (unsigned)(position - left);
        if (bank_span(left) < count) count = bank_span(left);
        if (bank_span(right) < count) count = bank_span(right);
        move_mem(left, right, count);
        left += count;
        right += count;
    }
    while (left > position) {
        count = sizeof(io);
        if (left - position < count) count = (unsigned)(left - position);
        limit = ((unsigned)(left - 1) & (BANK_SIZE - 1)) + 1;
        if (limit < count) count = limit;
        limit = ((unsigned)(right - 1) & (BANK_SIZE - 1)) + 1;
        if (limit < count) count = limit;
        destination = right - count;
        source = left - count;
        move_mem(destination, source, count);
        left = source;
        right = destination;
    }
}

void doc_init(unsigned banks)
{
    /* The second half is a staging area for opening files transactionally. */
    ceiling = (Pos)banks * BANK_SIZE / 2;
    doc_clear();
}

void doc_clear(void)
{
    left = floor = MIN_FLOOR;
    right = ceiling;
    prefix_size = 0;
    doc_readonly = false;
    doc_changed = false;
}

Pos doc_size(void)
{
    return left - floor + ceiling - right;
}

unsigned char doc_at(Pos position)
{
    Pos address = position + floor;
    if (address >= left) address += right - left;
    BANK = (unsigned char)(address >> 13);
    return WINDOW[(unsigned)address & (BANK_SIZE - 1)];
}

unsigned doc_copy(Pos position, void *destination, unsigned count)
{
    Pos address = position + floor;
    Pos available;
    if (address >= left) {
        address += right - left;
        available = ceiling - address;
    } else {
        available = left - address;
    }
    if (available < count) count = (unsigned)available;
    if (bank_span(address) < count) count = bank_span(address);
    if (count) read_mem(address, destination, count);
    return count;
}

bool doc_insert(Pos position, const void *source, unsigned count)
{
    const unsigned char *bytes = source;
    unsigned n;
    if (doc_readonly || position > doc_size() || right - left < count)
        return false;
    move_gap(position + floor);
    while (count) {
        n = bank_span(left);
        if (n > count) n = count;
        write_mem(left, bytes, n);
        left += n;
        bytes += n;
        count -= n;
    }
    doc_changed = true;
    return true;
}

void doc_delete(Pos first, Pos last)
{
    if (doc_readonly || first >= last || last > doc_size()) return;
    move_gap(first + floor);
    right += last - first;
    doc_changed = true;
}

bool doc_replace(Pos first, Pos last, const void *bytes, unsigned count)
{
    if (doc_readonly || first > last || last > doc_size() ||
        right - left + last - first < count)
        return false;
    doc_delete(first, last);
    return !count || doc_insert(first, bytes, count);
}

unsigned doc_token(Pos p)
{
    unsigned char value = doc_at(p);
    unsigned length;
    static const unsigned char sizes[] = {4, 9, 11, 3, 3, 5, 6, 7, 4, 5, 6, 6, 8, 10, 10, 12};
    if (value < 0xc0) return 1;
    if (value < 0xd0)
        length = sizes[value - 0xc0];
    else {
        if (doc_size() - p < 8) return 0;
        length = doc_at(p + 2) | ((unsigned)doc_at(p + 3) << 8);
        if (length < 4 || length > 65531u) return 0;
        length += 4;
    }
    if (doc_size() - p < length || doc_at(p + length - 1) != value) return 0;
    if (value >= 0xd0 &&
        (doc_at(p + 1) != doc_at(p + length - 2) ||
         doc_at(p + 2) != doc_at(p + length - 4) ||
         doc_at(p + 3) != doc_at(p + length - 3))) return 0;
    return length;
}

Pos doc_previous(Pos position)
{
    unsigned length;
    unsigned value;
    if (!position) return 0;
    value = doc_at(position - 1);
    if (value < 0xc0) return position - 1;
    if (value < 0xd0) {
        for (length = 3; length <= 12 && length <= position; ++length)
            if (doc_at(position - length) == value && doc_token(position - length) == length)
                return position - length;
    } else if (position >= 8) {
        length = doc_at(position - 4) | ((unsigned)doc_at(position - 3) << 8);
        if (length >= 4 && length <= 65531u) {
            length += 4;
            if (length <= position && doc_token(position - length) == length)
                return position - length;
        }
    }
    return position - 1;
}

static bool validate_body(void)
{
    Pos p = 0, size = doc_size();
    unsigned n, available, index;
    unsigned char c;
    while (p < size) {
        available = doc_copy(p, io, sizeof(io));
        if (!available) return false;
        index = 0;
        while (index < available) {
            c = io[index];
            if (c >= 32 && c <= 126) {
                ++index;
                continue;
            }
            if (c >= 0xc0) {
                n = doc_token(p + index);
                if (!n) return false;
                if (c != 0xc3 && c != 0xc4 &&
                    !(c == 0xc1 && !(doc_at(p + index + 1) & 0xf4)) &&
                    !(c == 0xd0 && n == 16 &&
                      (doc_at(p + index + 1) == 1 || doc_at(p + index + 1) == 5)))
                    doc_readonly = true;
                if (n > available - index) {
                    p += index + n;
                    index = 0;
                    break;
                }
                index += n;
                continue;
            }
            if (c != 10 && c != 11 && c != 12 && c != 13 && c != 0xa0)
                doc_readonly = true;
            ++index;
        }
        p += index;
    }
    return true;
}

static void transfer(Pos target, Pos source, Pos count)
{
    unsigned n;
    while (count) {
        n = sizeof(io);
        if (count < n) n = (unsigned)count;
        if (bank_span(source) < n) n = bank_span(source);
        if (bank_span(target) < n) n = bank_span(target);
        move_mem(target, source, n);
        count -= n;
        target += n;
        source += n;
    }
}

static bool append_staged(Pos *end, Pos limit, const unsigned char *data, unsigned size)
{
    unsigned span;
    if (limit - *end < size) return false;
    while (size) {
        span = bank_span(*end);
        if (span > size) span = size;
        write_mem(*end, data, span);
        *end += span;
        data += span;
        size -= span;
    }
    return true;
}

static bool stage_text(int initial, Pos *body_size)
{
    static unsigned char output[256];
    static const unsigned char tab[] = {0xc1, 2, 0, 0, 0, 0, 0, 0, 0xc1};
    const unsigned char *data = header;
    Pos end = ceiling, limit = ceiling + (ceiling - MIN_FLOOR);
    bool after_cr = false, after_eof = false;
    int size = initial, i;
    unsigned used = 0;
    unsigned char c;

    for (;;) {
        doc_error = "Unsupported file format";
        for (i = 0; i < size; ++i) {
            c = data[i];
            if (after_eof) return false;
            if (c == 0x1a) {
                after_eof = true;
                continue;
            }
            if (c == 10 && after_cr) {
                after_cr = false;
                continue;
            }
            after_cr = c == 13;
            if (after_cr) c = 10;
            if (c == 9) {
                memcpy(output + used, tab, sizeof(tab));
                used += sizeof(tab);
            } else {
                if (c != 10 && c != 12 && (c < 32 || c > 126)) return false;
                output[used++] = c;
            }
            if (used > sizeof(output) - sizeof(tab)) {
                doc_error = "Document exceeds capacity";
                if (!append_staged(&end, limit, output, used)) return false;
                used = 0;
            }
        }
        BANK = 1;
        size = read_data(io, sizeof(io));
        if (size < 0) {
            doc_error = "Read error";
            return false;
        }
        if (!size) break;
        data = io;
    }

    doc_error = "Document exceeds capacity";
    if (!append_staged(&end, limit, output, used)) return false;
    *body_size = end - ceiling;
    return true;
}

static bool stage_document(const char *name, unsigned *prefix_size_out,
                           Pos *body_size_out, bool *readonly_out)
{
    char path[28];
    unsigned prefix, space;
    int n;
    unsigned char opened, status;
    Pos end, stage = ceiling, candidate_floor;
    Pos old_floor = floor, old_left = left, old_right = right, old_ceiling = ceiling;
    bool old_readonly = doc_readonly;
    doc_error = "Cannot open document";
    file_name(path, name, 'R');
    BANK = 1;
    opened = cbm_open(CHANNEL, DRIVE, CBM_READ, path);
    status = disk_command("");
    if (opened) return false;
    if (status) goto fail;
    doc_error = "Unsupported or invalid WordPerfect file";
    n = cbm_read(CHANNEL, header, sizeof(header));
    if (n < 0) goto fail;
    if (n < 4 || memcmp(header, "\xffWPC", 4)) {
        bool valid = stage_text(n, body_size_out);
        cbm_close(CHANNEL);
        *prefix_size_out = 0;
        *readonly_out = false;
        return valid;
    }
    if (n != sizeof(header) || header[6] || header[7] || header[8] != 1 || header[9] != 10 ||
        header[10] || header[11] > 1 || header[12] || header[13])
        goto fail;
    prefix = header[4] | ((unsigned)header[5] << 8);
    if (prefix < sizeof(header)) goto fail;
    candidate_floor = body_floor(prefix);
    if (candidate_floor > ceiling) goto fail;
    write_mem(stage, header, sizeof(header));
    end = stage + sizeof(header);
    doc_error = "Read error or document exceeds capacity";
    for (;;) {
        space = bank_span(end);
        if (space > sizeof(io)) space = sizeof(io);
        BANK = 1;
        n = read_data(io, space);
        if (n < 0) goto fail;
        if (!n) break;
        if (end + n > stage + ceiling - candidate_floor + prefix) goto fail;
        write_mem(end, io, n);
        end += n;
    }
    cbm_close(CHANNEL);
    if (end < stage + prefix) {
        doc_error = "Truncated WordPerfect prefix";
        return false;
    }

    floor = stage + prefix;
    left = right = ceiling = end;
    doc_readonly = false;
    {
        bool valid = validate_body();
        *readonly_out = doc_readonly;
        doc_readonly = old_readonly;
        floor = old_floor;
        left = old_left;
        right = old_right;
        ceiling = old_ceiling;
        if (!valid) {
            doc_error = "Malformed WordPerfect document codes";
            return false;
        }
    }
    *prefix_size_out = prefix;
    *body_size_out = end - stage - prefix;
    return true;

fail:
    cbm_close(CHANNEL);
    return false;
}

bool doc_load(const char *name)
{
    unsigned prefix;
    Pos size;
    bool readonly;
    if (!stage_document(name, &prefix, &size, &readonly)) return false;
    transfer(BANK_SIZE, ceiling, prefix);
    floor = body_floor(prefix);
    transfer(floor, ceiling + prefix, size);
    left = floor + size;
    right = ceiling;
    prefix_size = prefix;
    doc_readonly = readonly;
    doc_changed = false;
    doc_error = 0;
    return true;
}

bool doc_insert_file(const char *name, Pos position)
{
    unsigned prefix;
    Pos size;
    bool readonly;

    doc_error = "Document is read-only";
    if (doc_readonly) return false;
    doc_error = "Invalid insertion position";
    if (position > doc_size()) return false;
    if (!stage_document(name, &prefix, &size, &readonly)) return false;
    doc_error = "File uses formatting that cannot be safely inserted";
    if (readonly) return false;
    doc_error = "Insufficient document memory";
    if (size > right - left) return false;
    if (size) {
        move_gap(floor + position);
        transfer(left, ceiling + prefix, size);
        left += size;
        doc_changed = true;
    }
    doc_error = 0;
    return true;
}

static unsigned char rename_file(const char *from, const char *to)
{
    char command[46];
    strcpy(command, "R0:");
    strcat(command, to);
    strcat(command, "=");
    strcat(command, from);
    return disk_command(command);
}

static unsigned char scratch_file(const char *name)
{
    char command[28];
    strcpy(command, "S0:");
    strcat(command, name);
    return disk_command(command);
}

static bool name_available(const char *name)
{
    char path[28];
    unsigned char opened, status;
    file_name(path, name, 'R');
    BANK = 1;
    opened = cbm_open(CHANNEL, DRIVE, CBM_READ, path);
    status = disk_command("");
    if (!opened) cbm_close(CHANNEL);
    return status == 62;
}

static DocSaveResult write_new(const char *name, bool temporary)
{
    char path[28];
    Pos p = 0;
    unsigned n, prefix_pos = 0;
    unsigned char opened, status;
    bool ok = true;
    doc_error = "Cannot create file or name already exists";
    file_name(path, name, 'W');
    BANK = 1;
    opened = cbm_open(CHANNEL, DRIVE, CBM_WRITE, path);
    status = disk_command("");
    if (opened || status) {
        if (!opened) cbm_close(CHANNEL);
        return status == 63 ? DOC_SAVE_EXISTS : DOC_SAVE_FAILED;
    }

    if (!prefix_size) {
        ok = write_data(minimal_prefix, sizeof(minimal_prefix));
    } else {
        while (ok && prefix_pos < prefix_size) {
            n = prefix_size - prefix_pos;
            if (n > sizeof(io)) n = sizeof(io);
            read_mem(BANK_SIZE + prefix_pos, io, n);
            BANK = 1;
            ok = write_data(io, n);
            prefix_pos += n;
        }
    }
    while (ok && p < doc_size()) {
        n = doc_copy(p, io, sizeof(io));
        BANK = 1;
        ok = n && write_data(io, n);
        p += n;
    }
    cbm_close(CHANNEL);
    status = disk_command("");
    if (ok && status == 0) return DOC_SAVE_OK;
    if (temporary) {
        scratch_file(name);
        doc_error = "Write failed. Original file kept.";
    } else {
        doc_error = "Write failed. Check disk and partial new file.";
    }
    return DOC_SAVE_FAILED;
}

static bool can_replace(const char *name)
{
    char pattern[25];
    struct cbm_dirent entry;
    unsigned char result;
    strcpy(pattern, "$:");
    strcat(pattern, name);
    BANK = 1;
    if (cbm_opendir(CHANNEL, DRIVE, pattern)) return false;
    do {
        result = cbm_readdir(CHANNEL, &entry);
    } while (!result && entry.type == CBM_T_HEADER);
    cbm_closedir(CHANNEL);
    return !result && (entry.type == CBM_T_PRG || entry.type == CBM_T_SEQ)
        && entry.access != CBM_A_RO;
}

static DocSaveResult replace_file(const char *name)
{
    static char recovery[78];
    char temporary[] = "X16P00.TMP";
    char backup[] = "X16P00.BAK";
    unsigned char slot, status;
    DocSaveResult result;

    if (!can_replace(name)) {
        doc_error = "Cannot replace this file. Original kept.";
        return DOC_SAVE_FAILED;
    }

    /* A same-directory temporary file must be closed successfully first. */
    for (slot = 0; slot < 100; ++slot) {
        temporary[4] = backup[4] = '0' + slot / 10;
        temporary[5] = backup[5] = '0' + slot % 10;
        if (!name_available(backup)) continue;
        result = write_new(temporary, true);
        if (result == DOC_SAVE_OK) break;
        if (result != DOC_SAVE_EXISTS) return result;
    }
    if (slot == 100) {
        doc_error = "No free temporary and backup name. Original file kept.";
        return DOC_SAVE_FAILED;
    }

    /* HostFS can replace a rename destination. Check the backup name again. */
    if (!name_available(backup) || rename_file(name, backup)) {
        scratch_file(temporary);
        doc_error = "Could not move original to backup. Replacement canceled.";
        return DOC_SAVE_FAILED;
    }

    if (!name_available(name) || rename_file(temporary, name)) {
        if (!name_available(name) || rename_file(backup, name)) {
            strcpy(recovery, "Save failed. Original: ");
            strcat(recovery, backup);
            strcat(recovery, "  New copy: ");
            strcat(recovery, temporary);
            doc_error = recovery;
        } else {
            scratch_file(temporary);
            doc_error = "Replacement failed. Original file restored.";
        }
        return DOC_SAVE_FAILED;
    }

    status = scratch_file(backup);
    if (status != 1 && status != 0) {
        strcpy(recovery, "Document saved. Backup kept: ");
        strcat(recovery, backup);
        doc_error = recovery;
    } else {
        doc_error = 0;
    }
    return DOC_SAVE_OK;
}

DocSaveResult doc_save(const char *name, bool replace_existing)
{
    DocSaveResult result;
    doc_error = "Document is read-only";
    if (doc_readonly) return DOC_SAVE_FAILED;
    if (replace_existing) {
        result = replace_file(name);
    } else {
        result = write_new(name, false);
        if (result == DOC_SAVE_OK) doc_error = 0;
    }
    if (result == DOC_SAVE_OK) doc_changed = false;
    return result;
}
