/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise the kernel's read-only variant preflight against regular images. */
#define _POSIX_C_SOURCE 200809L
#include "fat_variant_guard.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc,char **argv)
{
    unsigned char b[512];
    struct stat st;
    int fd;
    unsigned expected,actual;
    if (argc!=3 && argc!=4) return 2;
    fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0 || fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || read(fd,b,sizeof b)!=(ssize_t)sizeof b)
        return 2;
    close(fd);
    expected=(unsigned)strtoul(argv[2],NULL,10);
    actual=ifs_fat_bpb_width(b);
    if(actual!=expected) {
        fprintf(stderr,"FAT-KO-BPB-FAIL: expected %u got %u\n",expected,actual);
        return 1;
    }
    if (argc==4 && (strcmp(argv[3],"dos1")!=0 || !ifs_fat_is_dos1_bpb(b)))
        return 1;
    printf("FAT-KO-BPB-PASS: %u\n",actual);
    return 0;
}
