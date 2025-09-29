#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ccap::ccap" for configuration "Release"
set_property(TARGET ccap::ccap APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ccap::ccap PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libccap.so"
  IMPORTED_SONAME_RELEASE "libccap.so"
  )

list(APPEND _cmake_import_check_targets ccap::ccap )
list(APPEND _cmake_import_check_files_for_ccap::ccap "${_IMPORT_PREFIX}/lib/libccap.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
