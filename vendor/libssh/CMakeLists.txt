cmake_minimum_required(VERSION 3.12.0)

# Specify search path for CMake modules to be loaded by include()
# and find_package()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

# Add defaults for cmake
# Those need to be set before the project() call.
include(DefineCMakeDefaults)
include(DefineCompilerFlags)

project(libssh VERSION 0.11.2 LANGUAGES C)

# global needed variable
set(APPLICATION_NAME ${PROJECT_NAME})

# SOVERSION scheme: CURRENT.AGE.REVISION
#   If there was an incompatible interface change:
#     Increment CURRENT. Set AGE and REVISION to 0
#   If there was a compatible interface change:
#     Increment AGE. Set REVISION to 0
#   If the source code was changed, but there were no interface changes:
#     Increment REVISION.
set(LIBRARY_VERSION "4.10.2")
set(LIBRARY_SOVERSION "4")

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked

# add definitions
include(DefinePlatformDefaults)
include(DefineOptions.cmake)
include(CPackConfig.cmake)
include(GNUInstallDirs)

include(CompilerChecks.cmake)

# disallow in-source build
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out of source build. Please create a separate build directory and run 'cmake /path/to/${PROJECT_NAME} [options]' there.")

# Copy library files to a lib sub-directory
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# search for libraries
if (WITH_ZLIB)
    find_package(ZLIB REQUIRED)
endif (WITH_ZLIB)

if (WITH_GCRYPT)
  find_package(GCrypt 1.5.0 REQUIRED)
  message(WARNING "libgcrypt cryptographic backend is deprecated and will be removed in future releases.")
elseif(WITH_MBEDTLS)
    find_package(MbedTLS REQUIRED)
else()
  find_package(OpenSSL 1.1.1 REQUIRED)
endif()

if (UNIT_TESTING)
    find_package(CMocka REQUIRED)
endif ()

# Find out if we have threading available
set(CMAKE_THREAD_PREFER_PTHREADS ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

if (WITH_GSSAPI)
    find_package(GSSAPI)
endif (WITH_GSSAPI)

if (WITH_NACL)
    find_package(NaCl)
    if (NOT NACL_FOUND)
        set(WITH_NACL OFF)
    endif (NOT NACL_FOUND)
endif (WITH_NACL)

# Disable symbol versioning in non UNIX platforms
if (UNIX)
    find_package(ABIMap 0.3.1)
else (UNIX)
    set(WITH_SYMBOL_VERSIONING OFF)
endif (UNIX)

# config.h checks
include(ConfigureChecks.cmake)
configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

if (NOT HAVE_ARGP_PARSE)
  find_package(Argp)
endif (NOT HAVE_ARGP_PARSE)

# check subdirectories
add_subdirectory(doc)
add_subdirectory(include)
add_subdirectory(src)

# pkg-config file
if (UNIX OR MINGW)
configure_file(libssh.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libssh.pc @ONLY)
install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libssh.pc
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  COMPONENT
    pkgconfig
)
endif (UNIX OR MINGW)

# CMake config files
include(CMakePackageConfigHelpers)

set(LIBSSH_LIBRARY_NAME ${CMAKE_SHARED_LIBRARY_PREFIX}ssh${CMAKE_SHARED_LIBRARY_SUFFIX})

# libssh-config-version.cmake
write_basic_package_version_file(libssh-config-version.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY SameMajorVersion)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    COMPONENT
        devel)

if (WITH_EXAMPLES)
    add_subdirectory(examples)
endif (WITH_EXAMPLES)

if (UNIT_TESTING)
    include(AddCMockaTest)
    add_subdirectory(tests)
endif (UNIT_TESTING)

