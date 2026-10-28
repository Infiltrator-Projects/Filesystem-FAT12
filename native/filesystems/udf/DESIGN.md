# UDF Native Filesystem Implementation

## Actual original implementation

The original Linux UDF filesystem source is directly editable under
`native/filesystems/udf/linux/`. Its original on-disk decoding and
Linux VFS semantics remain intact, with source licences and attribution.
No duplicate FUSE or rewritten imitation replaces this original code.

## Native Linux driver status

The original Linux module now builds independently as `udf.ko` using
explicit out-of-tree Kbuild registration. The single UDF catalogue identity
is managed by `data/native-filesystems.tsv`; the existing Linux native
module helper owns kernel-version matching, source freshness, signing,
module selection, safe removal and rollback.

The actual source passed Debian 13 exported-symbol linking and a separate
Linux 6.12 QEMU test
(https://github.com/Infiltrator-Projects/Filesystems/actions/runs/39932498942).
This test booted a kernel without built-in UDF support, loaded the project's
original `udf.ko`, mounted UDF 2.01 hard-disk media independently formatted
by `mkudffs`, created/renamed files, wrote 128 KiB of data, unmounted/remounted
and verified persistence. A separate host Linux kernel then mounted the same
media read-only and independently confirmed both files and contents.

## Remaining qualification

Other UDF revisions, VAT/sparable/metadata partition variants, optical
media compatibility, unclean power-loss recovery, invalid volume metadata,
write durability, formatter/repair workflows and physical media need
additional independent validation before their support can be claimed.

The corresponding read-only UEFI `.efi` driver and any InfiltratorOS
root/setup qualification remain separate unfinished work.
