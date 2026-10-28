# Filesystem FAT12

Standalone filesystem repository migration in progress.
This migration branch contains the full original Filesystems source at
commit `d7a572dcd0d11f7bc69cc05e09e3e944be747838` as a safety-preserving starting snapshot.
It intentionally contains unrelated filesystem code to be trimmed after
independent FAT12.so, fat12.ko and FAT12.efi
builds, tests and packaging are created. No release is authorised by this copy.

The FAT12 defragmentation and recovery engine must remain in
FAT12.so with the ABI expected by Defragmenter. The central
Filesystems repository and Defragmenter have not been modified.
The original upstream README is FILESYSTEMS-SOURCE-README.md.
