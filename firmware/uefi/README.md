# Independent EFI filesystem drivers

Filesystems owns the firmware filesystem adapters in this directory. Boot
contains their generic UEFI loader, but does not compile, vendor or package any
filesystem driver. Drivers implement EFI Driver Binding and EFI Simple File
System and can be installed alongside any compatible boot manager.

The `filesystems/` source and its licences were moved from Boot commit
`9043feba1b72e41969dbf26c5a65aedbea0fe0bc`. `IMPORT.json` records each original
source path and SHA-256 digest. The implementation files were preserved byte
for byte, including the dormant adapters and pinned InfiltratorFS portable
source. `Make.common`, the platform headers and the source credits/licences
provide the imported build support; no Boot source checkout is required.

Build the maintained x86-64 driver payload with GNU-EFI, GCC, make and binutils:

```sh
bash scripts/build-efi-drivers.sh build-efi
```

The established ext4 reader is emitted as `drivers_x64/ext4_x64.efi`.
The independent OFS implementation is built from `native/filesystems/ofs/` and
emitted as `experimental/OFS_x64.efi`. Its experimental status remains explicit.
The pinned InfiltratorFS 0.18 adapter and other imported drivers are retained as
migration source; they are not added to the maintained binary payload by this
move. The canonical InfiltratorFS project continues to own its format and core.

For Debian release builds, configure CMake with
`-DFILESYSTEMS_BUILD_EFI_DRIVERS=ON`. Filesystems installs its driver payload under
`/usr/lib/filesystems/efi/`. This is component storage, not an EFI installation:
neither building nor installing the package writes to an ESP, modifies NVRAM,
changes Boot configuration or installs a kernel. Setup, or an explicit local
test, selects the required drivers and places them in the chosen boot manager's
driver directory. There are no machine-specific destinations in this build.

The imported makefiles and EDK2 metadata retain their historical names and
upstream assumptions. The script above is the maintained independent build;
other adapters/toolchains require their own qualification before distribution.
