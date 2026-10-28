#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# QEMU init used by Amiga runtime qualification. The first boot exercises
# ordinary read/write semantics, then deliberately reboots with writable
# filesystems still mounted after sync. The second boot proves that committed
# state survives that unclean shutdown and remains mountable/readable.
set -eu

fail()
{
    echo "AMIGA-RUNTIME-FAIL: $*" > /dev/ttyS0
    poweroff -f
    exit 1
}

mount -t proc proc /proc || fail proc
mount -t sysfs sysfs /sys || fail sysfs
mount -t devtmpfs devtmpfs /dev || fail devtmpfs

for fs in ofs ffs sfs sfs2 pfs3; do
    insmod "/$fs.ko" || fail "insmod $fs"
done

# A freshly formatted OFS volume must expose free blocks even when
# mounted read-only. This caught the physical 119 GiB SD regression where
# statfs returned 100% used despite an empty root. Do not write to media.
mount -t ofs -o ro /dev/vda /mnt/ofs || fail "read-only OFS mount"
ofs_total=$(stat -f -c '%b' /mnt/ofs) || fail "read-only OFS statfs total"
ofs_free=$(stat -f -c '%a' /mnt/ofs) || fail "read-only OFS statfs free"
test "$ofs_total" -gt 0 && test "$ofs_free" -gt 0 &&
    test "$ofs_free" -le "$ofs_total" ||
    fail "read-only OFS free-space accounting ($ofs_free/$ofs_total)"
umount /mnt/ofs || fail "read-only OFS unmount"
echo "AMIGA-OFS-READONLY-STATFS-PASS" > /dev/ttyS0

# Exercise the full bitmap-extension chain on a sparse 120 GiB OFS image.
# The large fixture is read-only for the entire run: preserve exact media
# bytes, cross-check statfs against the geometry, and measure mount in seconds.
large_start=$(date +%s)
mount -t ofs -o ro /dev/vdf /mnt/ofs-large || fail "large OFS read-only mount"
large_end=$(date +%s)
large_total=$(stat -f -c '%b' /mnt/ofs-large) || fail "large OFS statfs total"
large_free=$(stat -f -c '%a' /mnt/ofs-large) || fail "large OFS statfs free"
test "$large_total" -gt 200000000 &&
    test "$large_free" -gt 200000000 &&
    test "$large_free" -le "$large_total" ||
    fail "large OFS free-space accounting ($large_free/$large_total)"
umount /mnt/ofs-large || fail "large OFS read-only unmount"
dmesg | grep -Eq 'OFS bitmap mount:.*batches=[1-9][0-9]* batched_blocks=[1-9][0-9]*' ||
    fail "large OFS did not exercise contiguous BIO fast path"
echo "AMIGA-OFS-LARGE-BATCHED-IO-PASS" > /dev/ttyS0
echo "AMIGA-OFS-LARGE-READONLY-PASS mount_seconds=$((large_end - large_start)) free_blocks=$large_free total_blocks=$large_total" > /dev/ttyS0

crash_phase_armed()
{
    mount -t ofs /dev/vda /mnt/ofs || fail "crash probe mount ofs"
    if test -f /mnt/ofs/.ifs-crash-phase; then
        umount /mnt/ofs || fail "crash probe unmount ofs"
        return 0
    fi
    umount /mnt/ofs || fail "initial probe unmount ofs"
    return 1
}

recover_after_unclean_shutdown()
{
    for pair in 'ofs /dev/vda' 'ffs /dev/vdb' 'sfs /dev/vdc' 'sfs2 /dev/vdd'; do
        set -- $pair
        fs="$1"
        dev="$2"
        mnt="/mnt/$fs"
        mount -t "$fs" "$dev" "$mnt" || fail "crash recovery mount $fs"
        test "$(cat "$mnt/.ifs-crash-stable")" = "committed-$fs" ||
            fail "crash recovery committed data $fs"
        rm -f "$mnt/.ifs-crash-stable" "$mnt/.ifs-crash-phase" ||
            fail "crash recovery cleanup $fs"
        sync
        umount "$mnt" || fail "crash recovery unmount $fs"
        echo "AMIGA-CRASH-RECOVERY-PASS: $fs" > /dev/ttyS0
    done

    echo "AMIGA-RUNTIME-PASS" > /dev/ttyS0
    poweroff -f
}

