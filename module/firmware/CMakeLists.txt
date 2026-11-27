# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)
project(FAT12_firmware C)
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  message(FATAL_ERROR "BOOT firmware profile requires AMD64")
endif()
if(NOT EXISTS "${COMMON_INCLUDE_DIR}/infiltratr/filesystem_module_v2.h")
  message(FATAL_ERROR "Pass the pinned canonical Common V2 include directory")
endif()
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(FS "${ROOT}/native/filesystems/fat")
add_library(firmware MODULE
  "${FS}/module/classic_fat_module.c"
  "${FS}/portable/fatfs_module.c"
  "${FS}/portable/ff.c"
  "${FS}/portable/ffunicode.c"
  "${FS}/portable/fatfs_libc.c"
)
set_target_properties(firmware PROPERTIES PREFIX "" SUFFIX ".so" OUTPUT_NAME "FAT12" C_STANDARD 11 C_STANDARD_REQUIRED ON C_VISIBILITY_PRESET hidden)
target_include_directories(firmware PRIVATE "${COMMON_INCLUDE_DIR}" "${FS}/module" "${FS}/portable" "${ROOT}/native/include")
target_compile_definitions(firmware PRIVATE _GNU_SOURCE _FILE_OFFSET_BITS=64 IFS_FATFS_ONLY=12 IFS_CLASSIC_FAT_BITS=12)
target_compile_options(firmware PRIVATE -O2 -march=x86-64 -mtune=generic -mno-red-zone -mgeneral-regs-only -fcf-protection=none -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)
target_link_options(firmware PRIVATE -nostdlib -Wl,-Bsymbolic,-z,defs,-z,noexecstack,-z,relro,-z,now,-z,nopack-relative-relocs,-z,max-page-size=4096 "-Wl,--version-script=${ROOT}/standalone/v2-only.map")
