# - Try to find GCrypt
# Once done this will define
#
#  GCRYPT_FOUND - system has GCrypt
#  GCRYPT_INCLUDE_DIRS - the GCrypt include directory
#  GCRYPT_LIBRARIES - Link these to use GCrypt
#  GCRYPT_DEFINITIONS - Compiler switches required for using GCrypt
#
#=============================================================================
#  Copyright (c) 2009-2012 Andreas Schneider <asn@cryptomilk.org>
#
#  Distributed under the OSI-approved BSD License (the "License");
#  see accompanying file Copyright.txt for details.
#
#  This software is distributed WITHOUT ANY WARRANTY; without even the
#  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  See the License for more information.
#=============================================================================
#

set(_GCRYPT_ROOT_HINTS
    $ENV{GCRYTPT_ROOT_DIR}
    ${GCRYPT_ROOT_DIR})

set(_GCRYPT_ROOT_PATHS
    "$ENV{PROGRAMFILES}/libgcrypt")

set(_GCRYPT_ROOT_HINTS_AND_PATHS
    HINTS ${_GCRYPT_ROOT_HINTS}
    PATHS ${_GCRYPT_ROOT_PATHS})


find_path(GCRYPT_INCLUDE_DIR
    NAMES
        gcrypt.h
    HINTS
        ${_GCRYPT_ROOT_HINTS_AND_PATHS}
    PATH_SUFFIXES
        include
)

find_path(GCRYPT_ERROR_INCLUDE_DIR
    NAMES
        gpg-error.h
    HINTS
        ${_GCRYPT_ROOT_HINTS_AND_PATHS}
    PATH_SUFFIXES
        include
)

find_library(GCRYPT_LIBRARY
    NAMES
        gcrypt
        gcrypt11
        libgcrypt-11
    HINTS
        ${_GCRYPT_ROOT_HINTS_AND_PATHS}
    PATH_SUFFIXES
        lib
)
find_library(GCRYPT_ERROR_LIBRARY
    NAMES
        gpg-error
        libgpg-error-0
        libgpg-error6-0
    HINTS
        ${_GCRYPT_ROOT_HINTS_AND_PATHS}
    PATH_SUFFIXES
        lib        
)
set(GCRYPT_LIBRARIES ${GCRYPT_ERROR_LIBRARY} ${GCRYPT_LIBRARY})

if (GCRYPT_INCLUDE_DIR)
    file(STRINGS "${GCRYPT_INCLUDE_DIR}/gcrypt.h" _gcrypt_version_str REGEX "^#define GCRYPT_VERSION \"[0-9]+\\.[0-9]+\\.[0-9]")

    string(REGEX REPLACE "^.*GCRYPT_VERSION.*([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" GCRYPT_VERSION "${_gcrypt_version_str}")
endif (GCRYPT_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
if (GCRYPT_VERSION)
    find_package_handle_standard_args(GCrypt
        REQUIRED_VARS
            GCRYPT_INCLUDE_DIR
            GCRYPT_LIBRARIES
        VERSION_VAR
            GCRYPT_VERSION
        FAIL_MESSAGE
            "Could NOT find GCrypt, try to set the path to GCrypt root folder in the system variable GCRYPT_ROOT_DIR"
    )
else (GCRYPT_VERSION)
    find_package_handle_standard_args(GCrypt
        "Could NOT find GCrypt, try to set the path to GCrypt root folder in the system variable GCRYPT_ROOT_DIR"
        GCRYPT_INCLUDE_DIR
        GCRYPT_LIBRARIES)
endif (GCRYPT_VERSION)

# show the GCRYPT_INCLUDE_DIRS, GCRYPT_LIBRARIES and GCRYPT_ERROR_INCLUDE_DIR variables only in the advanced view
mark_as_advanced(GCRYPT_INCLUDE_DIR GCRYPT_ERROR_INCLUDE_DIR GCRYPT_LIBRARIES)

if(GCRYPT_FOUND)
  if(NOT TARGET libgcrypt::libgcrypt)
      add_library(libgcrypt::libgcrypt UNKNOWN IMPORTED)
      set_target_properties(libgcrypt::libgcrypt PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${GCRYPT_INCLUDE_DIR}"
                            INTERFACE_LINK_LIBRARIES libgcrypt::libgcrypt
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${GCRYPT_LIBRARY}")
  endif()
  
  if(NOT TARGET libgpg-error::libgpg-error)
      add_library(libgpg-error::libgpg-error UNKNOWN IMPORTED)
      set_target_properties(libgpg-error::libgpg-error PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${GCRYPT_ERROR_INCLUDE_DIR}"
                            INTERFACE_LINK_LIBRARIES libgpg-error::libgpg-error
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${GCRYPT_ERROR_LIBRARY}")
  endif()
endif()
