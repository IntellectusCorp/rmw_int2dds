# Exposes the prebuilt int2DDS FFI library as an imported target.
# Consumed by downstream packages via find_package(int2dds_ffi_vendor).
#
# Provides:
#   int2dds_ffi::int2dds_ffi   - imported shared library + include dir
#   int2dds_ffi_INCLUDE_DIRS   - include directory
#   int2dds_ffi_LIBRARY        - full path to the link library

# ${int2dds_ffi_vendor_DIR} is <prefix>/share/int2dds_ffi_vendor/cmake at use time.
get_filename_component(_int2dds_ffi_prefix
  "${int2dds_ffi_vendor_DIR}/../../.." ABSOLUTE)

set(_int2dds_ffi_include "${_int2dds_ffi_prefix}/include")
set(_int2dds_ffi_libdir  "${_int2dds_ffi_prefix}/lib")

if(WIN32)
  set(_int2dds_ffi_implib  "${_int2dds_ffi_libdir}/int2dds_ffi.lib")
  set(_int2dds_ffi_runtime "${_int2dds_ffi_libdir}/int2dds_ffi.dll")
elseif(APPLE)
  set(_int2dds_ffi_lib "${_int2dds_ffi_libdir}/libint2dds_ffi.dylib")
else()
  set(_int2dds_ffi_lib "${_int2dds_ffi_libdir}/libint2dds_ffi.so")
endif()

if(NOT TARGET int2dds_ffi::int2dds_ffi)
  add_library(int2dds_ffi::int2dds_ffi SHARED IMPORTED)
  set_target_properties(int2dds_ffi::int2dds_ffi PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_int2dds_ffi_include}")
  if(WIN32)
    set_target_properties(int2dds_ffi::int2dds_ffi PROPERTIES
      IMPORTED_IMPLIB   "${_int2dds_ffi_implib}"
      IMPORTED_LOCATION "${_int2dds_ffi_runtime}")
    set(int2dds_ffi_LIBRARY "${_int2dds_ffi_implib}")
  else()
    set_target_properties(int2dds_ffi::int2dds_ffi PROPERTIES
      IMPORTED_LOCATION "${_int2dds_ffi_lib}"
      IMPORTED_NO_SONAME TRUE)
    set(int2dds_ffi_LIBRARY "${_int2dds_ffi_lib}")
  endif()
endif()

set(int2dds_ffi_INCLUDE_DIRS "${_int2dds_ffi_include}")
