/*
 * Copyright (c) 2026 Gan
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>

#include "ml_string.h"




const char * ml_strchr(const char *s, int c)
{
    for (; ; ++s) {
        if (*s == (char)c)
            return s;
        if (*s == '\0')
            return NULL;
    }
}



void ml_strlow(char *dst, const char *src, int n)
{
    while (n) {
        *dst = ml_tolower(*src);
        dst++;
        src++;
        n--;
    }
}


int ml_strnlen(const char *p, int n)
{
    int  i;

    for (i = 0; i < n; i++) {

        if (p[i] == '\0') {
            return i;
        }
    }

    return n;
}



const char * ml_strrchr(const char *s, int c)
{
    const char *last = NULL;
    do {
        if (*s == (char)c)
            last = s;
    } while (*s++);
    return last;
}





const char * ml_strnstr(const char *s1, const char *s2, int len)
{
    int l2;

    l2 = strlen(s2);
    if (!l2)
        return s1;

    while (len >= l2 && *s1 != '\0') {
        len--;
        if (!memcmp(s1, s2, l2))
            return s1;
        s1++;
    }
    return NULL;
}



char * ml_cpystrn(char *dst, const char *src, int n)
{
    if (n == 0) {
        return dst;
    }

    while (--n) {
        *dst = *src;

        if (*dst == '\0') {
            return dst;
        }

        dst++;
        src++;
    }

    *dst = '\0';

    return dst;
}



char * ml_cpystrrc(char *dst, const char *src, int dstlen, unsigned char c)
{
    const char * p1;
    int l, l1, l2;

    l1 = ml_strlen(src);
    if (!l1)
        return NULL;
    
    p1 = ml_strrchr(src, c);
    if (NULL == p1)
        return NULL;

    p1++;
    l2 = l1 - (p1 - src);
    l  = (l2 < dstlen) ? l2 : (dstlen -1);
    return ml_strncpy(dst, p1, l);
}




char * ml_cpystrc2(char *dst, const char *src, int dstlen, unsigned char c)
{
    int n;
    char * tmp = dst;

    n = ml_strlen(src);
    if (n == 0 || dstlen <= 0) {
        return dst;
    }

    while (--n) {
        if ((dst - tmp) >= (dstlen - 1)) {
            *dst = '\0';
            return dst;
        }

        *dst = *src;

        if (*dst == '\0') {
            return dst;
        }

        if (*src == c) {
            *dst = 0;
            return dst;
        }

        dst++;
        src++;
    }

    *dst = '\0';

    return dst;

}




char * ml_cpystrc(char *dst, const char *src, int dstlen, int srclen, unsigned char c)
{
    int n = srclen;

    if (n == 0 || dstlen <= 0) {
        return dst;
    }

    while (--n) {
        if ((dstlen--) <= 1) {
            *dst = '\0';
            return dst;
        }

        *dst = *src;

        if (*dst == '\0') {
            return dst;
        }

        if (*src == c) {
            *dst = 0;
            return dst;
        }

        dst++;
        src++;
    }

    *dst = '\0';

    return dst;
}




char * ml_cpystrs(char * dst, const char * src, int dstlen, const char * ctx)
{
    const char * p1;
    int l1, l2, l;

    l1 = ml_strlen(src);
    p1 = ml_strnstr(src, ctx, l1);
    if (NULL == p1)
        return NULL;

    l2 = p1 - src;
    l  = (l2 < dstlen) ? l2 : (dstlen-1);
    return ml_strncpy(dst, src, l);
}





int ml_strresp(const char * input, const char * ctx, const char * rpl, char * output, int inlen, int outlen)
{
    const char * p, *tmp;
    char *dst;
    int left = 0, len = 0;
    int lct, lrp, l1, l2;
    int size;

    lct = ml_strlen(ctx);
    lrp = ml_strlen(rpl);
    tmp = input;
    left = inlen;
    dst = output;


    while (left) {
        p = ml_strnstr(tmp, ctx, left);
        if (p) {
            l1 = p-tmp;
            size = outlen - len;
            if (size > 0 && size > l1) {
                ml_strncpy(dst+len, tmp, l1);

                len += l1;
                left -= l1;
                tmp = p + lct;


                size = outlen - len;
                if (size > 0 && size > lrp) {
                    ml_strncpy(dst+len, rpl, lrp);

                    len += lrp;
                    left -= lct;
                }
                else
                    break;
                
            }
            else
                break;
        }
        else {
            l2 = inlen-(tmp-input);
            size = outlen - len;
            if (size > 0 && size > l2) {
                ml_strncpy(dst+len, tmp, l2);

                len += l2;
                left -= l2;
            }

            break;
        }
    }

    return len;
}



