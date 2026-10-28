# Native UEFI storage providers (experimental)

Infiltrator Boot 0.1.34 can load opt-in hardware-specific EFI storage
providers from `storage_x64/` before format drivers in `drivers_x64/`.
A provider must independently identify and **self-bind** to its compatible
controller during EFI startup, publish standard `EFI_BLOCK_IO_PROTOCOL`
and normally `EFI_DISK_IO_PROTOCOL` handles, and allow normal UEFI
partition drivers to discover partitions. Existing filesystem drivers
(OFS, ext4, etc.) remain unaware of the hardware implementation.

## Qualification gates

1. Implement actual controller-backed storage operations (PCI I/O, SD
   command/response handling, media initialization, timeouts and recovery).
   A one-bit PCI link ASPM workaround is **not** a storage provider.
2. Match hardware by PCI IDs and capabilities and leave unrelated
   computers, their existing firmware BlockIo handles and NVRAM alone.
3. Prevent duplicate conflicting ownership with the firmware SD driver,
   preserve a working fallback, and avoid changing any boot order.
4. Validate read-only device probing, large-offset and random reads,
   verified file contents, exceptional failures, and OVMF/physical hardware.
5. Only publish/install a hardware provider automatically once the
   physical test has completed successfully without 30-second failures.

The archived `experimental/rtsx525a/` sources are a failed ASPM
experiment from OFS 0.5.73 and are **not** a native SD controller driver
or a successful workaround. No firmware transport bypass is currently
qualified. Boot's missing `storage_x64/` directory must be a no-op
on other computers.
