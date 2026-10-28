# FAT12 engineering decisions

**Standalone ownership.** The sole installation authority for `/System/Filesystems/FAT12.so` is this repository's own package `infiltrator-filesystem-fat12`. Filesystems manages selection; Defragmenter supplies a versioned generic host, not a duplicate FAT implementation.

**Duplication is intentional.** Complete supporting FAT, Linux, EDK2, portable and Common implementation source was initially copied locally for independence. Neither sibling repository is a required source dependency. After stable qualification, duplication may be reduced without changing the ABI or installed module identities.

**Read/write safety first.** Fail closed on unsupported geometry, foreign FAT width and ambiguous boundaries. Durable journal, mounted-target refusal, host identity checks and independently verified recovery are separate mandatory contracts.

**ABI-specific evidence.** A Linux shared object, kernel module and UEFI driver have different binary contracts; compilation of one cannot certify the others. The initial 0.1.x package installs only the .so; unqualified .ko and .efi drivers remain source-only.

**Release immutability.** Only successful tests against the exact main commit authorise a new version tag; publish one generic Debian package and one native source RUN, with the standard GitHub-generated source archive links. A published release is not overwritten.

**Provenance.** Copied ChaN FatFs, Linux, TianoCore, original Defragmenter and Common code retain their original notices. Documentation must not relabel imported source as newly invented implementation.