if crash_phase_armed; then
    recover_after_unclean_shutdown
fi

qualify_rw()
{
    fs="$1"; dev="$2"; mnt="/mnt/$fs"
    mount -t "$fs" "$dev" "$mnt" || fail "mount $fs"
    echo "runtime-$fs" > "$mnt/new.txt" || fail "create $fs"
    mkdir "$mnt/dir" || fail "mkdir $fs"
    mv "$mnt/new.txt" "$mnt/dir/renamed.txt" || fail "rename $fs"
    ln "$mnt/dir/renamed.txt" "$mnt/hard.txt" || fail "hardlink $fs"
    ln -s dir/renamed.txt "$mnt/soft.txt" || fail "symlink $fs"
    test "$(cat "$mnt/hard.txt")" = "runtime-$fs" || fail "readback $fs"
    sync
    umount "$mnt" || fail "unmount $fs"
    mount -t "$fs" "$dev" "$mnt" || fail "remount $fs"
    test "$(cat "$mnt/dir/renamed.txt")" = "runtime-$fs" || fail "remount-read $fs"
    rm "$mnt/hard.txt" "$mnt/soft.txt" "$mnt/dir/renamed.txt" || fail "unlink $fs"
    rmdir "$mnt/dir" || fail "rmdir $fs"
    sync
    umount "$mnt" || fail "final-unmount $fs"
}

qualify_sfs_rw()
{
    fs="$1"; dev="$2"; mnt="/mnt/$fs"
    mount -t "$fs" "$dev" "$mnt" || fail "mount $fs"
    echo "runtime-$fs" > "$mnt/new.txt" || fail "create $fs"
    test "$(cat "$mnt/new.txt")" = "runtime-$fs" || fail "write-readback $fs"
    # The VFS reports 512-byte sectors, independently of filesystem block size.
    dd if=/dev/zero of="$mnt/accounting.bin" bs=4096 count=256 2>/dev/null ||
        fail "accounting write $fs"
    test "$(stat -c '%b' "$mnt/accounting.bin")" = 2048 ||
        fail "sector accounting after allocation $fs"
    mkdir "$mnt/dir" || fail "mkdir $fs"
    mv "$mnt/new.txt" "$mnt/dir/renamed.txt" || fail "rename $fs"
    ln -s dir/renamed.txt "$mnt/soft.txt" || fail "symlink $fs"
    test "$(cat "$mnt/soft.txt")" = "runtime-$fs" || fail "symlink-read $fs"
    sync
    umount "$mnt" || fail "unmount $fs"
    mount -t "$fs" "$dev" "$mnt" || fail "remount $fs"
    test "$(cat "$mnt/dir/renamed.txt")" = "runtime-$fs" || fail "remount-read $fs"
    test "$(cat "$mnt/soft.txt")" = "runtime-$fs" || fail "remount-symlink $fs"
    test "$(stat -c '%b' "$mnt/accounting.bin")" = 2048 ||
        fail "sector accounting after remount $fs"
    : > "$mnt/accounting.bin" || fail "accounting truncate $fs"
    test "$(stat -c '%b' "$mnt/accounting.bin")" = 0 ||
        fail "sector accounting after truncate $fs"
    rm "$mnt/accounting.bin" || fail "accounting unlink $fs"
    : > "$mnt/dir/renamed.txt" || fail "truncate $fs"
    test ! -s "$mnt/dir/renamed.txt" || fail "truncate-readback $fs"
    rm "$mnt/soft.txt" "$mnt/dir/renamed.txt" || fail "unlink $fs"
    rmdir "$mnt/dir" || fail "rmdir $fs"
    sync
    umount "$mnt" || fail "final-unmount $fs"
}