### SOURCE PACKAGE
if (WITH_SYMBOL_VERSIONING AND ABIMAP_FOUND)
    # Get the current ABI version from source
    get_filename_component(current_abi_path
                           "${CMAKE_SOURCE_DIR}/src/ABI/current"
                           ABSOLUTE)

    # Check if the ABI version should be updated
    file(READ ${current_abi_path} CURRENT_ABI_CONTENT)
    string(STRIP "${CURRENT_ABI_CONTENT}" CURRENT_ABI_VERSION)

    if (LIBRARY_VERSION VERSION_GREATER CURRENT_ABI_VERSION)
        set(UPDATE_ABI TRUE)
    endif ()

    if (UPDATE_ABI)
        message(STATUS "Library version bumped to ${LIBRARY_VERSION}: Updating ABI")

        # Get the list of header files
        get_file_list(${PROJECT_NAME}_header_list
                      DIRECTORIES "${CMAKE_SOURCE_DIR}/include/libssh"
                      FILES_PATTERNS "*.h")

        # Extract the symbols marked as "LIBSSH_API" from the header files
        extract_symbols(${PROJECT_NAME}.symbols
                        HEADERS_LIST ${PROJECT_NAME}_header_list
                        FILTER_PATTERN "LIBSSH_API"
                        COPY_TO "${CMAKE_SOURCE_DIR}/src/ABI/${PROJECT_NAME}-${LIBRARY_VERSION}.symbols")

        if (WITH_ABI_BREAK)
            set(ALLOW_ABI_BREAK "BREAK_ABI")
        endif()

        # Target we can depend on in 'make dist'
        set(_SYMBOL_TARGET "${PROJECT_NAME}.map")

        # Set the path to the current map file
        set(MAP_PATH "${CMAKE_SOURCE_DIR}/src/${_SYMBOL_TARGET}")

        # Generate the symbol version map file
        generate_map_file(${_SYMBOL_TARGET}
                          SYMBOLS ${PROJECT_NAME}.symbols
                          RELEASE_NAME_VERSION ${PROJECT_NAME}_${LIBRARY_VERSION}
                          CURRENT_MAP ${MAP_PATH}
                          COPY_TO ${MAP_PATH}
                          FINAL
                          ${ALLOW_ABI_BREAK})

        # Write the current version to the source
        file(WRITE ${current_abi_path} ${LIBRARY_VERSION})
    endif(UPDATE_ABI)
endif (WITH_SYMBOL_VERSIONING AND ABIMAP_FOUND)

# Coverage
if (WITH_COVERAGE)
    ENABLE_LANGUAGE(CXX)
    include(CodeCoverage)
    setup_target_for_coverage_lcov(
        NAME "coverage"
        EXECUTABLE make test
        DEPENDENCIES ssh tests)
    set(GCOVR_ADDITIONAL_ARGS --xml-pretty --exclude-unreachable-branches --print-summary)
    setup_target_for_coverage_gcovr_xml(
        NAME "coverage_xml"
        EXECUTABLE make test
        DEPENDENCIES ssh tests)
endif (WITH_COVERAGE)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source DEPENDS ${_SYMBOL_TARGET} VERBATIM)

get_directory_property(hasParent PARENT_DIRECTORY)
if(NOT(hasParent))
  # Link compile database for clangd if we are the master project
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json")
endif()

message(STATUS "********************************************")
message(STATUS "********** ${PROJECT_NAME} build options : **********")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Coverage: ${WITH_COVERAGE}")
message(STATUS "zlib support: ${WITH_ZLIB}")
message(STATUS "libgcrypt support: ${WITH_GCRYPT}")
message(STATUS "libmbedTLS support: ${WITH_MBEDTLS}")
message(STATUS "libnacl support: ${WITH_NACL}")
message(STATUS "SFTP support: ${WITH_SFTP}")
message(STATUS "Server support : ${WITH_SERVER}")
message(STATUS "GSSAPI support : ${WITH_GSSAPI}")
message(STATUS "GEX support : ${WITH_GEX}")
message(STATUS "Support insecure none cipher and MAC : ${WITH_INSECURE_NONE}")
message(STATUS "Support exec : ${WITH_EXEC}")
message(STATUS "Pcap debugging support : ${WITH_PCAP}")
message(STATUS "Build shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "Unit testing: ${UNIT_TESTING}")
message(STATUS "Client code testing: ${CLIENT_TESTING}")
message(STATUS "Blowfish cipher support: ${HAVE_BLOWFISH}")
message(STATUS "PKCS #11 URI support: ${WITH_PKCS11_URI}")
message(STATUS "With PKCS #11 provider support: ${WITH_PKCS11_PROVIDER}")
set(_SERVER_TESTING OFF)
if (WITH_SERVER)
    set(_SERVER_TESTING ${SERVER_TESTING})
endif()
message(STATUS "Server code testing: ${_SERVER_TESTING}")
if (WITH_INTERNAL_DOC)
    message(STATUS "Internal documentation generation")
else (WITH_INTERNAL_DOC)
    message(STATUS "Public API documentation generation")
endif (WITH_INTERNAL_DOC)
message(STATUS "Benchmarks: ${WITH_BENCHMARKS}")
message(STATUS "Symbol versioning: ${WITH_SYMBOL_VERSIONING}")
message(STATUS "Allow ABI break: ${WITH_ABI_BREAK}")
message(STATUS "Release is final: ${WITH_FINAL}")
message(STATUS "Global client config: ${GLOBAL_CLIENT_CONFIG}")
if (WITH_SERVER)
message(STATUS "Global bind config: ${GLOBAL_BIND_CONFIG}")
endif()
message(STATUS "********************************************")

