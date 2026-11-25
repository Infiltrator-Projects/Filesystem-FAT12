# Filesystem-FAT12 packaging and installation

## Current release contract

The sole installed filesystem engine is `FAT12.so`, with the Common V2 public entry point `infiltrator_filesystem_api_v2`. The release publishes exactly one portable amd64 Debian package and one source-downloading native RUN. GitHub generates source archives from the immutable release tag; no separate source or checksum asset is uploaded. `VERSION` supplies the package version.

- Portable: `infiltrator-filesystem-fat12_X.Y.Z_amd64.deb`
- Native: `filesystem-fat12-X.Y.Z-native-installer.run`

The package owns `/System/Filesystems/FAT12.so` and `/System/Resources/Documentation/Filesystem-FAT12.pdf`. It must not install another filesystem, a kernel module, a separate EFI driver or a consumer executable. The module's freestanding reader is a profile of the same provider, not a second installed filesystem driver.

## Native installation

The RUN is a small shell bootstrap. It fetches the exact release commit from this repository, verifies the checkout revision, and invokes the existing native builder. It contains neither a source archive nor a prebuilt Debian package. Internet access and build prerequisites are required. The default aggressive profile compiles for the local CPU, uses LTO and qualified two-pass PGO, and installs the resulting `X.Y.Z+native1` Debian package. The build-only option is for package qualification. See the [release workflow](../.github/workflows/release.yml) for exact tests and the [release page](https://github.com/Infiltrator-Projects/Filesystem-FAT12/releases) for available artifacts.

## Verification and ownership

Install only the selected provider and verify package ownership, source revision, module ABI, build profile and manual integrity. Failed installation or validation is not success. No filesystem-specific kernel or FUSE fallback is permitted. CI qualifies the exact revision before publication; existing release tags and their assets remain immutable. Historical artifacts are not the current deployment contract.

See [README](../README.md), [architecture](ONE_MODULE_END_STATE.md) and [validation](VALIDATION.md). 
