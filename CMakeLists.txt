# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FILESYSTEMS_VERSION)
string(STRIP "${FILESYSTEMS_VERSION}" FILESYSTEMS_VERSION)
if(NOT FILESYSTEMS_VERSION MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
    message(FATAL_ERROR "VERSION must contain major.minor.patch")
endif()

project(Filesystems VERSION "${FILESYSTEMS_VERSION}" LANGUAGES C CXX)
include(CTest)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FILESYSTEMS_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(FILESYSTEMS_BUILD_EFI_DRIVERS "Build independent x86-64 EFI filesystem drivers" OFF)
set(FILESYSTEMS_FAT_EFI_PREBUILT_DIR "" CACHE PATH
    "Directory containing runtime-qualified FAT12/FAT16 EFI drivers")
set(FILESYSTEMS_FAT_KO_REFERENCE_DIR "" CACHE PATH
    "Directory containing FAT12/FAT16 kernel modules for the qualified test kernel")

# Application-owned executable and resources live in the native namespace.
# Linux may keep a minimal /usr/bin launcher and distro-specific integration,
# but neither the application nor its selected format build sources live
# under /usr/lib/filesystems. One configure-time root sets all app paths.
set(FILESYSTEMS_APPLICATION_ROOT "/Programs/Filesystems" CACHE PATH
    "Native Filesystems application directory")

# Native deployment policy owns only filesystem/module membership. Desktop
# integration has an independent authority in data/desktop-filesystems.tsv and
# must never influence which kernel modules are built, packaged or managed.
set(FILESYSTEMS_NATIVE_MANIFEST
    "${CMAKE_CURRENT_SOURCE_DIR}/data/native-filesystems.tsv")
file(STRINGS "${FILESYSTEMS_NATIVE_MANIFEST}"
    FILESYSTEMS_NATIVE_MANIFEST_LINES)
set(FILESYSTEMS_NATIVE_FILESYSTEMS)
set(FILESYSTEMS_NATIVE_MODULES)
foreach(native_line IN LISTS FILESYSTEMS_NATIVE_MANIFEST_LINES)
    string(STRIP "${native_line}" native_line)
    if(native_line STREQUAL "" OR native_line MATCHES "^#")
        continue()
    endif()

    string(REPLACE "\t" ";" native_fields "${native_line}")
    list(LENGTH native_fields native_field_count)
    if(NOT native_field_count EQUAL 2)
        message(FATAL_ERROR
            "Invalid native filesystem manifest row: ${native_line}")
    endif()

    list(GET native_fields 0 native_fs)
    list(GET native_fields 1 native_module)
    if(NOT native_fs MATCHES "^[A-Za-z0-9_.+-]+$" OR
       NOT native_module MATCHES "^[A-Za-z0-9_.+-]+$")
        message(FATAL_ERROR
            "Invalid native filesystem/module token: ${native_line}")
    endif()

    list(FIND FILESYSTEMS_NATIVE_FILESYSTEMS
        "${native_fs}" native_duplicate_index)
    if(NOT native_duplicate_index EQUAL -1)
        message(FATAL_ERROR
            "Duplicate native filesystem in manifest: ${native_fs}")
    endif()
    list(FIND FILESYSTEMS_NATIVE_MODULES
        "${native_module}" module_duplicate_index)
    if(NOT module_duplicate_index EQUAL -1)
        message(FATAL_ERROR
            "Duplicate native module in manifest: ${native_module}")
    endif()

    list(APPEND FILESYSTEMS_NATIVE_FILESYSTEMS "${native_fs}")
    list(APPEND FILESYSTEMS_NATIVE_MODULES "${native_module}")
    set("FILESYSTEMS_NATIVE_MODULE_${native_fs}" "${native_module}")
endforeach()
if(NOT FILESYSTEMS_NATIVE_FILESYSTEMS)
    message(FATAL_ERROR "Native filesystem deployment manifest is empty")
endif()

# Generate the C++ native policy directly from the same deployment manifest.
# catalog.cpp therefore cannot acquire a second, independently maintained list
# of native filesystem IDs or kernel-module names.
set(FILESYSTEMS_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${FILESYSTEMS_GENERATED_DIR}")
set(FILESYSTEMS_NATIVE_POLICY_HEADER
    "${FILESYSTEMS_GENERATED_DIR}/native_filesystems.generated.hpp")
list(LENGTH FILESYSTEMS_NATIVE_FILESYSTEMS
    FILESYSTEMS_NATIVE_FILESYSTEM_COUNT)
set(native_policy_header
    "// Generated from data/native-filesystems.tsv; do not edit.\n#pragma once\n\n#include <array>\n#include <string_view>\n\nnamespace filesystems {\n\nstruct NativeFilesystemPolicy {\n    std::string_view filesystem_id;\n    std::string_view module;\n};\n\ninline constexpr std::array<NativeFilesystemPolicy, ${FILESYSTEMS_NATIVE_FILESYSTEM_COUNT}> kNativeFilesystemPolicies = {{\n")
foreach(native_fs IN LISTS FILESYSTEMS_NATIVE_FILESYSTEMS)
    set(native_module "${FILESYSTEMS_NATIVE_MODULE_${native_fs}}")
    string(APPEND native_policy_header
        "    {\"${native_fs}\", \"${native_module}\"},\n")
endforeach()
string(APPEND native_policy_header "}};\n\n} // namespace filesystems\n")
file(WRITE "${FILESYSTEMS_NATIVE_POLICY_HEADER}" "${native_policy_header}")

set(INFILTRATR_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common")
set(INFILTRATR_COMMON_EXPECTED_VERSION "1.19.46")

if(NOT EXISTS "${INFILTRATR_COMMON_DIR}/VERSION")
    message(FATAL_ERROR "Pinned Libraries submodule is required")
endif()

file(READ "${INFILTRATR_COMMON_DIR}/VERSION" INFILTRATR_COMMON_VERSION)
string(STRIP "${INFILTRATR_COMMON_VERSION}" INFILTRATR_COMMON_VERSION)
if(NOT INFILTRATR_COMMON_VERSION STREQUAL INFILTRATR_COMMON_EXPECTED_VERSION)
    message(FATAL_ERROR
        "Filesystems requires Common ${INFILTRATR_COMMON_EXPECTED_VERSION}; found ${INFILTRATR_COMMON_VERSION}")
endif()

# Git itself owns the exact Common revision through the submodule gitlink. Do
# not duplicate that SHA in CMake: compare the checked-out submodule directly
# with the commit recorded by this Filesystems revision.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    find_package(Git REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}"
                rev-parse "HEAD:src/infiltratr-common"
        RESULT_VARIABLE COMMON_GITLINK_RESULT
        OUTPUT_VARIABLE INFILTRATR_COMMON_GITLINK
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${INFILTRATR_COMMON_DIR}" rev-parse HEAD
        RESULT_VARIABLE COMMON_CHECKOUT_RESULT
        OUTPUT_VARIABLE INFILTRATR_COMMON_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT COMMON_GITLINK_RESULT EQUAL 0 OR
       NOT COMMON_CHECKOUT_RESULT EQUAL 0 OR
       NOT INFILTRATR_COMMON_COMMIT STREQUAL INFILTRATR_COMMON_GITLINK)
        message(FATAL_ERROR
            "Libraries checkout ${INFILTRATR_COMMON_COMMIT} does not match repository gitlink ${INFILTRATR_COMMON_GITLINK}")
    endif()
endif()

set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INFILTRATR_COMMON_WARNINGS_AS_ERRORS ON CACHE BOOL "" FORCE)
add_subdirectory("${INFILTRATR_COMMON_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/infiltratr-common" EXCLUDE_FROM_ALL)

set(FILESYSTEMS_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow)

function(filesystems_configure target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(FILESYSTEMS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang" OR
           CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE ${FILESYSTEMS_WARNING_FLAGS})
        if(FILESYSTEMS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Filesystem-neutral native mechanisms. Platform-specific adapters provide I/O
# and operating-system integration; filesystem semantics must not depend on
# Linux VFS or Windows IFS objects.
add_library(infiltratr-fs-native-core STATIC
    native/core/status.c
    native/core/io.c
    native/core/byte_reader.c
    native/core/filesystem.c)
target_include_directories(infiltratr-fs-native-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/include")
target_link_libraries(infiltratr-fs-native-core
    PUBLIC InfiltratrCommon::Portable)
filesystems_configure(infiltratr-fs-native-core)

# Canonical EXT2 on-disk semantics. This exact source is also compiled into
# the Linux and Windows adapters; OS-specific code must not reimplement it.
add_library(infiltratr-fs-ext2-core STATIC
    native/filesystems/ext2/core/ext2_core.c
    native/filesystems/ext2/core/ext2_engine.c)
target_include_directories(infiltratr-fs-ext2-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ext2/core")
filesystems_configure(infiltratr-fs-ext2-core)

# Portable read-only firmware-facing EXT2 reader, not a UEFI driver binary.
add_library(infiltratr-fs-ext2-efi-reader STATIC
    native/filesystems/ext2/efi/reader.c)
target_link_libraries(infiltratr-fs-ext2-efi-reader
    PUBLIC infiltratr-fs-ext2-core)
target_include_directories(infiltratr-fs-ext2-efi-reader
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ext2/efi")
filesystems_configure(infiltratr-fs-ext2-efi-reader)

# Canonical EXT3 feature/format policy. Linux and future Windows adapters
# consume this exact implementation rather than maintaining independent rules.
add_library(infiltratr-fs-ext3-core STATIC
    native/filesystems/ext3/core/ext3_core.c)
target_include_directories(infiltratr-fs-ext3-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ext3/core")
filesystems_configure(infiltratr-fs-ext3-core)

# Canonical EXT4 feature/format policy shared by every host adapter.
add_library(infiltratr-fs-ext4-core STATIC
    native/filesystems/ext4/core/ext4_core.c)
target_include_directories(infiltratr-fs-ext4-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ext4/core")
filesystems_configure(infiltratr-fs-ext4-core)

# Canonical Amiga OFS and FFS format classification.
add_library(infiltratr-fs-ofs-core STATIC
    native/filesystems/ofs/core/ofs_core.c
    native/filesystems/ofs/core/ofs_primitives.c)
target_include_directories(infiltratr-fs-ofs-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ofs/core")
filesystems_configure(infiltratr-fs-ofs-core)

# Portable, project-owned AmigaDOS OFS firmware reader; no Boot source dependency.
add_library(infiltratr-fs-ofs-efi-reader STATIC
    native/filesystems/ofs/efi/reader.c)
target_link_libraries(infiltratr-fs-ofs-efi-reader
    PUBLIC infiltratr-fs-ofs-core)
target_include_directories(infiltratr-fs-ofs-efi-reader
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ofs/efi")
filesystems_configure(infiltratr-fs-ofs-efi-reader)

add_library(infiltratr-fs-ffs-core STATIC
    native/filesystems/ffs/core/ffs_core.c
    native/filesystems/ffs/core/ffs_primitives.c)
target_include_directories(infiltratr-fs-ffs-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ffs/core")
filesystems_configure(infiltratr-fs-ffs-core)

# Canonical Amiga SFS format/layout policy shared by host adapters.
add_library(infiltratr-fs-sfs-core STATIC
    native/filesystems/sfs/core/sfs_core.c)
target_include_directories(infiltratr-fs-sfs-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/sfs/core")
filesystems_configure(infiltratr-fs-sfs-core)

# Canonical Amiga SFS2 format policy. SFS2 is deliberately independent
# from the SFS core because its on-disk structures are not 1:1 compatible.
add_library(infiltratr-fs-sfs2-core STATIC
    native/filesystems/sfs2/core/sfs2_core.c)
target_include_directories(infiltratr-fs-sfs2-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/sfs2/core")
filesystems_configure(infiltratr-fs-sfs2-core)

# Canonical PFS format/geometry policy used by the PFS3 implementation.
add_library(infiltratr-fs-pfs3-core STATIC
    native/filesystems/pfs3/core/pfs3_core.c)
target_include_directories(infiltratr-fs-pfs3-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/pfs3/core")
filesystems_configure(infiltratr-fs-pfs3-core)

# Stable on-media identity is a platform-neutral authority. Keep it physically
# separate from Linux package/provider policy so non-Linux consumers cannot
# acquire Debian concerns through a target-level dependency.
add_library(filesystems-identity STATIC
    src/platform_support.cpp)
target_include_directories(filesystems-identity
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(filesystems-identity
    PUBLIC InfiltratrCommon::Portable)
target_compile_definitions(filesystems-identity
    PUBLIC FILESYSTEMS_VERSION="${PROJECT_VERSION}")
filesystems_configure(filesystems-identity)

# Linux catalogue materializes stable identity with Linux provider/package and
# native-deployment policy. This target is therefore intentionally Linux-aware.
add_library(filesystems-catalog STATIC
    src/catalog.cpp)
target_include_directories(filesystems-catalog
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PRIVATE "${FILESYSTEMS_GENERATED_DIR}")
target_link_libraries(filesystems-catalog
    PUBLIC filesystems-identity InfiltratrCommon::Portable)
filesystems_configure(filesystems-catalog)

if(WIN32)
    # Native Windows management shell consumes stable filesystem identity only;
    # Debian package/provider policy is not part of its dependency graph.
    add_executable(filesystems WIN32
        windows/manager/main.cpp)
    target_link_libraries(filesystems
        PRIVATE filesystems-identity Comctl32)
    target_compile_definitions(filesystems
        PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    filesystems_configure(filesystems)

    install(TARGETS filesystems
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(UNIX)
    if(FILESYSTEMS_BUILD_EFI_DRIVERS)
        if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
            message(FATAL_ERROR "EFI filesystem payload is currently qualified for x86-64 build hosts")
        endif()
        set(FILESYSTEMS_EFI_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/efi-drivers")
        file(GLOB_RECURSE FILESYSTEMS_EFI_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/firmware/uefi/*"
            "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ofs/efi/*"
            "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ofs/core/*")
        add_custom_command(
            OUTPUT
                "${FILESYSTEMS_EFI_OUTPUT}/drivers_x64/ext4_x64.efi"
                "${FILESYSTEMS_EFI_OUTPUT}/experimental/OFS_x64.efi"
            COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-efi-drivers.sh"
                "${FILESYSTEMS_EFI_OUTPUT}"
            DEPENDS ${FILESYSTEMS_EFI_SOURCES}
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-efi-drivers.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-ofs-efi.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gnu-efi-toolchain.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
            VERBATIM)
        add_custom_target(filesystems-efi-drivers ALL DEPENDS
            "${FILESYSTEMS_EFI_OUTPUT}/drivers_x64/ext4_x64.efi"
            "${FILESYSTEMS_EFI_OUTPUT}/experimental/OFS_x64.efi")
        install(FILES "${FILESYSTEMS_EFI_OUTPUT}/drivers_x64/ext4_x64.efi"
            DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Resources/EFI/drivers_x64")
        install(FILES "${FILESYSTEMS_EFI_OUTPUT}/experimental/OFS_x64.efi"
            DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Resources/EFI/experimental")

        # The release build consumes the *same* images used in the OVMF
        # runtime tests. For independent source builds, build them locally.
        if(FILESYSTEMS_FAT_EFI_PREBUILT_DIR)
            set(FILESYSTEMS_FAT_EFI_DIR "${FILESYSTEMS_FAT_EFI_PREBUILT_DIR}")
        else()
            set(FILESYSTEMS_FAT_EFI_DIR "${FILESYSTEMS_EFI_OUTPUT}/fat")
            file(GLOB_RECURSE FILESYSTEMS_FAT_EFI_SOURCES CONFIGURE_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/fat/efi/*")
            add_custom_command(
                OUTPUT "${FILESYSTEMS_FAT_EFI_DIR}/FAT12.efi"
                       "${FILESYSTEMS_FAT_EFI_DIR}/FAT16.efi"
                COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-fat12-fat16-efi.sh"
                    "${FILESYSTEMS_FAT_EFI_DIR}"
                DEPENDS ${FILESYSTEMS_FAT_EFI_SOURCES}
                    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-fat12-fat16-efi.sh"
                VERBATIM)
            add_custom_target(filesystems-fat-efi-drivers ALL DEPENDS
                "${FILESYSTEMS_FAT_EFI_DIR}/FAT12.efi"
                "${FILESYSTEMS_FAT_EFI_DIR}/FAT16.efi")
        endif()
        install(FILES "${FILESYSTEMS_FAT_EFI_DIR}/FAT12.efi"
                      "${FILESYSTEMS_FAT_EFI_DIR}/FAT16.efi"
            DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Resources/EFI/drivers_x64")
        install(FILES native/filesystems/fat/efi/EDK2-LICENSE.txt
                      native/filesystems/fat/efi/EDK2-CREDITS.txt
            DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Resources/EFI/drivers_x64")
        install(FILES
            firmware/uefi/filesystems/LICENSE.txt
            firmware/uefi/filesystems/LICENSE_GPL.txt
            firmware/uefi/COPYING.txt
            firmware/uefi/CREDITS.txt
            firmware/uefi/README.md
            DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/filesystems/efi")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
    pkg_check_modules(GIO2 REQUIRED IMPORTED_TARGET gio-2.0)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0>=3.22)

    add_library(infiltratr-fs-userspace STATIC
        native/platform/userspace/file_io.c)
    target_include_directories(infiltratr-fs-userspace
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/native/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/native/platform/userspace")
    target_link_libraries(infiltratr-fs-userspace
        PUBLIC infiltratr-fs-native-core InfiltratrCommon::Common)
    filesystems_configure(infiltratr-fs-userspace)

    add_executable(fsinspect
        tools/fsinspect/main.c)
    target_link_libraries(fsinspect PRIVATE infiltratr-fs-userspace)
    filesystems_configure(fsinspect)

    # Developer-preview, image-only OFS formatter. Cannot overwrite a file
    # or open a block device; physical partition formatting is NOT qualified.
    add_executable(filesystems-ofs-image tools/ofs-image/main.c)
    target_link_libraries(filesystems-ofs-image PRIVATE infiltratr-fs-ofs-core)
    filesystems_configure(filesystems-ofs-image)
    install(TARGETS filesystems-ofs-image
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

    # Standalone OFS.so: directly reuse our canonical OFS core and the
    # existing freestanding OFS EFI *reader* (not the EFI driver wrapper).
    # The Filesystems Install OFS action compiles this source on demand.
    add_library(filesystems-ofs-dropin SHARED
        native/filesystems/ofs/module/ofs_module.c
        native/filesystems/ofs/efi/reader.c
        native/filesystems/ofs/core/ofs_core.c
        native/filesystems/ofs/core/ofs_primitives.c)
    set_target_properties(filesystems-ofs-dropin PROPERTIES
        PREFIX "" OUTPUT_NAME "OFS")
    target_include_directories(filesystems-ofs-dropin PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ofs/module"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ofs/efi"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ofs/core")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Match the on-demand compiler helper's -O2 optimization profile.
        # Higher -O3 optimization triggers a misleading cross-TU array-bound
        # diagnostic in the independently qualified legacy OFS reader; do
        # not edit, suppress warnings globally, or fork that reader.
        target_compile_options(filesystems-ofs-dropin PRIVATE
            -O2 -ffreestanding -fno-builtin -fno-stack-protector -fvisibility=hidden)
        if(CMAKE_C_FLAGS MATCHES "fsanitize")
            target_link_options(filesystems-ofs-dropin PRIVATE
                -Wl,-z,relro -Wl,-z,now)
        else()
            target_link_options(filesystems-ofs-dropin PRIVATE
                -nostdlib -Wl,-z,defs -Wl,-z,relro -Wl,-z,now)
        endif()
    endif()
    filesystems_configure(filesystems-ofs-dropin)
    install(FILES
        native/filesystems/ofs/module/ofs_module.c
        native/filesystems/ofs/module/ofs_module.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/ofs/module")
    install(FILES
        native/filesystems/ofs/efi/reader.c
        native/filesystems/ofs/efi/reader.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/ofs/efi")
    install(FILES
        native/filesystems/ofs/core/ofs_core.c
        native/filesystems/ofs/core/ofs_core.h
        native/filesystems/ofs/core/ofs_primitives.c
        native/filesystems/ofs/core/ofs_primitives.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/ofs/core")
    install(PROGRAMS packaging/linux/ofs-module-helper.sh
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Helpers"
        RENAME ofs-module-helper)

    # The FAT defrag engine lives in each independently installable module.
    # The host provides process, device-safety, and generic Common services;
    # no host runtime or Common implementation is linked into the module.
    set(IFS_FAT_DEFRAG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/fat/defrag")
    set(IFS_FAT_DEFRAG_SOURCES
        "${IFS_FAT_DEFRAG_ROOT}/writer.c"
        "${IFS_FAT_DEFRAG_ROOT}/fat_analysis.c"
        "${IFS_FAT_DEFRAG_ROOT}/fat_directory.c"
        "${IFS_FAT_DEFRAG_ROOT}/fat_relayout.c"
        "${IFS_FAT_DEFRAG_ROOT}/fat_io.c"
        "${IFS_FAT_DEFRAG_ROOT}/fat_journal.c"
        "${IFS_FAT_DEFRAG_ROOT}/fat_relocation.c"
        "${IFS_FAT_DEFRAG_ROOT}/fat_volume.c"
        "${IFS_FAT_DEFRAG_ROOT}/fat_defrag_module.c")
    function(filesystems_fat_defrag target bits)
        target_sources(${target} PRIVATE ${IFS_FAT_DEFRAG_SOURCES})
        target_include_directories(${target} PRIVATE
            "${IFS_FAT_DEFRAG_ROOT}" "${IFS_FAT_DEFRAG_ROOT}/host"
            "${INFILTRATR_COMMON_DIR}/include")
        target_compile_definitions(${target} PRIVATE
            _GNU_SOURCE _FILE_OFFSET_BITS=64 IFS_FAT_DEFRAG_BITS=${bits})
    endfunction()
    install(DIRECTORY "${IFS_FAT_DEFRAG_ROOT}/"
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat-defrag"
        FILES_MATCHING PATTERN "*.c" PATTERN "*.h")
    install(DIRECTORY "${INFILTRATR_COMMON_DIR}/include/infiltratr"
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat-defrag/include"
        FILES_MATCHING PATTERN "*.h")

    # FAT16 is the first selectable single-file module. Build the exact
    # source in CI; installed Filesystems compiles that SAME source when the
    # user selects "Install FAT16". Do not preinstall a module or use APT.
    add_library(filesystems-fat16-module SHARED
        native/filesystems/fat/module/fat16_module.c
        native/filesystems/fat/portable/fatfs_module.c
        native/filesystems/fat/portable/ff.c
        native/filesystems/fat/portable/ffunicode.c
        native/filesystems/fat/portable/fatfs_libc.c)
    target_include_directories(filesystems-fat16-module PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/fat/module"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/fat/portable")
    target_compile_definitions(filesystems-fat16-module PRIVATE IFS_FATFS_ONLY=16)
    filesystems_fat_defrag(filesystems-fat16-module 16)
    set_target_properties(filesystems-fat16-module PROPERTIES
        PREFIX "" OUTPUT_NAME "FAT16")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(filesystems-fat16-module PRIVATE
            -ffreestanding -fno-builtin -fno-stack-protector -fvisibility=hidden)
        # Production module is freestanding. Sanitizer CI deliberately links
        # compiler runtimes; -nostdlib/-z defs would prevent ASan/UBSan tests.
        if(CMAKE_C_FLAGS MATCHES "fsanitize")
            target_link_options(filesystems-fat16-module PRIVATE
                -Wl,-z,relro -Wl,-z,now)
        else()
            target_link_options(filesystems-fat16-module PRIVATE
                -nostdlib -Wl,-z,defs -Wl,-z,relro -Wl,-z,now)
        endif()
    endif()
    filesystems_configure(filesystems-fat16-module)
    install(FILES
        native/filesystems/fat/module/classic_fat_module.c
        native/filesystems/fat/module/classic_fat_module.h
        native/filesystems/fat/module/fat16_module.c
        native/filesystems/fat/module/fat16_module.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat16")
    install(FILES
        native/filesystems/fat/portable/ff.c
        native/filesystems/fat/portable/ff.h
        native/filesystems/fat/portable/ffconf.h
        native/filesystems/fat/portable/diskio.h
        native/filesystems/fat/portable/ffunicode.c
        native/filesystems/fat/portable/fatfs_libc.c
        native/filesystems/fat/portable/fatfs_module.c
        native/filesystems/fat/portable/fatfs_module.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat16")
    install(PROGRAMS packaging/linux/fat16-module-helper.sh
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Helpers"
        RENAME fat16-module-helper)

    # One shared source engine, two independently compiled and installed .so files.
    add_library(filesystems-fat12-module SHARED
        native/filesystems/fat/module/fat12_module.c
        native/filesystems/fat/portable/fatfs_module.c
        native/filesystems/fat/portable/ff.c
        native/filesystems/fat/portable/ffunicode.c
        native/filesystems/fat/portable/fatfs_libc.c)
    target_include_directories(filesystems-fat12-module PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/fat/module"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/fat/portable")
    target_compile_definitions(filesystems-fat12-module PRIVATE IFS_FATFS_ONLY=12)
    filesystems_fat_defrag(filesystems-fat12-module 12)
    set_target_properties(filesystems-fat12-module PROPERTIES
        PREFIX "" OUTPUT_NAME "FAT12")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(filesystems-fat12-module PRIVATE
            -ffreestanding -fno-builtin -fno-stack-protector -fvisibility=hidden)
        if(CMAKE_C_FLAGS MATCHES "fsanitize")
            target_link_options(filesystems-fat12-module PRIVATE
                -Wl,-z,relro -Wl,-z,now)
        else()
            target_link_options(filesystems-fat12-module PRIVATE
                -nostdlib -Wl,-z,defs -Wl,-z,relro -Wl,-z,now)
        endif()
    endif()
    filesystems_configure(filesystems-fat12-module)
    install(FILES
        native/filesystems/fat/module/classic_fat_module.c
        native/filesystems/fat/module/classic_fat_module.h
        native/filesystems/fat/module/fat12_module.c
        native/filesystems/fat/module/fat12_module.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat12")
    install(FILES
        native/filesystems/fat/portable/ff.c
        native/filesystems/fat/portable/ff.h
        native/filesystems/fat/portable/ffconf.h
        native/filesystems/fat/portable/diskio.h
        native/filesystems/fat/portable/ffunicode.c
        native/filesystems/fat/portable/fatfs_libc.c
        native/filesystems/fat/portable/fatfs_module.c
        native/filesystems/fat/portable/fatfs_module.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat12")
    install(PROGRAMS packaging/linux/fat12-module-helper.sh
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Helpers"
        RENAME fat12-module-helper)


    # FAT32 has real 28-bit FAT entries and a cluster-chain root directory.
    add_library(filesystems-fat32-module SHARED
        native/filesystems/fat/module/fat32_module.c
        native/filesystems/fat/portable/fatfs_libc.c)
    filesystems_fat_defrag(filesystems-fat32-module 32)
    target_include_directories(filesystems-fat32-module PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/fat/module")
    set_target_properties(filesystems-fat32-module PROPERTIES
        PREFIX "" OUTPUT_NAME "FAT32")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(filesystems-fat32-module PRIVATE
            -ffreestanding -fno-builtin -fno-stack-protector -fvisibility=hidden)
        if(CMAKE_C_FLAGS MATCHES "fsanitize")
            target_link_options(filesystems-fat32-module PRIVATE
                -Wl,-z,relro -Wl,-z,now)
        else()
            target_link_options(filesystems-fat32-module PRIVATE
                -nostdlib -Wl,-z,defs -Wl,-z,relro -Wl,-z,now)
        endif()
    endif()
    filesystems_configure(filesystems-fat32-module)
    install(FILES
        native/filesystems/fat/module/fat32_module.c
        native/filesystems/fat/module/fat32_module.h
        native/filesystems/fat/portable/fatfs_libc.c
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat32")
    install(PROGRAMS packaging/linux/fat32-module-helper.sh
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Helpers"
        RENAME fat32-module-helper)

    # Independent EXT2.so: canonical read/directory engine plus constrained
    # existing-block overwrites. Does not replace ext2.ko or EFI.
    add_library(filesystems-ext2-dropin SHARED
        native/filesystems/ext2/module/ext2_module.c
        native/filesystems/ext2/core/ext2_core.c
        native/filesystems/ext2/core/ext2_engine.c)
    set_target_properties(filesystems-ext2-dropin PROPERTIES
        PREFIX "" OUTPUT_NAME "EXT2")
    target_include_directories(filesystems-ext2-dropin PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ext2/module"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/ext2/core")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(filesystems-ext2-dropin PRIVATE
            -O2 -ffreestanding -fno-builtin -fno-stack-protector
            -fvisibility=hidden)
        if(CMAKE_C_FLAGS MATCHES "fsanitize")
            target_link_options(filesystems-ext2-dropin PRIVATE
                -Wl,-z,relro -Wl,-z,now)
        else()
            target_link_options(filesystems-ext2-dropin PRIVATE
                -nostdlib -Wl,-z,defs -Wl,-z,relro -Wl,-z,now)
        endif()
    endif()
    filesystems_configure(filesystems-ext2-dropin)
    install(FILES
        native/filesystems/ext2/module/ext2_module.c
        native/filesystems/ext2/module/ext2_module.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/ext2/module")
    install(FILES
        native/filesystems/ext2/core/ext2_core.c
        native/filesystems/ext2/core/ext2_core.h
        native/filesystems/ext2/core/ext2_engine.c
        native/filesystems/ext2/core/ext2_engine.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/ext2/core")
    install(PROGRAMS packaging/linux/ext2-module-helper.sh
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Helpers"
        RENAME ext2-module-helper)

    # SCO BFS has contiguous file data and fixed 512-byte blocks. This
    # standalone shared module implements real root traversal and in-place
    # overwrites, independently of the Linux BFS kernel driver.
    add_library(filesystems-bfs-dropin SHARED
        native/filesystems/bfs/module/bfs_module.c)
    set_target_properties(filesystems-bfs-dropin PROPERTIES
        PREFIX "" OUTPUT_NAME "BFS")
    target_include_directories(filesystems-bfs-dropin PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/bfs/module")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(filesystems-bfs-dropin PRIVATE
            -O2 -ffreestanding -fno-builtin -fno-stack-protector
            -fvisibility=hidden)
        if(CMAKE_C_FLAGS MATCHES "fsanitize")
            target_link_options(filesystems-bfs-dropin PRIVATE
                -Wl,-z,relro -Wl,-z,now)
        else()
            target_link_options(filesystems-bfs-dropin PRIVATE
                -nostdlib -Wl,-z,defs -Wl,-z,relro -Wl,-z,now)
        endif()
    endif()
    filesystems_configure(filesystems-bfs-dropin)
    install(FILES
        native/filesystems/bfs/module/bfs_module.c
        native/filesystems/bfs/module/bfs_module.h
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/bfs/module")
    install(PROGRAMS packaging/linux/bfs-module-helper.sh
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Helpers"
        RENAME bfs-module-helper)

    add_library(filesystems-core STATIC
        src/probe.cpp
        src/platform/linux_host_paths.cpp
        src/action_policy.cpp)
    target_include_directories(filesystems-core
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(filesystems-core
        PUBLIC filesystems-catalog PkgConfig::GLIB2)
    target_compile_definitions(filesystems-core PRIVATE
        FILESYSTEMS_APPLICATION_ROOT="${FILESYSTEMS_APPLICATION_ROOT}")
    filesystems_configure(filesystems-core)

    add_library(filesystems-actions STATIC
        src/installer.cpp
        src/removal_guard.cpp
        src/desktop_actions.cpp)
    target_include_directories(filesystems-actions
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(filesystems-actions
        PUBLIC filesystems-core PkgConfig::GIO2)
    target_compile_definitions(filesystems-actions PRIVATE
        FILESYSTEMS_APPLICATION_ROOT="${FILESYSTEMS_APPLICATION_ROOT}")
    filesystems_configure(filesystems-actions)

    add_executable(filesystems-package-removal-guard
        src/removal_guard_main.cpp)
    target_link_libraries(filesystems-package-removal-guard
        PRIVATE filesystems-actions)
    set_target_properties(filesystems-package-removal-guard PROPERTIES
        OUTPUT_NAME package-removal-guard)
    filesystems_configure(filesystems-package-removal-guard)
    install(TARGETS filesystems-package-removal-guard
        RUNTIME DESTINATION "lib/filesystems")

    add_executable(filesystems
        src/main.cpp)
    # The native program is /Programs/Filesystems/Filesystems; the lowercase
    # 'filesystems' name is reserved for the Linux /usr/bin launcher.
    set_target_properties(filesystems PROPERTIES OUTPUT_NAME "Filesystems")
    target_link_libraries(filesystems
        PRIVATE filesystems-actions InfiltratrCommon::Portable PkgConfig::GTK3)
    filesystems_configure(filesystems)

    install(TARGETS filesystems
        RUNTIME DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}")
    # Debian/Mint-only entrypoint. The actual application binary stays in
    # /Programs/Filesystems; this script is not a second installation.
    configure_file(packaging/linux/filesystems-launcher.sh.in
        "${CMAKE_CURRENT_BINARY_DIR}/filesystems-launcher.sh" @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/filesystems-launcher.sh"
        DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME filesystems)
    install(FILES data/org.infiltrator.Filesystems.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

    # Project-owned native Linux filesystem modules are built against the
    # running kernel at install time. The coordinator and kernel transaction
    # helper are separate installed programs so desktop policy can never leak
    # into the kernel rollback implementation.
    install(PROGRAMS packaging/linux/native-module-helper.sh
        DESTINATION "lib/filesystems"
        RENAME native-module-helper)
    # Source and build helper are application-owned. The build helper never
    # installs or loads a kernel module. These two reference .ko files were
    # built for the qualification VM's kernel and are not generic host drivers.
    install(DIRECTORY "native/filesystems/fat/linux/"
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat/kernel"
        PATTERN "*.o" EXCLUDE PATTERN "*.ko" EXCLUDE
        PATTERN "*.mod" EXCLUDE PATTERN "*.mod.c" EXCLUDE
        PATTERN "Module.symvers" EXCLUDE PATTERN "modules.order" EXCLUDE)
    install(FILES native/filesystems/fat/DESIGN.md
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Sources/fat")
    install(PROGRAMS packaging/linux/fat-kernel-build.sh
        DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Helpers"
        RENAME fat-kernel-build)
    if(FILESYSTEMS_FAT_KO_REFERENCE_DIR)
        install(FILES "${FILESYSTEMS_FAT_KO_REFERENCE_DIR}/fat12.ko"
                      "${FILESYSTEMS_FAT_KO_REFERENCE_DIR}/fat16.ko"
            DESTINATION "${FILESYSTEMS_APPLICATION_ROOT}/Resources/Kernel/6.12.107-qemu-test-only")
    endif()
    install(PROGRAMS packaging/linux/kernel-module-helper.sh
        DESTINATION "lib/filesystems"
        RENAME kernel-module-helper)
    install(FILES "${FILESYSTEMS_NATIVE_MANIFEST}"
        DESTINATION "lib/filesystems"
        RENAME native-filesystems.tsv)

    # Shared kernel-safe adapter source is compiled directly into each
    # filesystem module that consumes it. It is not installed as a shared
    # runtime module and therefore cannot couple the .ko lifecycles together.
    install(DIRECTORY "native/platform/linux_kernel/"
        DESTINATION "lib/filesystems/native/platform/linux_kernel"
        FILES_MATCHING PATTERN "*.h")

    foreach(native_fs IN LISTS FILESYSTEMS_NATIVE_FILESYSTEMS)
        set(native_source_directories
            "native/filesystems/${native_fs}/linux")
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/native/filesystems/${native_fs}/core")
            list(APPEND native_source_directories
                "native/filesystems/${native_fs}/core")
        endif()
        install(DIRECTORY ${native_source_directories}
            DESTINATION "lib/filesystems/native/filesystems/${native_fs}"
            PATTERN "*.o" EXCLUDE
            PATTERN "*.ko" EXCLUDE
            PATTERN "*.mod" EXCLUDE
            PATTERN "*.mod.c" EXCLUDE
            PATTERN "Module.symvers" EXCLUDE
            PATTERN "modules.order" EXCLUDE)
    endforeach()
endif()

if(BUILD_TESTING)
    if(TARGET filesystems-ext2-dropin)
        add_executable(filesystems-ext2-dropin-test
            tests/ext2_dropin_module_test.c)
        target_link_libraries(filesystems-ext2-dropin-test
            PRIVATE filesystems-ext2-dropin)
        filesystems_configure(filesystems-ext2-dropin-test)
        add_test(NAME filesystems-ext2-dropin
            COMMAND filesystems-ext2-dropin-test)
    endif()
    if(TARGET filesystems-ofs-dropin)
        add_executable(filesystems-ofs-dropin-test
            tests/ofs_dropin_module_test.c)
        target_link_libraries(filesystems-ofs-dropin-test PRIVATE
            filesystems-ofs-dropin)
        filesystems_configure(filesystems-ofs-dropin-test)
        add_test(NAME filesystems-ofs-dropin
            COMMAND filesystems-ofs-dropin-test)
    endif()

    if(TARGET filesystems-fat16-module)
        add_executable(filesystems-fat16-module-test
            tests/fat16_module_test.c)
        target_link_libraries(filesystems-fat16-module-test PRIVATE
            filesystems-fat16-module)
        filesystems_configure(filesystems-fat16-module-test)
        add_test(NAME filesystems-fat16-module
            COMMAND filesystems-fat16-module-test)
    endif()

    if(TARGET filesystems-fat12-module)
        add_executable(filesystems-fat12-module-test
            tests/fat12_module_test.c)
        target_link_libraries(filesystems-fat12-module-test PRIVATE
            filesystems-fat12-module)
        filesystems_configure(filesystems-fat12-module-test)
        add_test(NAME filesystems-fat12-module
            COMMAND filesystems-fat12-module-test)
    endif()

    if(TARGET filesystems-fat32-module)
        add_executable(filesystems-fat32-module-test
            tests/fat32_module_test.c)
        target_link_libraries(filesystems-fat32-module-test PRIVATE
            filesystems-fat32-module)
        filesystems_configure(filesystems-fat32-module-test)
        add_test(NAME filesystems-fat32-module
            COMMAND filesystems-fat32-module-test)
    endif()

    add_executable(filesystems-catalog-test
        tests/catalog_test.cpp)
    target_link_libraries(filesystems-catalog-test
        PRIVATE filesystems-catalog)
    filesystems_configure(filesystems-catalog-test)
    add_test(
        NAME filesystems-catalog
        COMMAND filesystems-catalog-test
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/FILESYSTEMS_MATRIX.md"
                "${FILESYSTEMS_NATIVE_MANIFEST}")

    add_executable(filesystems-native-core-test
        tests/native_core_test.c)
    target_link_libraries(filesystems-native-core-test
        PRIVATE infiltratr-fs-native-core)
    filesystems_configure(filesystems-native-core-test)
    add_test(
        NAME filesystems-native-core
        COMMAND filesystems-native-core-test)

    if(TARGET filesystems-bfs-dropin)
        add_executable(filesystems-bfs-real-mkfs-test
            tests/bfs_real_mkfs_test.c)
        target_link_libraries(filesystems-bfs-real-mkfs-test PRIVATE
            filesystems-bfs-dropin)
        filesystems_configure(filesystems-bfs-real-mkfs-test)

        add_executable(filesystems-bfs-dropin-test
            tests/bfs_dropin_module_test.c)
        target_link_libraries(filesystems-bfs-dropin-test PRIVATE
            filesystems-bfs-dropin)
        filesystems_configure(filesystems-bfs-dropin-test)
        add_test(NAME filesystems-bfs-dropin COMMAND filesystems-bfs-dropin-test)
    endif()

    add_executable(filesystems-ext2-core-test
        tests/ext2_core_test.c)
    target_link_libraries(filesystems-ext2-core-test
        PRIVATE infiltratr-fs-ext2-core)
    filesystems_configure(filesystems-ext2-core-test)
    add_test(
        NAME filesystems-ext2-core
        COMMAND filesystems-ext2-core-test)

    add_executable(filesystems-ext2-engine-test
        tests/ext2_engine_test.c)
    target_link_libraries(filesystems-ext2-engine-test
        PRIVATE infiltratr-fs-ext2-efi-reader)
    filesystems_configure(filesystems-ext2-engine-test)
    add_test(
        NAME filesystems-ext2-engine
        COMMAND filesystems-ext2-engine-test)

    add_executable(filesystems-ext3-core-test
        tests/ext3_core_test.c)
    target_link_libraries(filesystems-ext3-core-test
        PRIVATE infiltratr-fs-ext3-core)
    filesystems_configure(filesystems-ext3-core-test)
    add_test(
        NAME filesystems-ext3-core
        COMMAND filesystems-ext3-core-test)

    add_executable(filesystems-ext4-core-test
        tests/ext4_core_test.c)
    target_link_libraries(filesystems-ext4-core-test
        PRIVATE infiltratr-fs-ext4-core)
    filesystems_configure(filesystems-ext4-core-test)
    add_test(
        NAME filesystems-ext4-core
        COMMAND filesystems-ext4-core-test)

    if(UNIX)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME filesystems-fat-defrag-architecture
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fat_defrag_architecture_test.py")
        add_test(NAME filesystems-ofs-image
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ofs_image_test.py"
                "$<TARGET_FILE:filesystems-ofs-image>")
        add_test(NAME filesystems-native-source-digest
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_source_digest_test.py"
                "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME filesystems-amiga-resize-errors
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/amiga_resize_error_test.py"
                "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_C_COMPILER}")
        add_test(NAME filesystems-package-transactions
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/package_transaction_test.py"
                "$<TARGET_FILE:filesystems-package-removal-guard>"
                "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CXX_COMPILER}")
    endif()

    add_executable(filesystems-ofs-core-test
        tests/ofs_core_test.c)
    target_link_libraries(filesystems-ofs-core-test
        PRIVATE infiltratr-fs-ofs-core)
    filesystems_configure(filesystems-ofs-core-test)
    add_test(NAME filesystems-ofs-core COMMAND filesystems-ofs-core-test)

    add_executable(filesystems-ofs-primitives-test
        tests/ofs_primitives_test.c)
    target_link_libraries(filesystems-ofs-primitives-test
        PRIVATE infiltratr-fs-ofs-core)
    filesystems_configure(filesystems-ofs-primitives-test)
    add_test(NAME filesystems-ofs-primitives COMMAND filesystems-ofs-primitives-test)

    add_executable(filesystems-ffs-core-test
        tests/ffs_core_test.c)
    target_link_libraries(filesystems-ffs-core-test
        PRIVATE infiltratr-fs-ffs-core)
    filesystems_configure(filesystems-ffs-core-test)
    add_test(NAME filesystems-ffs-core COMMAND filesystems-ffs-core-test)

    add_executable(filesystems-ffs-primitives-test
        tests/ffs_primitives_test.c)
    target_link_libraries(filesystems-ffs-primitives-test
        PRIVATE infiltratr-fs-ffs-core)
    filesystems_configure(filesystems-ffs-primitives-test)
    add_test(NAME filesystems-ffs-primitives COMMAND filesystems-ffs-primitives-test)

    add_executable(filesystems-sfs-core-test
        tests/sfs_core_test.c)
    target_link_libraries(filesystems-sfs-core-test
        PRIVATE infiltratr-fs-sfs-core)
    filesystems_configure(filesystems-sfs-core-test)
    add_test(
        NAME filesystems-sfs-core
        COMMAND filesystems-sfs-core-test)

    add_executable(filesystems-sfs2-core-test
        tests/sfs2_core_test.c)
    target_link_libraries(filesystems-sfs2-core-test
        PRIVATE infiltratr-fs-sfs2-core)
    filesystems_configure(filesystems-sfs2-core-test)
    add_test(
        NAME filesystems-sfs2-core
        COMMAND filesystems-sfs2-core-test)

    add_executable(filesystems-pfs3-core-test
        tests/pfs3_core_test.c)
    target_link_libraries(filesystems-pfs3-core-test
        PRIVATE infiltratr-fs-pfs3-core)
    filesystems_configure(filesystems-pfs3-core-test)
    add_test(
        NAME filesystems-pfs3-core
        COMMAND filesystems-pfs3-core-test)

    add_executable(filesystems-windows-catalog-test
        tests/windows_catalog_test.cpp)
    target_link_libraries(filesystems-windows-catalog-test
        PRIVATE filesystems-identity)
    filesystems_configure(filesystems-windows-catalog-test)
    add_test(
        NAME filesystems-windows-catalog
        COMMAND filesystems-windows-catalog-test)

    if(UNIX)
        add_executable(filesystems-removal-guard-test
            tests/removal_guard_test.cpp src/removal_guard.cpp)
        target_include_directories(filesystems-removal-guard-test
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        filesystems_configure(filesystems-removal-guard-test)
        add_test(NAME filesystems-removal-guard
            COMMAND filesystems-removal-guard-test)

        add_executable(filesystems-action-policy-test
            tests/action_policy_test.cpp)
        target_link_libraries(filesystems-action-policy-test
            PRIVATE filesystems-actions)
        filesystems_configure(filesystems-action-policy-test)
        add_test(
            NAME filesystems-action-policy
            COMMAND filesystems-action-policy-test)

        add_executable(filesystems-linux-host-paths-test
            tests/linux_host_paths_test.cpp)
        target_link_libraries(filesystems-linux-host-paths-test
            PRIVATE filesystems-core)
        filesystems_configure(filesystems-linux-host-paths-test)
        add_test(NAME filesystems-linux-host-paths
            COMMAND filesystems-linux-host-paths-test)

        add_executable(filesystems-desktop-actions-test
            tests/desktop_actions_test.cpp)
        target_link_libraries(filesystems-desktop-actions-test
            PRIVATE filesystems-actions)
        filesystems_configure(filesystems-desktop-actions-test)
        add_test(
            NAME filesystems-desktop-actions
            COMMAND filesystems-desktop-actions-test)
    endif()
endif()

if(UNIX)
    # Required so CPack includes native absolute /Programs paths in a
    # Linux .deb while respecting DESTDIR staging.
    set(CPACK_SET_DESTDIR ON)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "filesystems")
    set(CPACK_PACKAGE_VENDOR "Infiltrator Projects")
    set(CPACK_PACKAGE_CONTACT "Shannon Smith")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Filesystems - discover and install filesystem support on Debian")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "Filesystems-${PROJECT_VERSION}-amd64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Shannon Smith")
    set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "policykit-1, apt, dpkg, kmod, mokutil")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/amiga-desktop-detection.cmake")
    include(CPack)
endif()

if(BUILD_TESTING)
    add_executable(infiltratr-fs-ofs-efi-reader-test tests/ofs_efi_reader_test.c)
    target_link_libraries(infiltratr-fs-ofs-efi-reader-test
        PRIVATE infiltratr-fs-ofs-efi-reader)
    filesystems_configure(infiltratr-fs-ofs-efi-reader-test)
    add_test(NAME ofs-efi-reader COMMAND infiltratr-fs-ofs-efi-reader-test)
endif()
