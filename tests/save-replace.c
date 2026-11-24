#include <ascii_charmap.h>
#include <cbm.h>
#include "../document.h"

extern unsigned bank_count(void);

static void line(const char *s)
{
    while (*s) cbm_k_bsout(*s++);
    cbm_k_bsout(13);
}

static bool has_body(const char *name, const char *body, unsigned size)
{
    unsigned i;
    if (!doc_load(name) || doc_size() != size) return false;
    for (i = 0; i < size; ++i)
        if (doc_at(i) != (unsigned char)body[i]) return false;
    return true;
}

int main(void)
{
    static const char first[] = "original";
    static const char second[] = "replacement";

    cbm_k_bsout(0x0f);
    doc_init(bank_count());
    if (doc_load("FULL.WPD")) {
        /* Run with FULL.WPD containing "original" and no free disk space. */
        if (!doc_replace(0, doc_size(), second, sizeof(second) - 1) ||
            doc_save("FULL.WPD", true) != DOC_SAVE_FAILED || !doc_changed ||
            !has_body("FULL.WPD", first, sizeof(first) - 1))
            goto fail;
        line("FULL DISK PASS");
        return 0;
    }
    if (!doc_insert(0, first, sizeof(first) - 1) ||
        doc_save("X16P00.BAK", false) != DOC_SAVE_OK ||
        doc_save("X16P01.TMP", false) != DOC_SAVE_OK ||
        doc_save("CHECK.WPD", false) != DOC_SAVE_OK)
        goto fail;
    if (!doc_replace(0, doc_size(), second, sizeof(second) - 1) ||
        doc_save("CHECK.WPD", false) != DOC_SAVE_EXISTS || !doc_changed)
        goto fail;
    if (!has_body("CHECK.WPD", first, sizeof(first) - 1))
        goto fail;
    if (!doc_replace(0, doc_size(), second, sizeof(second) - 1) ||
        doc_save("MISSING.WPD", true) != DOC_SAVE_FAILED || !doc_changed ||
        doc_save("CHECK.WPD", true) != DOC_SAVE_OK || doc_changed)
        goto fail;
    if (!has_body("CHECK.WPD", second, sizeof(second) - 1) ||
        !has_body("X16P00.BAK", first, sizeof(first) - 1) ||
        !has_body("X16P01.TMP", first, sizeof(first) - 1))
        goto fail;
    line("SAVE REPLACE PASS");
    return 0;
fail:
    line("SAVE REPLACE FAILED");
    if (doc_error) line(doc_error);
    return 1;
}
