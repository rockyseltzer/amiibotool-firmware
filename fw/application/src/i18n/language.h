#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_id.h"

#define _T(x) getLangString(_L_##x)

extern const char *const lang_en_US[_L_COUNT];

static inline const char *getLangString(L_StringID stringID)
{
    if (stringID >= _L_COUNT)
    {
        return "@@STR@@";
    }
    return lang_en_US[stringID];
}

#endif /* LANGUAGE_H */
