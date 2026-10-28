# Source and licence provenance

Original: https://github.com/Infiltrator-Projects/Filesystems
Pinned source commit: d7a572dcd0d11f7bc69cc05e09e3e944be747838
Target variant: FAT12
Imported on an isolated migration branch; not a qualified standalone driver.

This is a full, untrimmed source copy. It includes the existing Linux,
TianoCore EDK2, FatFs, Common, and defragmentation sources as found in the
pinned source, with their licence and attribution notices preserved.
Common was copied from the pinned Git submodule into an ordinary directory.
Existing Filesystems files and Defragmenter were not modified.

No published package should install this snapshot as an independent
FAT12 driver until one-variant builds, independent validation,
provenance audits and installation ownership transitions are complete.
