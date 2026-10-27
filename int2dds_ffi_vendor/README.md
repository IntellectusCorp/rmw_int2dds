# int2dds_ffi_vendor

ROS 2 vendor package that fetches the **prebuilt int2DDS FFI library** for the
host platform at build time and exposes it to colcon/ament as an imported CMake
target. It is the dependency boundary between the Rust `int2DDS` core (shipped as
prebuilt binaries) and the C++ `rmw_int2dds_cpp` middleware.

## What it does

1. Detects the host OS, architecture, and libc (gnu/musl).
2. Downloads the release asset for that OS, architecture and libc from the
   [int2DDS releases](https://github.com/IntellectusCorp/int2DDS/releases)
   page.
3. Reads the manifest in the archive and verifies the library's sha256.
4. Installs `int2dds-ffi.h` + the platform shared library as `include/` + `lib/`
   (on Windows the DLL goes to `bin/` and the import library to `lib/`).
5. Exports the imported target `int2dds_ffi::int2dds_ffi`.

## Consuming it (downstream)

```cmake
find_package(int2dds_ffi_vendor REQUIRED)
target_link_libraries(my_target int2dds_ffi::int2dds_ffi)
```

```xml
<!-- package.xml -->
<depend>int2dds_ffi_vendor</depend>
```

## Release asset layout

The int2DDS release tagged `v${INT2DDS_FFI_VERSION}` (see
[CMakeLists.txt](CMakeLists.txt)) publishes one archive per OS and architecture:
`int2dds-<version>-<dist>.tar.gz` (`.zip` on Windows), where `<dist>` is
`linux-x86_64`, `linux-x86_64-musl`, `linux-aarch64`, `linux-aarch64-musl`,
`linux-armhf`, `linux-armhf-musl`, `windows-x86_64`, `windows-aarch64`,
`macos-x86_64` or `macos-arm64`. The Linux library carries the usual SONAME
symlink chain:

```
int2dds-<version>-linux-x86_64.tar.gz
└── int2dds-<version>-linux-x86_64/
    ├── include/int2dds-ffi.h                  # C API header
    ├── lib/
    │   ├── libint2dds_ffi.so.<version>        #   real file
    │   ├── libint2dds_ffi.so.1 -> .so.<version>  # SONAME
    │   └── libint2dds_ffi.so   -> .so.1       #   linker name
    ├── manifest.txt                           # dist + sha256 + soname + min_glibc
    └── LICENSE
```

On Windows the DLL is `bin/int2dds_ffi.dll` and the import library
`lib/int2dds_ffi.dll.lib` (installed as `lib/int2dds_ffi.lib`).

`manifest.txt`:

```yaml
name: int2dds
version: "1.0.0"
api_header: int2dds-ffi.h
license: Apache-2.0
triple: x86_64-unknown-linux-gnu
dist: linux-x86_64
sha256: <hex>
soname: libint2dds_ffi.so.1
min_glibc: "2.28"
```

The vendor package checks that `dist:` matches the archive it selected and that
`sha256:` matches the library, so no SHA values need to be hard-coded here.

### libc selection

The default is glibc (`gnu`), which is what ROS binaries target. Each gnu
archive's `manifest.txt` names the oldest glibc it runs on (`min_glibc`). For a
musl host:

```bash
colcon build --cmake-args -DINT2DDS_FFI_LIBC=musl
```

Every Linux architecture, `armhf` included, has a musl archive.
