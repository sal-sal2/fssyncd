find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_SODIUM QUIET libsodium)
endif()

find_path(Sodium_INCLUDE_DIR
  NAMES sodium.h
  HINTS ${PC_SODIUM_INCLUDE_DIRS}
  PATH_SUFFIXES include
)

find_library(Sodium_LIBRARY
  NAMES sodium libsodium
  HINTS ${PC_SODIUM_LIBRARY_DIRS}
  PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sodium
  REQUIRED_VARS Sodium_LIBRARY Sodium_INCLUDE_DIR
  VERSION_VAR PC_SODIUM_VERSION
)

if(Sodium_FOUND AND NOT TARGET Sodium::sodium)
  add_library(Sodium::sodium UNKNOWN IMPORTED)
  set_target_properties(Sodium::sodium PROPERTIES
    IMPORTED_LOCATION "${Sodium_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Sodium_INCLUDE_DIR}"
  )
endif()

if(Sodium_FOUND AND NOT TARGET sodium)
  add_library(sodium INTERFACE)
  target_link_libraries(sodium INTERFACE Sodium::sodium)
endif()

mark_as_advanced(Sodium_INCLUDE_DIR Sodium_LIBRARY)