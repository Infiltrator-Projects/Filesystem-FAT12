#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fault-inject the actual OFS/FFS VFS resize boundary in a minimal host harness.

Only Linux services and low-level resizing are stubbed. The production resize
and setattr bodies are compiled verbatim, so a swallowed error fails this test.
Mounted media and mutation correctness are qualified separately in QEMU.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

repo = Path(sys.argv[1]).resolve()
compiler = sys.argv[2]


def function(source, name):
    start = source.index("int " + name + "(")
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"

stubs = r"""
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
typedef long long loff_t;
typedef uint32_t u32;
#define U32_MAX UINT32_MAX
#define EFBIG 27
#define EPERM 1
#define ATTR_SIZE 1
#define ATTR_UID 2
#define ATTR_GID 4
#define ATTR_MODE 8
#define SF_OFS 1
#define SF_SETUID 2
#define SF_SETGID 4
#define SF_QUIET 8
#define AFFS_MOUNT_SF_SETMODE 16
#define AFFS_MOUNT_SF_IMMUTABLE 32
struct mnt_idmap { int unused; };
struct affs_sb_info { unsigned long s_flags; };
struct super_block { struct affs_sb_info info; };
struct affs_inode_info { loff_t mmu_private; };
struct inode { loff_t i_size; unsigned long i_ino; struct super_block *i_sb; struct affs_inode_info priv; };
struct dentry { struct inode *inode; };
struct iattr { unsigned ia_valid; loff_t ia_size; };
static struct mnt_idmap nop_mnt_idmap;
static int injected_error, warnings, attributes_copied;
#define AFFS_I(i) (&(i)->priv)
#define AFFS_SB(s) (&(s)->info)
static int affs_test_opt(unsigned long f,unsigned long m) { return (f&m)!=0; }
static void truncate_pagecache(struct inode *i,loff_t s) { (void)i; (void)s; }
static void mark_inode_dirty(struct inode *i) { (void)i; }
static void affs_warning(struct super_block *s,const char *f,const char *m,...) { (void)s; (void)f; (void)m; ++warnings; }
static struct inode *d_inode(struct dentry *d) { return d->inode; }
static int setattr_prepare(struct mnt_idmap *m,struct dentry *d,struct iattr *a) { (void)m;(void)d;(void)a;return 0; }
static int inode_newsize_ok(struct inode *i,loff_t s) { (void)i;(void)s;return 0; }
static loff_t i_size_read(struct inode *i) { return i->i_size; }
static void truncate_setsize(struct inode *i,loff_t s) { i->i_size=s; }
static void setattr_copy(struct mnt_idmap *m,struct inode *i,struct iattr *a) { (void)m;(void)i;(void)a; ++attributes_copied; }
static void affs_mode_to_prot(struct inode *i) { (void)i; }
static int ifs_ofs_zero_extend_ofs(struct inode *i,u32 s) { if (!injected_error) i->priv.mmu_private=s; return injected_error; }
static int ifs_ofs_zero_extend_ffs(struct inode *i,u32 s) { if (!injected_error) i->priv.mmu_private=s; return injected_error; }
static int ifs_ofs_shrink_file(struct inode *i,u32 s) { if (!injected_error) i->priv.mmu_private=s; return injected_error; }
"""
main = r"""
int main(void)
{
    const int errors[] = {-28, -117, -5};
    struct super_block sb = {0};
    struct inode inode = {.i_size=100, .i_ino=1, .i_sb=&sb,
                          .priv={.mmu_private=100}};
    struct dentry dentry = {&inode};
    struct iattr attributes = {ATTR_SIZE, 200};
    for (unsigned format=0; format<2; ++format) {
        sb.info.s_flags = format ? SF_OFS : 0;
        for (unsigned e=0; e<sizeof(errors)/sizeof(errors[0]); ++e) {
            for (unsigned growth=0; growth<2; ++growth) {
                attributes.ia_size = growth ? 200 : 0;
                injected_error = errors[e];
                warnings = attributes_copied = 0;
                int result = affs_notify_change(&nop_mnt_idmap, &dentry, &attributes);
                if (result != injected_error || inode.i_size != 100 ||
                    inode.priv.mmu_private != 100 || warnings != 1 ||
                    attributes_copied != 0) {
                    fprintf(stderr, "Resize error lost: %d, size %lld\\n", result, inode.i_size);
                    return 1;
                }
            }
        }
    }
    injected_error = 0;
    attributes.ia_size = 200;
    if (affs_notify_change(&nop_mnt_idmap, &dentry, &attributes) != 0 ||
        inode.i_size != 200 || inode.priv.mmu_private != 200 || attributes_copied != 1)
        return 1;
    attributes.ia_size = 0;
    if (affs_notify_change(&nop_mnt_idmap, &dentry, &attributes) != 0 ||
        inode.i_size != 0 || inode.priv.mmu_private != 0)
        return 1;
    attributes.ia_size = (loff_t)U32_MAX + 1;
    if (affs_notify_change(&nop_mnt_idmap, &dentry, &attributes) != -EFBIG)
        return 1;
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="filesystems-resize-") as temporary:
    root = Path(temporary)
    for fs in ("ofs", "ffs"):
        source = (repo / "native/filesystems" / fs / "linux/storage.c").read_text()
        support = stubs
        if fs == "ffs":
            support = support.replace("ifs_ofs_zero_extend_ofs", "unused_ofs_extend")
            support = support.replace("ifs_ofs_", "ifs_ffs_")
        harness = root / f"{fs}.c"
        harness.write_text(support + function(source, "affs_truncate") +
                           function(source, "affs_notify_change") + main)
        binary = root / fs
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-Wno-unused-parameter", "-Wno-unused-function",
                        str(harness), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print(f"{fs.upper()} preserves resize failures and refuses later attribute updates.")
