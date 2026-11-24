#include <ascii_charmap.h>
#include <cbm.h>
#include <string.h>
#include "../document.h"

extern unsigned bank_count(void);

static unsigned char fill[1024];

static void line(const char *s)
{
    while (*s) cbm_k_bsout(*s++);
    cbm_k_bsout(13);
}

static bool body_at(Pos pos, const unsigned char *expected, unsigned size)
{
    unsigned i;
    for (i = 0; i < size; ++i)
        if (doc_at(pos + i) != expected[i]) return false;
    return true;
}

int main(void)
{
    static const unsigned char excerpt[] = {'b', 0xc3, 12, 0xc3, 'C', 0xc4, 12, 0xc4, 'd'};
    static const unsigned char expected[] = {'X', 'b', 0xc3, 12, 0xc3, 'C', 0xc4, 12, 0xc4, 'd', 'Y'};
    static const unsigned char malformed[] = {0xd0, 1};
    static const unsigned char unsupported[] = {0x80};
    static const char text[] = "123456789012345\r\nx\tz\rW\n";
    static const unsigned char translated[] =
        {'1','2','3','4','5','6','7','8','9','0','1','2','3','4','5', 10, 'x',
         0xc1,2,0,0,0,0,0,0,0xc1, 'z',10,'W',10};
    unsigned i;
    int written;
    const Pos at = 8191;

    cbm_k_bsout(0x0f);
    doc_init(bank_count());
    if (!doc_insert(0, excerpt, sizeof(excerpt)) ||
        doc_save("EXCERPT.WPD", false) != DOC_SAVE_OK)
        goto fail;
    doc_clear();
    if (!doc_insert(0, malformed, sizeof(malformed)) ||
        doc_save("BROKEN.WPD", false) != DOC_SAVE_OK)
        goto fail;
    doc_clear();
    if (!doc_insert(0, unsupported, sizeof(unsupported)) ||
        doc_save("LOCKED.WPD", false) != DOC_SAVE_OK)
        goto fail;
    doc_clear();
    memset(fill, 'X', sizeof(fill));
    for (i = 0; i < 8; ++i)
        if (!doc_insert(doc_size(), fill, sizeof(fill))) goto fail;
    if (!doc_insert(doc_size(), "Y", 1) ||
        doc_save("TARGET.WPD", false) != DOC_SAVE_OK ||
        !doc_load("TARGET.WPD"))
        goto fail;
    if (doc_insert_file("MISSING.WPD", at) ||
        doc_insert_file("BROKEN.WPD", at) ||
        doc_insert_file("LOCKED.WPD", at) ||
        doc_changed || doc_readonly || doc_size() != 8193 ||
        doc_at(at - 1) != 'X' || doc_at(at) != 'X' || doc_at(at + 1) != 'Y')
        goto fail;
    if (!doc_insert_file("EXCERPT.WPD", 8192) ||
        !doc_changed || doc_readonly || doc_size() != 8193 + sizeof(excerpt) ||
        !body_at(at, expected, sizeof(expected)) ||
        doc_save("TARGET.WPD", true) != DOC_SAVE_OK ||
        !doc_load("TARGET.WPD") || !body_at(at, expected, sizeof(expected)))
        goto fail;
    if (cbm_open(4, 8, CBM_WRITE, "0:RAW.TXT,P,W")) goto fail;
    written = cbm_write(4, text, sizeof(text) - 1);
    cbm_close(4);
    if (written != sizeof(text) - 1 || !doc_load("RAW.TXT") ||
        doc_changed || doc_readonly || doc_size() != sizeof(translated) ||
        !body_at(0, translated, sizeof(translated)))
        goto fail;
    line("RETRIEVE PASS");
    return 0;
fail:
    line("RETRIEVE FAILED");
    if (doc_error) line(doc_error);
    return 1;
}
