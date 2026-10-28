# SPDX-License-Identifier: GPL-3.0-or-later
# Canonical MB Corpo asset provenance used by first-party graphical consumers.
#
# Common owns the immutable source identity and hashes, not the font binaries.
# A consumer that imposes a strict no-fallback policy may fetch and package
# these exact resources. Consumers with a normal platform-fallback policy can
# simply consume the family/role contract from infiltratr/design.h.

set(INFILTRATR_MB_CORPO_SOURCE_COMMIT
    "aa161e7342112beab8feb7669f072c870f742765")
set(INFILTRATR_MB_CORPO_ARCHIVE_URL
    "https://raw.githubusercontent.com/Infiltrator-Projects/MBLink/${INFILTRATR_MB_CORPO_SOURCE_COMMIT}/assets/fonts/mb-corpo-fonts.tar.xz")
set(INFILTRATR_MB_CORPO_ARCHIVE_SHA256
    "bdb6063f838a7fab22b4d6b412170640c69511df53aa3dfa9a4ea8431c9d8274")

set(INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE "mb_corpo_a_cond_regular.ttf")
set(INFILTRATR_MB_CORPO_UI_BOLD_FILE "mb_corpo_s_bold.ttf")
set(INFILTRATR_MB_CORPO_UI_REGULAR_FILE "mb_corpo_s_regular.ttf")

set(INFILTRATR_MB_CORPO_BRAND_REGULAR_SHA256
    "c8bcd7e1a7d71169b38491d9b7c1ffe7ba7b46e888f0c1219931343a47bc0e05")
set(INFILTRATR_MB_CORPO_UI_BOLD_SHA256
    "d37ea986e2344d83390f94f170e6272b56efd00bfec808afe8314c4ca45d43b4")
set(INFILTRATR_MB_CORPO_UI_REGULAR_SHA256
    "94ede6629443c03d4362dcef425fb3ff520be5d654370021a34e81286804465c")
