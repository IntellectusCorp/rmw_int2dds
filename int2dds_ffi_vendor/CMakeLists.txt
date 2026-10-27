cmake_minimum_required(VERSION 3.12)
project(int2dds_ffi_vendor NONE)

find_package(ament_cmake REQUIRED)

# ============================================================================
# Prebuilt artifact configuration
#
#   The int2DDS GitHub release (IntellectusCorp/int2DDS, tag v<version>)
#   publishes one archive per OS and architecture:
#
#       int2dds-<version>-linux-x86_64.tar.gz       (amd64,  gnu)
#       int2dds-<version>-linux-x86_64-musl.tar.gz  (amd64,  musl)
#       int2dds-<version>-linux-aarch64.tar.gz      (arm64,  gnu)
#       int2dds-<version>-linux-aarch64-musl.tar.gz (arm64,  musl)
#       int2dds-<version>-linux-armhf.tar.gz        (armv7,  gnu)
#       int2dds-<version>-linux-armhf-musl.tar.gz   (armv7,  musl)
#       int2dds-<version>-windows-x86_64.zip / -windows-aarch64.zip
#       int2dds-<version>-macos-x86_64.tar.gz / -macos-arm64.tar.gz
#
#   Each archive unpacks to int2dds-<version>-<dist>/ with include/int2dds-ffi.h,
#   the library in lib/ (the Windows DLL in bin/), and manifest.txt (dist,
#   sha256 of the library, min_glibc).
#
#   This package downloads the archive matching the host OS, arch and libc,
#   verifies the library's sha256 against the manifest, and installs it as
#   include/ + lib/.
# ============================================================================

set(INT2DDS_FFI_VERSION "1.0.0")
set(INT2DDS_FFI_RELEASE_TAG "v${INT2DDS_FFI_VERSION}")
set(INT2DDS_FFI_BASE_URL
  "https://github.com/IntellectusCorp/int2DDS/releases/download/${INT2DDS_FFI_RELEASE_TAG}")

# glibc vs musl. ROS binaries target glibc; override with -DINT2DDS_FFI_LIBC=musl.
set(INT2DDS_FFI_LIBC "gnu" CACHE STRING "C library variant of the prebuilt int2DDS FFI (gnu|musl)")

# Local artifact override. Point this at a tarball produced by the int2DDS tree
# (an int2dds-<version>-<dist> archive in the layout above) to build against a locally built FFI
# instead of the published release asset, and to build with no network at all.
#
# This exists because the release tag does not identify the ABI: the v0.1.1 assets
# were rebuilt in place, so two different int2dds-ffi.h files have carried that same
# version. The bundled manifest is rebuilt with them, which means the sha256 check
# below cannot tell the two apart either. Naming the file you want is the only way
# to be certain which build you linked against.
#
# The sha256 verification still runs - the manifest travels inside the tarball.
# The tarball must carry INT2DDS_FFI_VERSION's library soname; a mismatched version
# fails below with "artifact not found".
set(INT2DDS_FFI_TARBALL "" CACHE FILEPATH
  "Local int2DDS FFI tarball to use instead of downloading the release asset")

# ----------------------------------------------------------------------------
# Detect OS -> archive extension
# ----------------------------------------------------------------------------
if(WIN32)
  set(_os "windows")
  set(_ext ".zip")
elseif(APPLE)
  set(_os "macos")
  set(_ext ".tar.gz")
else()
  set(_os "linux")
  set(_ext ".tar.gz")
endif()

# ----------------------------------------------------------------------------
# Detect architecture -> release dist token (<os>-<arch>[-musl])
# ----------------------------------------------------------------------------
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _proc)
if(_proc MATCHES "x86_64|amd64")
  set(_arch_token "x86_64")
elseif(_proc MATCHES "aarch64|arm64")
  set(_arch_token "aarch64")
elseif(_proc MATCHES "armv7|armhf|arm")
  set(_arch_token "armhf")
else()
  message(FATAL_ERROR "int2dds_ffi_vendor: unsupported architecture '${CMAKE_SYSTEM_PROCESSOR}'")
endif()
# The macOS archives call 64-bit ARM "arm64".
if(APPLE AND _arch_token STREQUAL "aarch64")
  set(_arch_token "arm64")
endif()

# Every Linux architecture, armhf included, has a gnu and a musl archive.
if(INT2DDS_FFI_LIBC STREQUAL "musl")
  set(_subdir "${_os}-${_arch_token}-musl")
else()
  set(_subdir "${_os}-${_arch_token}")
endif()

if(WIN32)
  set(_libname "int2dds_ffi.dll")
elseif(APPLE)
  set(_libname "libint2dds_ffi.dylib")
else()
  set(_libname "libint2dds_ffi.so.${INT2DDS_FFI_VERSION}")
endif()
set(_asset "int2dds-${INT2DDS_FFI_VERSION}-${_subdir}${_ext}")
set(_top "int2dds-${INT2DDS_FFI_VERSION}-${_subdir}")
if(WIN32)
  set(_member "${_top}/bin/${_libname}")
else()
  set(_member "${_top}/lib/${_libname}")
endif()

# ----------------------------------------------------------------------------
# Download + extract at configure time
# ----------------------------------------------------------------------------
set(_dl_dir "${CMAKE_CURRENT_BINARY_DIR}/download")
set(_extract_dir "${CMAKE_CURRENT_BINARY_DIR}/prebuilt")
set(_tarball "${_dl_dir}/${_asset}")
set(_url "${INT2DDS_FFI_BASE_URL}/${_asset}")

