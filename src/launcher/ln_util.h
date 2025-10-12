
#ifndef __LN_UTIL_H__
#define __LN_UTIL_H__

#include <stdarg.h>
#include "doomtype.h"

extern int dialog_open;

#define DIALOG_EMPTY  0
#define DIALOG_OK     1
#define DIALOG_YES_NO 2

void LN_DialogResponder(void (*responder)(int));
void LN_OpenDialog(int type, const char *header, const char *msg);
void LN_CloseDialog(void);
void LN_HandleDialog(void);

char *LN_allocvsprintf(const char *fmt, va_list args);
char *LN_allocsprintf(const char *fmt, ...) PRINTF_ATTR(1, 2);

#endif
