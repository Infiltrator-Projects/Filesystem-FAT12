# CP/M Filesystem Implementation

## Original editable implementation source

The real original CP/M filesystem implementation has been copied into
`native/filesystems/cpm/userspace/` directly from
`lipro-cpm4l/cpmtools` at pinned source commit
`e534e20c15973a9559e981efb498a102020e5db7`.
Fifty-five original implementation/build/source-document files are retained,
including the full `cpmfs.c`/`cpmfs.h` parser and mutation code, CP/M disk
profiles in `diskdefs`, portable/Win32 device I/O adapters, filesystem
creation/checking tools, build scripts, and the original `COPYING` file.

This is a genuine editable implementation source baseline, not a design-only
reference. Source code and copyright notices remain attributable to the
original authors where inherited.

## Native Linux module status

The copied implementation is originally userspace and is not yet a Linux
VFS driver. No `cpm.ko` has been built or qualified. The CP/M identity remains
outside the native installation manifest; current access stays tools-only.

CP/M has multiple disk definitions and layouts. A correct native kernel
adapter must make the disk profile explicit, preserve format-specific
directory/extents and block allocation logic, validate malformed media,
implement safe Linux VFS operations, and qualify persistence with separate
formatters and independently produced images. The original implementations
are the starting point and must not be replaced with empty design skeletons.

Write support must be justified separately for the supported profiles.
EFI remains a separate read-only filesystem-reader qualification.
