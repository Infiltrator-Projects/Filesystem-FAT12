# Amiga Fast File System (FFS) — Canonical Format Specification

**Scope:** This document owns the **FFS** DOSTypes and on-media FFS invariants, separately from [the OFS specification](../ofs/OFS_SPECIFICATION.md). It is the FFS format reference, not a status report. **Independent historical interoperability and complete FFS module qualification are not yet certified.**

## DOSType identifiers — FFS only

| Big-endian ID | Binary four-byte name | Format | Variant rules |
| --- | --- | --- | --- |
| `0x444F5301` | `DOS\1` | Original Fast File System | Classic 30-byte filename metadata; FFS raw data blocks |
| `0x444F5303` | `DOS\3` | International FFS | International Latin-1 folding; classic 30-byte metadata |
| `0x444F5305` | `DOS\5` | Directory-cache FFS | International folding; classic directory metadata plus cache blocks |
| `0x444F5307` | `DOS\7` | Long-name FFS (LNFS) | International folding; separate NaC name/comment and type-64 overflow comment format |

The four-byte DOSType consists of the three ASCII bytes `DOS` plus a **binary** selector byte. All FFS identifiers are odd-numbered; all OFS identifiers are specified in the OFS document and must **not** be reinterpreted as FFS.

## Canonical data-layout distinction

- All on-disk longwords are **big-endian**, with partition-relative **unsigned BE32** block keys and 32-bit file byte lengths. The 512..32768-byte classic filesystem-block size range and correct separation from physical-device sectors must be tested.
- Root and directory checksum, hash bucket and bitmap layouts are related to OFS, but FFS's **regular-file data blocks are raw payload bytes**. Do **not** place the OFS 24-byte header, OFS data-block checksum, sequence number or next-data pointer inside an FFS data block.
- Files have a `T_HEADER=2` file metadata block; their data-block pointers are held in the header's reversed pointer table, with `T_LIST=16` extension metadata when more pointer entries are needed. The payload per FFS data block is the **entire filesystem block** (`B` bytes), not `B-24`.
- Directory enumeration and lookup use the correct DOSType-specific case folding. On international profiles, hash and equality comparisons must consistently perform the Latin-1 fold. A directory-cache writer must maintain valid cache records; read-only parsers may fall back to authoritative directory hash chains after compatibility checks.
- For `DOS\7`, all non-root entry headers must use the authentic LNFS 112-byte `NaC` field at `B-184` with name and comment as two adjacent BCPL strings; overflow comment pointer at `B-72`; shifted DateStamp at `B-60`; a type-64 comment block when necessary. The root volume name remains 30 bytes; the root repeats its DOSType at `B-16` and has an allocated-block counter at `B-44`. LNFS uses international folding, **not** directory caching. Hyperion's format guidance specifies a conservative 106-character writer limit.
- The format's 32-bit pointers do **not** justify a universal signed-31-bit block limit. Device transfer geometry, large-address overflow, bitmaps and mount/write qualifications are separate from format arithmetic.

## Conformance contract

A correct FFS implementation verifies and preserves metadata and allocation checksums, root discovery and bitmap accounting; supports files crossing pointer-extension boundaries; reads/writes the full `B`-byte payload; handles directory hash collisions, rename and deletion; and rejects damaged or foreign media without reinterpreting it.

Before adding writable `DOS\7` support, test NaC names at 30, 31 and 106 characters, comment overflow allocation/free, Latin-1 hash chains, root signature and used-block counter updates, and actual reader/writer interoperability. As with OFS, a successful build, trivial empty image or passing self-verifier does not alone establish historic AmigaOS compatibility.

**Original sources:** [AmigaDOS ROM Kernel Reference Manual (DOS)](https://developer.amigaos3.net/sites/default/files/downloads/2024-10/Amiga_ROM_Kernel_Reference_Manual_DOS.pdf), [Olaf Barthel's official DCFS/LNFS on-disk structures](https://wiki.amigaos.net/wiki/DCFS_and_LNFS_Low_Level_Data_Structures), [ADFlib disk-format description](https://adflib.github.io/FAQ/adf_info.html), [Linux AFFS documentation](https://www.kernel.org/doc/html/v6.7/filesystems/affs.html), and [Hyperion's long-filename formatter FAQ](https://www.hyperion-entertainment.com/index.php/frequently-asked-questions/55-amigaos-314/204-long-filenames-i-was-promised-long-filenames-how-do-i-use-them).

**Status:** The four FFS format profiles are documented. This document is an initial canonical contract; exhaustive record-by-record golden fixtures, independent tests and complete FFS module support must be recorded before declaring full specification qualification.
