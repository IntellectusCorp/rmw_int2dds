^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package int2dds_ffi_vendor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1.0.0 (2026-10-27)
------------------
* Vendor the int2DDS FFI 1.0.0 release assets in place of 0.1.8. The libraries
  are a fresh build, so the sha256 of every artifact moved.
* The soname moved from ``libint2dds_ffi.so.0`` to ``libint2dds_ffi.so.1``;
  anything linked against 0.1.8 needs a rebuild.
* Fetch one archive per OS and architecture (``int2dds-<version>-<dist>``)
  from the int2DDS release page in place of the per-OS bundle from the
  int2dds_ffi_vendor release page. The manifest's ``dist:`` is checked against
  the selected archive, and every Linux architecture, ``armhf`` included, has
  a musl archive.
* Install the Windows DLL to ``bin/`` and the macOS dylib to ``lib/``.
* Extract the archive with ``cmake -E tar`` instead of ``file(ARCHIVE_EXTRACT)``
  so the package also configures with the CMake 3.16 of Ubuntu 20.04.
* Contributors: Intellectus Corp.

0.1.8 (2026-10-07)
------------------
* Vendor the int2DDS FFI 0.1.8 release assets in place of 0.1.7. The libraries
  are a fresh build, so the sha256 of every artifact moved.
* The soname is unchanged at ``libint2dds_ffi.so.0``.
* Contributors: Intellectus Corp.

0.1.7 (2026-09-23)
------------------
* Vendor the int2DDS FFI 0.1.7 release assets in place of 0.1.6. The libraries
  are a fresh build, so the sha256 of every artifact moved.
* The soname is unchanged at ``libint2dds_ffi.so.0``.
* Contributors: Intellectus Corp.

0.1.6 (2026-09-22)
------------------
* Vendor the int2DDS FFI 0.1.6 release assets in place of 0.1.5. The libraries
  are a fresh build, so the sha256 of every artifact moved.
* The soname is unchanged at ``libint2dds_ffi.so.0``.
* Contributors: Intellectus Corp.

0.1.5 (2026-09-10)
------------------
* Vendor the int2DDS FFI 0.1.5 release assets in place of 0.1.4. The libraries
  are a fresh build, so the sha256 of every artifact moved.
* The soname is unchanged at ``libint2dds_ffi.so.0``.
* Contributors: Intellectus Corp.

0.1.4 (2026-09-04)
------------------
* Vendor the int2DDS FFI 0.1.4 release assets in place of 0.1.3. The C API is
  unchanged: the published ``int2dds-ffi.h`` differs only in added safety
  documentation, every declaration is identical. The libraries are a fresh
  build of int2DDS ``d600e70``, so the sha256 of every artifact moved.
* The glibc floor is unchanged (2.28 on x86_64 and aarch64, 2.34 on armhf) and
  so is the soname, ``libint2dds_ffi.so.0``.
* Contributors: Intellectus Corp.

0.1.3 (2026-09-02)
------------------
* Vendor the int2DDS FFI 0.1.3 release assets in place of 0.1.1. The published
  ``v0.1.3`` tarball ships the same ``int2dds-ffi.h`` as its git tag, so the
  version string identifies the ABI again; the ``v0.1.1`` assets had been
  rebuilt in place under one version and did not.
* Lower the glibc floor from 2.34 to 2.28 on x86_64 and aarch64, widening the
  set of hosts the prebuilt artifact runs on. armhf stays at 2.34 and the musl
  builds are unaffected.
* The soname is unchanged at ``libint2dds_ffi.so.0``.
* Contributors: Intellectus Corp.

0.1.1 (2026-08-28)
------------------
* No source changes; released in lockstep with ``rmw_int2dds_cpp``.
* Contributors: Intellectus Corp.

0.1.0 (2026-08-21)
------------------
* Move the package into the ``rmw_int2dds`` repository, next to
  ``rmw_int2dds_cpp``, so the two are released in lockstep. The prebuilt FFI
  tarballs are still published on the ``int2dds_ffi_vendor`` release page and
  are downloaded and sha256-verified at CMake configure time, unchanged.
* Contributors: Intellectus Corp.
