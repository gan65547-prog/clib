/*
 * ml_string — Safe string operations and macros.
 *
 * Provides length-checked string copy, search, replace, and formatting
 * functions for C strings.  All functions guard against buffer overflow.
 */
#ifndef __ML_STRING_H__
#define __ML_STRING_H__

#include <stdio.h>
#include <string.h>

#define ml_string(s)               ((s) && (0 != *s))

#define ml_tolower(c)              (char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define ml_toupper(c)              (char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

#define ml_strncmp(s1, s2, n)      strncmp((const char *) s1, (const char *) s2, n)
#define ml_strcmp(s1, s2)          strcmp((const char *) s1, (const char *) s2)
#define ml_strlen(s)               strlen((const char *) s)
#define ml_strncpy(d, s, n)        strncpy(d, s, n)
#define ml_snprintf                snprintf
#define ml_sscanf                  sscanf

#define ml_sprintf(b, l, fmt, ...)                     \
    do {                                                \
        memset((b), 0, (l));                            \
        snprintf((b), ((l)-1), fmt, ##__VA_ARGS__);     \
    } while(0)

extern const char * ml_strchr(const char *s, int c);
extern const char * ml_strrchr(const char *s, int c);
extern void ml_strlow(char *dst, const char *src, int n);
extern int ml_strnlen(const char *p, int n);
extern char * ml_cpystrn(char *dst, const char *src, int n);
extern const char * ml_strnstr(const char *s1, const char *s2, int len);
extern char * ml_cpystrc(char *dst, const char *src, int dstlen, int srclen, unsigned char c);
extern char * ml_cpystrc2(char *dst, const char *src, int dstlen, unsigned char c);
extern char * ml_cpystrs(char * dst, const char * src, int dstlen, const char * ctx);
extern char * ml_cpystrrc(char *dst, const char *src, int dstlen, unsigned char c);
extern int ml_strresp(const char * src, const char * find, const char * replace, char * out, int slen, int olen);

#endif
