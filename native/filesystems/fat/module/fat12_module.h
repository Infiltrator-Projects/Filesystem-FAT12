/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Public ABI alias for the independent FAT12.so drop-in. */
#ifndef INFILTRATOR_FAT12_MODULE_H
#define INFILTRATOR_FAT12_MODULE_H
#include "classic_fat_module.h"
#define IFS_FAT12_ABI_VERSION IFS_CLASSIC_FAT_ABI_VERSION
#define IFS_FAT12_CAP_PROBE IFS_CLASSIC_FAT_CAP_PROBE
#define IFS_FAT12_CAP_ROOT_83 IFS_CLASSIC_FAT_CAP_ROOT_83
#define IFS_FAT12_CAP_FILE_READ IFS_CLASSIC_FAT_CAP_FILE_READ
#define IFS_FAT12_CAP_ROOT_FRAGMENT_COUNT IFS_CLASSIC_FAT_CAP_ROOT_FRAGMENT_COUNT
typedef IfsClassicFatReadAt IfsFat12ReadAt;
typedef IfsClassicFatIo IfsFat12Io;
typedef IfsClassicFatVolume IfsFat12Volume;
typedef IfsClassicFatFile IfsFat12File;
typedef IfsClassicFatModule IfsFat12Module;
#endif
