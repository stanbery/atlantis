#include "rtl.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char *rtl_strdup(const char *str)
{
    return str ? strcpy(malloc(strlen(str)+1), str) : 0;
}

char *rtl_strlwr(char *str)
{
    char *s;
    for (s = str; *s; ++s) {
        *s = (char) tolower(*s);
    }
    return str;
}

int rtl_memicmp(const char *a, const char *b, size_t size)
{
    int ca = tolower(*a);
    int cb = tolower(*b);
    if (size == 0)
        return 0;
    if (ca < cb)
        return -1;
    if (ca > cb)
        return 1;
    return rtl_memicmp(a + 1, b + 1, size - 1);
}

int rtl_strcmpl(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b) {
        int ca = tolower(*a);
        int cb = tolower(*b);
        if (ca < cb) {
            return -1;
        } else if (ca > cb) {
            return 1;
        }
    }
    if (*b)
        return -1;
    if (*a)
        return 1;
    return 0;
}