# This project-native OFS adapter assembles per-block 488-byte payloads into
# page-cache folios. Verify that an executable actually maps and runs from
# OFS before lifting the upstream AFFS-style SB_NOEXEC restriction.
mount -t ofs /dev/vda /mnt/ofs || fail "exec probe mount ofs"
# BusyBox dispatches using argv[0]: the copied executable must bear
# the name of a compiled-in applet. The previous arbitrary filename
# "ifs-elf-probe" incorrectly failed with "applet not found" even when
# the kernel successfully executed the bytes from the OFS volume.
# Force an actual first-extension boundary and persist the same executable
# before checking ELF execution (preserved from qualified PR #30).
test "$(stat -c '%s' /bin/busybox)" -gt 35136 ||
    fail "BusyBox ELF too small to qualify OFS extension blocks"
cp /bin/busybox /mnt/ofs/echo || fail "copy static ELF to OFS"
chmod 0755 /mnt/ofs/echo || fail "chmod OFS ELF probe"
cmp /bin/busybox /mnt/ofs/echo || fail "OFS ELF bytes differ"
sync || fail "sync OFS ELF before remount"
umount /mnt/ofs || fail "unmount OFS ELF before remount"
mount -t ofs /dev/vda /mnt/ofs || fail "remount OFS ELF probe"
cmp /bin/busybox /mnt/ofs/echo || fail "OFS multi-extension ELF corrupted after remount"
echo "AMIGA-OFS-MULTIEXT-REMOUNT-PASS" > /dev/ttyS0
test -x /mnt/ofs/echo || fail "OFS ELF is not executable"
result=$(/mnt/ofs/echo "OFS-EXEC-VERIFIED") || fail "direct ELF execution from OFS"
test "$result" = "OFS-EXEC-VERIFIED" || fail "wrong output from OFS ELF"
rm /mnt/ofs/echo || fail "remove OFS ELF probe"
sync
umount /mnt/ofs || fail "unmount OFS ELF probe"
echo "AMIGA-OFS-EXEC-PASS" > /dev/ttyS0

# DOS0 high-coverage integration: exercise both OFS 488-byte payload
# boundaries and the file-header/extension-pointer boundary (>72 blocks).
# A real executable provides deterministic binary content much larger than
# a single OFS extension; it is not synthetic all-zero data.
mount -t ofs /dev/vda /mnt/ofs || fail "DOS0 extended mount"
cp /bin/busybox /mnt/ofs/Dos0Mixed.bin || fail "DOS0 extended binary copy"
cmp /bin/busybox /mnt/ofs/Dos0Mixed.bin || fail "DOS0 extended byte integrity"
test "$(stat -c '%s' /mnt/ofs/Dos0Mixed.bin)" -gt 35136 ||
    fail "DOS0 fixture too short for file-extension exercise"
mv /mnt/ofs/Dos0Mixed.bin /mnt/ofs/RenamedMixed.bin ||
    fail "DOS0 rename across casefold"
cmp /bin/busybox /mnt/ofs/renamedmixed.bin ||
    fail "DOS0 ASCII casefold lookup"
ln /mnt/ofs/RenamedMixed.bin /mnt/ofs/HardAlias ||
    fail "DOS0 large-file hardlink"
rm /mnt/ofs/RenamedMixed.bin || fail "DOS0 original hardlink unlink"
cmp /bin/busybox /mnt/ofs/hardalias ||
    fail "DOS0 promote shared data after original unlink"
mv /mnt/ofs/HardAlias /mnt/ofs/ofs-persist.bin ||
    fail "DOS0 rename surviving hardlink"
cp /bin/busybox /mnt/ofs/truncate.bin || fail "DOS0 truncate source copy"
dd if=/bin/busybox of=/mnt/ofs/truncate.bin bs=512 count=3 2>/dev/null ||
    fail "DOS0 nonzero truncate write"
test "$(stat -c '%s' /mnt/ofs/truncate.bin)" = 1536 ||
    fail "DOS0 nonzero truncate size"
dd if=/bin/busybox bs=512 count=3 2>/dev/null |
    cmp - /mnt/ofs/truncate.bin || fail "DOS0 truncated byte integrity"
