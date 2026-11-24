#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdbool.h>

typedef unsigned long Pos;

void doc_init(unsigned banks);
Pos doc_size(void);
unsigned char doc_at(Pos position);
unsigned doc_copy(Pos position, void *destination, unsigned count);
bool doc_insert(Pos position, const void *source, unsigned count);
bool doc_replace(Pos first, Pos last, const void *bytes, unsigned count);
void doc_delete(Pos first, Pos last);
void doc_clear(void);
bool doc_load(const char *name);
typedef enum {
    DOC_SAVE_OK,
    DOC_SAVE_EXISTS,
    DOC_SAVE_FAILED
} DocSaveResult;

DocSaveResult doc_save(const char *name, bool replace_existing);
unsigned doc_token(Pos position);
Pos doc_previous(Pos position);

extern bool doc_readonly;
extern bool doc_changed;
extern const char *doc_error;

#endif
