# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" INFILTRATR_COMMON_VERSION)
string(STRIP "${INFILTRATR_COMMON_VERSION}" INFILTRATR_COMMON_VERSION)
if(NOT INFILTRATR_COMMON_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic major.minor.patch version.")
endif()

project(InfiltratrCommon VERSION "${INFILTRATR_COMMON_VERSION}" LANGUAGES C)
include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(INFILTRATR_COMMON_BUILD_TESTS "Build Common contract and smoke tests" ${PROJECT_IS_TOP_LEVEL})
option(INFILTRATR_COMMON_WARNINGS_AS_ERRORS "Treat Common warnings as errors" ${PROJECT_IS_TOP_LEVEL})
option(INFILTRATR_COMMON_BUILD_SHARED "Build the versioned shared Common library" ON)

set(INFILTRATR_COMMON_PORTABLE_SOURCES
    src/core.c src/arithmetic.c src/config.c src/i18n.c
    src/token.c src/timing.c src/temporal.c src/temporal_clock.c src/format.c src/quantity.c src/graphics.c src/escape.c src/design.c)
set(INFILTRATR_COMMON_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes)

function(infiltratr_common_configure_target target export_name)
    target_compile_features(${target} PUBLIC c_std_11)
    target_include_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
    set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF EXPORT_NAME "${export_name}")
    if(UNIX)
        target_compile_definitions(${target} PRIVATE
            INFILTRATR_COMMON_LIBRARY_ARCHITECTURE="${CMAKE_LIBRARY_ARCHITECTURE}")
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE ${INFILTRATR_COMMON_WARNING_FLAGS})
        if(INFILTRATR_COMMON_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(infiltratr-common-portable STATIC ${INFILTRATR_COMMON_PORTABLE_SOURCES})
add_library(InfiltratrCommon::Portable ALIAS infiltratr-common-portable)
set_target_properties(infiltratr-common-portable PROPERTIES OUTPUT_NAME infiltratr-portable)
infiltratr_common_configure_target(infiltratr-common-portable Portable)

set(INFILTRATR_COMMON_FULL_SOURCES ${INFILTRATR_COMMON_PORTABLE_SOURCES} src/dynlib.c)
if(UNIX)
    list(APPEND INFILTRATR_COMMON_FULL_SOURCES
        src/posix.c src/posix_path.c src/posix_io.c src/posix_numeric.c
        src/temporal_posix.c src/temporal_calendar_posix.c)
endif()

add_library(infiltratr-common STATIC ${INFILTRATR_COMMON_FULL_SOURCES})
add_library(InfiltratrCommon::Common ALIAS infiltratr-common)
set_target_properties(infiltratr-common PROPERTIES OUTPUT_NAME infiltratr-common)
if(WIN32)
    set_target_properties(infiltratr-common PROPERTIES OUTPUT_NAME infiltratr-common-static)
endif()
infiltratr_common_configure_target(infiltratr-common Common)

if(INFILTRATR_COMMON_BUILD_SHARED)
    add_library(infiltratr-common-shared SHARED ${INFILTRATR_COMMON_FULL_SOURCES})
    add_library(InfiltratrCommon::Shared ALIAS infiltratr-common-shared)
    set_target_properties(infiltratr-common-shared PROPERTIES
        OUTPUT_NAME infiltratr-common
        VERSION "${PROJECT_VERSION}"
        SOVERSION "${PROJECT_VERSION_MAJOR}"
        WINDOWS_EXPORT_ALL_SYMBOLS ON)
    infiltratr_common_configure_target(infiltratr-common-shared Shared)
endif()

foreach(target infiltratr-common-portable infiltratr-common)
    if(UNIX)
        target_link_libraries(${target} PUBLIC m)
    endif()
endforeach()
if(INFILTRATR_COMMON_BUILD_SHARED AND UNIX)
    target_link_libraries(infiltratr-common-shared PUBLIC m)
endif()
if(CMAKE_DL_LIBS)
    target_link_libraries(infiltratr-common PUBLIC "${CMAKE_DL_LIBS}")
    if(INFILTRATR_COMMON_BUILD_SHARED)
        target_link_libraries(infiltratr-common-shared PUBLIC "${CMAKE_DL_LIBS}")
    endif()
endif()

function(infiltratr_common_add_test target source library)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${library})
    target_compile_features(${target} PRIVATE c_std_11)
    set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE ${INFILTRATR_COMMON_WARNING_FLAGS})
        if(INFILTRATR_COMMON_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

if(INFILTRATR_COMMON_BUILD_TESTS)
    infiltratr_common_add_test(infiltratr-arithmetic-smoke tests/arithmetic_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-config-smoke tests/config_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-i18n-smoke tests/i18n_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-token-smoke tests/token_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-timing-smoke tests/timing_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-temporal-smoke tests/temporal_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-graphics-smoke tests/graphics_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-escape-smoke tests/escape_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-design-smoke tests/design_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-format-smoke tests/format_smoke.c InfiltratrCommon::Common)
    infiltratr_common_add_test(infiltratr-dynlib-smoke tests/dynlib_smoke.c InfiltratrCommon::Common)
    infiltratr_common_add_test(infiltratr-portable-smoke tests/portable_smoke.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-portable-contract tests/portable_contract.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-encoding-contract tests/encoding_contract.c InfiltratrCommon::Portable)
    infiltratr_common_add_test(infiltratr-completion-contract tests/completion_contract.c InfiltratrCommon::Portable)
    if(INFILTRATR_COMMON_BUILD_SHARED)
        infiltratr_common_add_test(infiltratr-shared-smoke tests/portable_smoke.c InfiltratrCommon::Shared)
    endif()
    if(UNIX)
        infiltratr_common_add_test(infiltratr-core-smoke tests/core_smoke.c InfiltratrCommon::Common)
        infiltratr_common_add_test(infiltratr-posix-contract tests/posix_contract.c InfiltratrCommon::Common)
        infiltratr_common_add_test(infiltratr-posix-path-smoke tests/posix_path_smoke.c InfiltratrCommon::Common)
        infiltratr_common_add_test(infiltratr-posix-io-contract tests/posix_io_contract.c InfiltratrCommon::Common)
        infiltratr_common_add_test(infiltratr-posix-numeric-contract tests/posix_numeric_contract.c InfiltratrCommon::Common)
        infiltratr_common_add_test(infiltratr-temporal-posix-smoke tests/temporal_posix_smoke.c InfiltratrCommon::Common)
        add_library(infiltratr-temporal-date-fixture MODULE tests/temporal_date_fixture.c)
        target_include_directories(infiltratr-temporal-date-fixture PRIVATE include)
        infiltratr_common_add_test(infiltratr-temporal-date-smoke tests/temporal_date_smoke.c InfiltratrCommon::Common)
        target_compile_definitions(infiltratr-temporal-date-smoke PRIVATE
            TEMPORAL_DATE_FIXTURE_PATH="$<TARGET_FILE:infiltratr-temporal-date-fixture>")
        add_dependencies(infiltratr-temporal-date-smoke infiltratr-temporal-date-fixture)
    endif()
endif()

set(INFILTRATR_COMMON_INSTALL_TARGETS infiltratr-common-portable infiltratr-common)
if(INFILTRATR_COMMON_BUILD_SHARED)
    list(APPEND INFILTRATR_COMMON_INSTALL_TARGETS infiltratr-common-shared)
endif()
install(TARGETS ${INFILTRATR_COMMON_INSTALL_TARGETS}
    EXPORT InfiltratrCommonTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY include/infiltratr DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT InfiltratrCommonTargets FILE InfiltratrCommonTargets.cmake
    NAMESPACE InfiltratrCommon:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/InfiltratrCommon")
configure_package_config_file(cmake/InfiltratrCommonConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/InfiltratrCommonConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/InfiltratrCommon")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/InfiltratrCommonConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/InfiltratrCommonConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/InfiltratrCommonConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/InfiltratrTypographyAssets.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/InfiltratrCommon")