if(INT2DDS_FFI_TARBALL)
  if(NOT EXISTS "${INT2DDS_FFI_TARBALL}")
    message(FATAL_ERROR
      "int2dds_ffi_vendor: INT2DDS_FFI_TARBALL does not exist:\n  ${INT2DDS_FFI_TARBALL}")
  endif()
  message(STATUS "int2dds_ffi_vendor: using local tarball ${INT2DDS_FFI_TARBALL}")
  set(_tarball "${INT2DDS_FFI_TARBALL}")
  # Re-configure when the local tarball is rebuilt, so a fresh int2DDS build is
  # picked up without having to wipe the build tree by hand.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${INT2DDS_FFI_TARBALL}")
else()
  file(MAKE_DIRECTORY "${_dl_dir}")
  message(STATUS "int2dds_ffi_vendor: fetching ${_url}")
  file(DOWNLOAD "${_url}" "${_tarball}"
    STATUS _dl_status
    SHOW_PROGRESS
    TLS_VERIFY ON)
  list(GET _dl_status 0 _dl_code)
  if(NOT _dl_code EQUAL 0)
    list(GET _dl_status 1 _dl_msg)
    message(FATAL_ERROR "int2dds_ffi_vendor: download failed (${_dl_code}): ${_dl_msg}\n  URL: ${_url}")
  endif()
endif()

file(REMOVE_RECURSE "${_extract_dir}")
file(MAKE_DIRECTORY "${_extract_dir}")
file(ARCHIVE_EXTRACT INPUT "${_tarball}" DESTINATION "${_extract_dir}")

set(_header "${_extract_dir}/${_top}/include/int2dds-ffi.h")
set(_lib "${_extract_dir}/${_member}")
set(_manifest "${_extract_dir}/${_top}/manifest.txt")

if(NOT EXISTS "${_header}")
  message(FATAL_ERROR
    "int2dds_ffi_vendor: ${_top}/include/int2dds-ffi.h not found in ${_tarball}\n"
    "  (host: ${_subdir}; an archive for another OS or architecture has a different top folder)")
endif()
if(NOT EXISTS "${_lib}")
  message(FATAL_ERROR
    "int2dds_ffi_vendor: artifact '${_member}' not found in ${_asset}\n"
    "  (host: ${CMAKE_SYSTEM_PROCESSOR}, libc: ${INT2DDS_FFI_LIBC})")
endif()

# ----------------------------------------------------------------------------
# Verify the selected library against the sha256 recorded in the manifest
# ----------------------------------------------------------------------------
if(EXISTS "${_manifest}")
  # One artifact per archive: `dist:` names it, `sha256:` is the library's hash.
  file(STRINGS "${_manifest}" _manifest_lines)
  set(_expected_sha "")
  set(_manifest_dist "")
  foreach(_line IN LISTS _manifest_lines)
    if(_line MATCHES "^dist:[ \t]*([^ \t]+)")
      set(_manifest_dist "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^sha256:[ \t]*([0-9a-fA-F]+)")
      set(_expected_sha "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  if(NOT _manifest_dist STREQUAL "" AND NOT _manifest_dist STREQUAL _subdir)
    message(FATAL_ERROR
      "int2dds_ffi_vendor: ${_asset} carries '${_manifest_dist}', expected '${_subdir}'")
  endif()

  if(_expected_sha STREQUAL "")
    message(WARNING "int2dds_ffi_vendor: no sha256 for '${_member}' in manifest; skipping verification.")
  else()
    file(SHA256 "${_lib}" _actual_sha)
    string(TOLOWER "${_expected_sha}" _expected_sha)
    string(TOLOWER "${_actual_sha}" _actual_sha)
    if(NOT _actual_sha STREQUAL _expected_sha)
      message(FATAL_ERROR
        "int2dds_ffi_vendor: sha256 mismatch for '${_member}'\n"
        "  expected: ${_expected_sha}\n  actual:   ${_actual_sha}")
    endif()
    message(STATUS "int2dds_ffi_vendor: verified ${_member} (sha256 ok)")
  endif()
else()
  message(WARNING "int2dds_ffi_vendor: manifest not found in ${_asset}; skipping verification.")
endif()

# ----------------------------------------------------------------------------
# Normalise into the package install prefix: include/ + lib/
# ----------------------------------------------------------------------------
install(FILES "${_header}" DESTINATION "include")
if(WIN32)
  # The DLL goes to bin/ (found through PATH at run time), the import library to lib/.
  install(FILES "${_lib}" DESTINATION "bin")
  # The release names the import library int2dds_ffi.dll.lib; the extras file imports int2dds_ffi.lib.
  set(_implib "${_extract_dir}/${_top}/lib/int2dds_ffi.dll.lib")
  if(EXISTS "${_implib}")
    install(FILES "${_implib}" DESTINATION "lib" RENAME "int2dds_ffi.lib")
  endif()
elseif(APPLE)
  install(FILES "${_lib}" DESTINATION "lib")
else()
  file(GLOB _libfiles "${_extract_dir}/${_top}/lib/libint2dds_ffi.so*")
  install(FILES ${_libfiles} DESTINATION "lib")
endif()

ament_export_include_directories(include)
ament_package(CONFIG_EXTRAS "cmake/int2dds_ffi_vendor-extras.cmake")
