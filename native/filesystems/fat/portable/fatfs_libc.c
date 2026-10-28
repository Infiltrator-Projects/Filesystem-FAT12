/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Freestanding C intrinsics required by the original FatFs engine. */
#include <stddef.h>
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d=(unsigned char *)dest;
    const unsigned char *s=(const unsigned char *)src;
    for(size_t i=0;i<n;++i) d[i]=s[i];
    return dest;
}
void *memset(void *dest, int ch, size_t n) {
    unsigned char *d=(unsigned char *)dest;
    for(size_t i=0;i<n;++i) d[i]=(unsigned char)ch;
    return dest;
}
int memcmp(const void *left, const void *right, size_t n) {
    const unsigned char *a=(const unsigned char *)left,*b=(const unsigned char *)right;
    for(size_t i=0;i<n;++i) if(a[i]!=b[i]) return (int)a[i]-(int)b[i];
    return 0;
}
size_t strlen(const char *s) {
    size_t n=0;
    while(s[n]) ++n;
    return n;
}
char *strchr(const char *s,int c) {
    const char ch=(char)c;
    for(;;++s) {if(*s==ch) return (char *)s; if(!*s) return 0;}
}
