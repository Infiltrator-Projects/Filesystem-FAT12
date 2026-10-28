# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the Linux minix filesystem routines.
#

# Build the complete original Minix VFS implementation as a standalone
# module regardless of the distribution kernel's own CONFIG_MINIX_FS value.
obj-m += minix.o

minix-objs := bitmap.o itree_v1.o itree_v2.o namei.o inode.o file.o dir.o