: > /mnt/ofs/truncate.bin || fail "DOS0 truncate to zero"
test ! -s /mnt/ofs/truncate.bin || fail "DOS0 empty-file size"
rm /mnt/ofs/truncate.bin || fail "DOS0 truncate unlink"
sync || fail "DOS0 extended sync"
umount /mnt/ofs || fail "DOS0 extended unmount"
mount -t ofs /dev/vda /mnt/ofs || fail "DOS0 extended remount"
cmp /bin/busybox /mnt/ofs/ofs-persist.bin ||
    fail "DOS0 extended remount data integrity"
umount /mnt/ofs || fail "DOS0 extended final unmount"
echo "AMIGA-OFS-DOS0-EXTENDED-PASS" > /dev/ttyS0

qualify_rw ofs /dev/vda
qualify_rw ffs /dev/vdb
qualify_sfs_rw sfs /dev/vdc
qualify_sfs_rw sfs2 /dev/vdd
qualify_sfs_rw sfs /dev/vdg
qualify_sfs_rw sfs2 /dev/vdh

mount -t pfs3 -o ro /dev/vde /mnt/pfs3 || fail "mount pfs3"
test -s /mnt/pfs3/frag || fail "fixture-read pfs3"
cat /mnt/pfs3/frag >/dev/null || fail "payload-read pfs3"
umount /mnt/pfs3 || fail "unmount pfs3"

qualify_desktop_user()
{
    fs="$1"; dev="$2"; mnt="/mnt/$fs"
    options=$(cat "/$fs.mount-options")
    mount -t "$fs" -o "$options" "$dev" "$mnt" || fail "desktop mount $fs"
    test "$(stat -c '%u:%g' "$mnt")" = '1000:1000' || fail "desktop ownership $fs"
    su -s /bin/sh -c 'test "$(id -u)" = 1000 && mkdir "$1/desktop-dir" && echo desktop-write > "$1/desktop-dir/file" && mv "$1/desktop-dir/file" "$1/desktop-dir/renamed"' desktop sh "$mnt" || fail "desktop create/write/rename $fs"
    if su -s /bin/sh -c 'echo forbidden > "$1/other-user"' other sh "$mnt"; then
        fail "unrelated user was allowed to write $fs"
    fi
    sync
    umount "$mnt" || fail "desktop unmount $fs"
    mount -t "$fs" -o "$options" "$dev" "$mnt" || fail "desktop remount $fs"
    su -s /bin/sh -c 'test "$(cat "$1/desktop-dir/renamed")" = desktop-write && rm "$1/desktop-dir/renamed" && rmdir "$1/desktop-dir"' desktop sh "$mnt" || fail "desktop persisted read/delete $fs"
    umount "$mnt" || fail "desktop final unmount $fs"
    echo "AMIGA-DESKTOP-USER-PASS: $fs" > /dev/ttyS0
}

qualify_desktop_user ofs /dev/vda
qualify_desktop_user ffs /dev/vdb
qualify_desktop_user sfs /dev/vdc
qualify_desktop_user sfs2 /dev/vdd

# Establish known committed state, then reboot without unmounting any writable
# filesystem. This verifies crash recovery of already-committed metadata/data;
# it deliberately makes no promise about unsynchronised writes.
for pair in 'ofs /dev/vda' 'ffs /dev/vdb' 'sfs /dev/vdc' 'sfs2 /dev/vdd'; do
    set -- $pair
    fs="$1"
    dev="$2"
    mnt="/mnt/$fs"
    mount -t "$fs" "$dev" "$mnt" || fail "crash arm mount $fs"
    echo "committed-$fs" > "$mnt/.ifs-crash-stable" || fail "crash arm write $fs"
    if test "$fs" = ofs; then
        echo armed > "$mnt/.ifs-crash-phase" || fail "crash arm marker ofs"
    fi
done
sync || fail "crash arm sync"
echo "AMIGA-CRASH-ARMED" > /dev/ttyS0
reboot -f
fail "forced reboot returned"
