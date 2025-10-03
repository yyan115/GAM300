#----------------------------------------------------------------
# Generated CMake target import file for configuration "Distribution".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Jolt::Jolt" for configuration "Distribution"
set_property(TARGET Jolt::Jolt APPEND PROPERTY IMPORTED_CONFIGURATIONS DISTRIBUTION)
set_target_properties(Jolt::Jolt PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DISTRIBUTION "CXX"
  IMPORTED_LOCATION_DISTRIBUTION "${_IMPORT_PREFIX}/lib/Jolt.lib"
  )

list(APPEND _cmake_import_check_targets Jolt::Jolt )
list(APPEND _cmake_import_check_files_for_Jolt::Jolt "${_IMPORT_PREFIX}/lib/Jolt.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
