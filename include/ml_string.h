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

/* True if s is non-NULL and non-empty */
#define ml_string(s)               ((s) && (0 != *s))

#define ml_tolower(c)              (char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define ml_toupper(c)              (char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

#define ml_strncmp(s1, s2, n)      strncmp((const char *) s1, (const char *) s2, n)
#define ml_strcmp(s1, s2)          strcmp((const char *) s1, (const char *) s2)
#define ml_strlen(s)               strlen((const char *) s)
#define ml_strncpy(d, s, n)        strncpy(d, s, n)
#define ml_snprintf                snprintf
#define ml_sscanf                  sscanf

/* Formatted print into a buffer (zero-fills, guarantees null termination) */
#define ml_sprintf(b, l, fmt, ...)                     \
    do {                                                \
        memset((b), 0, (l));                            \
        snprintf((b), ((l)-1), fmt, ##__VA_ARGS__);     \
    } while(0)

/* Find first occurrence of character c in string s */
extern char * ml_strchr(const char *s, int c);
/* Find last occurrence of character c in string s */
extern char * ml_strrchr(const char *s, int c);
/* Convert string to lowercase */
extern void ml_strlow(char *dst, char *src, int n);
/* Return string length, clamped to n */
extern int ml_strnlen(char *p, int n);
/* Copy n bytes of string, guaranteed null-terminated */
extern char * ml_cpystrn(char *dst, char *src, int n);
/* Search for s2 within s1 (len = length of s1) */
extern char * ml_strnstr(char *s1, char *s2, int len);
/* Safe copy, stops at character c. dstlen = destination buffer size */
extern char * ml_cpystrc(char *dst, char *src, int dstlen, int srclen, unsigned char c);
/* Safe copy, stops at character c */
extern char * ml_cpystrc2(char *dst, char *src, int dstlen, unsigned char c);
/* Safe copy, stops when the string ctx is found */
extern char * ml_cpystrs(char * dst, char * src, int dstlen, char * ctx);
/* Copy from the last occurrence of character c */
extern char * ml_cpystrrc(char *dst, char *src, int dstlen, unsigned char c);
/* Replace all occurrences of 'find' with 'replace' in src, write to out */
extern int ml_strresp(char * src, char * find, char * replace, char * out, int slen, int olen);

#endif
