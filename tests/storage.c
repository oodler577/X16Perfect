#include <ascii_charmap.h>
#include <cbm.h>
#include <stdlib.h>
#include <string.h>
#include "../document.h"

extern unsigned bank_count(void);
extern unsigned ticks(void);

static void line(const char *s)
{
    while (*s) cbm_k_bsout(*s++);
    cbm_k_bsout(13);
}

static void measurement(const char *label, unsigned start)
{
    char output[80];
    strcpy(output, label);
    strcat(output, " ");
    utoa((unsigned)(ticks() - start), output + strlen(output), 10);
    strcat(output, " ticks");
    line(output);
}

int main(void)
{
    static const unsigned char marker[] = {'X','1','6'};
    static const unsigned char needle[] = "unique phrase";
    unsigned start, count, index, found = 0;
    static unsigned char chunk[256];
    Pos p = 0, size;
    cbm_k_bsout(0x0f);
    doc_init(bank_count());
    start = ticks();
    if (!doc_load("BIG.WPD")) {
        line(doc_error);
        return 1;
    }
    measurement("OPEN", start);
    size = doc_size();
    if (size <= 8192 || doc_readonly) {
        line("Bad size or read only");
        return 2;
    }
    start = ticks();
    while (p < size) {
        count = doc_copy(p, chunk, sizeof(chunk));
        for (index = 0; index < count; ++index) {
            if (chunk[index] == needle[found]) {
                if (++found == sizeof(needle) - 1) break;
            } else {
                found = chunk[index] == needle[0];
            }
        }
        if (found == sizeof(needle) - 1) break;
        p += count;
    }
    if (found != sizeof(needle) - 1) {
        line("Search marker unavailable");
        return 3;
    }
    measurement("STREAM SCAN", start);
    if (!doc_insert(8191, marker, sizeof(marker)) || !doc_insert(0, marker, sizeof(marker))) {
        line("Insertion failed");
        return 4;
    }
    start = ticks();
    if (!doc_save("LARGEOUT.WPD")) {
        line(doc_error);
        return 5;
    }
    measurement("SAVE", start);
    line("X16PERFECT STORAGE PASS");
    return 0;
}
