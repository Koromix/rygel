set(LIBSSH_PUBLIC_INCLUDE_DIRS ${libssh_SOURCE_DIR}/include)

set(LIBSSH_PRIVATE_INCLUDE_DIRS
  ${libssh_BINARY_DIR}/include
  ${libssh_BINARY_DIR}
)

set(LIBSSH_LINK_LIBRARIES
  ${LIBSSH_REQUIRED_LIBRARIES}
)

if (TARGET OpenSSL::Crypto)
  list(APPEND LIBSSH_LINK_LIBRARIES OpenSSL::Crypto)
endif ()

if (TARGET MbedTLS::mbedcrypto)
  list(APPEND LIBSSH_LINK_LIBRARIES MbedTLS::mbedcrypto)
endif ()

if (TARGET libgcrypt::libgcrypt)
  list(APPEND LIBSSH_LINK_LIBRARIES ${GCRYPT_LIBRARIES})
endif ()

if (WITH_ZLIB)
  list(APPEND LIBSSH_LINK_LIBRARIES ZLIB::ZLIB)
endif (WITH_ZLIB)

if (WITH_GSSAPI AND GSSAPI_FOUND)
  set(LIBSSH_PRIVATE_INCLUDE_DIRS
    ${LIBSSH_PRIVATE_INCLUDE_DIRS}
    ${GSSAPI_INCLUDE_DIR}
  )

  set(LIBSSH_LINK_LIBRARIES
    ${LIBSSH_LINK_LIBRARIES}
    ${GSSAPI_LIBRARIES}
  )
endif (WITH_GSSAPI AND GSSAPI_FOUND)

if (WITH_NACL AND NACL_FOUND)
  set(LIBSSH_PRIVATE_INCLUDE_DIRS
    ${LIBSSH_PRIVATE_INCLUDE_DIRS}
    ${NACL_INCLUDE_DIR}
  )

  set(LIBSSH_LINK_LIBRARIES
    ${LIBSSH_LINK_LIBRARIES}
    ${NACL_LIBRARY}
  )
endif (WITH_NACL AND NACL_FOUND)

if (MINGW AND Threads_FOUND)
  set(LIBSSH_LINK_LIBRARIES
    ${LIBSSH_LINK_LIBRARIES}
    Threads::Threads
  )
endif()

# The ws2_32 needs to be last for mingw to build
# https://gitlab.com/libssh/libssh-mirror/-/issues/84
if (WIN32)
  set(LIBSSH_LINK_LIBRARIES
    ${LIBSSH_LINK_LIBRARIES}
    iphlpapi
    ws2_32
  )
endif (WIN32)

if (BUILD_STATIC_LIB)
  set(LIBSSH_STATIC_LIBRARY
    ssh_static
    CACHE INTERNAL "libssh static library"
  )
endif (BUILD_STATIC_LIB)

set(libssh_SRCS
  agent.c
  auth.c
  base64.c
  bignum.c
  buffer.c
  callbacks.c
  channels.c
  client.c
  config.c
  connect.c
  connector.c
  crypto_common.c
  curve25519.c
  dh.c
  ecdh.c
  error.c
  getpass.c
  gzip.c
  init.c
  kdf.c
  kex.c
  known_hosts.c
  knownhosts.c
  legacy.c
  log.c
  match.c
  messages.c
  misc.c
  options.c
  packet.c
  packet_cb.c
  packet_crypt.c
  pcap.c
  pki.c
  pki_container_openssh.c
  poll.c
  session.c
  scp.c
  socket.c
  string.c
  threads.c
  ttyopts.c
  wrapper.c
  external/bcrypt_pbkdf.c
  external/blowfish.c
  config_parser.c
  token.c
  pki_ed25519_common.c
)

if (DEFAULT_C_NO_DEPRECATION_FLAGS)
    set_source_files_properties(known_hosts.c
                                PROPERTIES
                                    COMPILE_FLAGS ${DEFAULT_C_NO_DEPRECATION_FLAGS})
endif()

if (CMAKE_USE_PTHREADS_INIT)
    set(libssh_SRCS
        ${libssh_SRCS}
        threads/noop.c
        threads/pthread.c
    )
elseif (CMAKE_USE_WIN32_THREADS_INIT)
        set(libssh_SRCS
            ${libssh_SRCS}
            threads/noop.c
            threads/winlocks.c
        )
else()
    set(libssh_SRCS
        ${libssh_SRCS}
        threads/noop.c
    )
endif()

if (WITH_GCRYPT)
    set(libssh_SRCS
        ${libssh_SRCS}
        threads/libgcrypt.c
        libgcrypt.c
        gcrypt_missing.c
        pki_gcrypt.c
        ecdh_gcrypt.c
        getrandom_gcrypt.c
        md_gcrypt.c
        dh_key.c
        pki_ed25519.c
        external/ed25519.c
        external/fe25519.c
        external/ge25519.c
        external/sc25519.c
       )
    if (NOT HAVE_GCRYPT_CHACHA_POLY)
        set(libssh_SRCS
            ${libssh_SRCS}
            external/chacha.c
            external/poly1305.c
            chachapoly.c
        )
    endif (NOT HAVE_GCRYPT_CHACHA_POLY)
elseif (WITH_MBEDTLS)
    set(libssh_SRCS
        ${libssh_SRCS}
        threads/mbedtls.c
        libmbedcrypto.c
        mbedcrypto_missing.c
        pki_mbedcrypto.c
        ecdh_mbedcrypto.c
        getrandom_mbedcrypto.c
        md_mbedcrypto.c
        dh_key.c
        pki_ed25519.c
        external/ed25519.c
        external/fe25519.c
        external/ge25519.c
        external/sc25519.c
       )
    if (NOT (HAVE_MBEDTLS_CHACHA20_H AND HAVE_MBEDTLS_POLY1305_H))
        set(libssh_SRCS
            ${libssh_SRCS}
            external/chacha.c
            external/poly1305.c
            chachapoly.c
           )
    endif()

else (WITH_GCRYPT)
    set(libssh_SRCS
        ${libssh_SRCS}
        threads/libcrypto.c
        pki_crypto.c
        ecdh_crypto.c
        getrandom_crypto.c
        md_crypto.c
        libcrypto.c
        dh_crypto.c
       )
    if (NOT HAVE_OPENSSL_EVP_CHACHA20)
        set(libssh_SRCS
            ${libssh_SRCS}
            external/chacha.c
            external/poly1305.c
            chachapoly.c
        )
    endif (NOT HAVE_OPENSSL_EVP_CHACHA20)
endif (WITH_GCRYPT)

if (WITH_SFTP)
  set(libssh_SRCS
    ${libssh_SRCS}
    sftp.c
    sftp_common.c
    sftp_aio.c
  )

  if (WITH_SERVER)
    set(libssh_SRCS
      ${libssh_SRCS}
      sftpserver.c
    )
  endif (WITH_SERVER)
endif (WITH_SFTP)

if (WITH_SERVER)
  set(libssh_SRCS
    ${libssh_SRCS}
    server.c
    bind.c
    bind_config.c
  )
endif (WITH_SERVER)

if (WITH_GEX)
  set(libssh_SRCS
    ${libssh_SRCS}
    dh-gex.c
  )
endif (WITH_GEX)

if (WITH_GSSAPI AND GSSAPI_FOUND)
  set(libssh_SRCS
    ${libssh_SRCS}
    gssapi.c
  )
endif (WITH_GSSAPI AND GSSAPI_FOUND)

if (NOT WITH_NACL)
    if (NOT HAVE_LIBCRYPTO)
        set(libssh_SRCS
            ${libssh_SRCS}
            external/curve25519_ref.c
           )
    endif()
endif (NOT WITH_NACL)

# Set the path to the default map file
set(MAP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.map")

if (WITH_SYMBOL_VERSIONING AND HAVE_LD_VERSION_SCRIPT AND ABIMAP_FOUND)
    # Get the list of header files
    get_file_list(dev_header_list
                  DIRECTORIES "${LIBSSH_PUBLIC_INCLUDE_DIRS}/libssh"
                  FILES_PATTERNS "*.h")

    # Extract the symbols marked as "LIBSSH_API" from the header files
    extract_symbols("${PROJECT_NAME}_dev.symbols"
                    HEADERS_LIST dev_header_list
                    FILTER_PATTERN "LIBSSH_API")

    if (WITH_ABI_BREAK)
        set(ALLOW_ABI_BREAK "BREAK_ABI")
    endif()

    # Generate the symbol version map file
    generate_map_file("${PROJECT_NAME}_dev.map"
                      SYMBOLS "${PROJECT_NAME}_dev.symbols"
                      RELEASE_NAME_VERSION ${PROJECT_NAME}_AFTER_${LIBRARY_VERSION}
                      CURRENT_MAP ${MAP_PATH}
                      ${ALLOW_ABI_BREAK})

    set(libssh_SRCS
      ${libssh_SRCS}
      ${PROJECT_NAME}_dev.map
    )
endif (WITH_SYMBOL_VERSIONING AND HAVE_LD_VERSION_SCRIPT AND ABIMAP_FOUND)

# This gets built as a static library, if -DBUILD_SHARED_LIBS=OFF is passed to
# cmake.
add_library(ssh ${libssh_SRCS})
target_compile_options(ssh
                       PRIVATE
                           ${DEFAULT_C_COMPILE_FLAGS})
if (CYGWIN)
    target_compile_definitions(ssh PRIVATE _GNU_SOURCE)
endif ()
target_include_directories(ssh
                           PUBLIC
                               $<BUILD_INTERFACE:${libssh_SOURCE_DIR}/include>
                               $<BUILD_INTERFACE:${libssh_BINARY_DIR}/include>
                               $<INSTALL_INTERFACE:include>
                           PRIVATE ${LIBSSH_PRIVATE_INCLUDE_DIRS})

target_link_libraries(ssh
                      PRIVATE ${LIBSSH_LINK_LIBRARIES})

if (WIN32 AND NOT BUILD_SHARED_LIBS)
    target_compile_definitions(ssh PUBLIC "LIBSSH_STATIC")
endif ()

add_library(ssh::ssh ALIAS ssh)

if (WITH_SYMBOL_VERSIONING AND HAVE_LD_VERSION_SCRIPT)
    if (ABIMAP_FOUND)
        # Change path to devel map file
        set(MAP_PATH "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_dev.map")
    endif (ABIMAP_FOUND)

    target_link_libraries(ssh PRIVATE "-Wl,--version-script,\"${MAP_PATH}\"")
endif (WITH_SYMBOL_VERSIONING AND HAVE_LD_VERSION_SCRIPT)

set_target_properties(ssh
    PROPERTIES
      C_STANDARD
        99
      VERSION
        ${LIBRARY_VERSION}
      SOVERSION
        ${LIBRARY_SOVERSION}
      DEFINE_SYMBOL
        LIBSSH_EXPORTS
)

if (WITH_VISIBILITY_HIDDEN)
    set_target_properties(ssh PROPERTIES C_VISIBILITY_PRESET hidden)
endif (WITH_VISIBILITY_HIDDEN)

if (MINGW)
    target_link_libraries(ssh PRIVATE "-Wl,--enable-stdcall-fixup")
    target_compile_definitions(ssh PRIVATE "_POSIX_SOURCE")
endif ()
if (WITH_COVERAGE)
    include(CodeCoverage)
    append_coverage_compiler_flags_to_target(ssh)
endif (WITH_COVERAGE)


install(TARGETS ssh
        EXPORT libssh-config
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT libraries)

install(EXPORT libssh-config
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

if (BUILD_STATIC_LIB)
  add_library(ssh-static STATIC ${libssh_SRCS})
  target_compile_options(ssh-static
                         PRIVATE
                             ${DEFAULT_C_COMPILE_FLAGS})
  if (CYGWIN)
    target_compile_definitions(ssh-static PRIVATE _GNU_SOURCE)
  endif ()

  target_include_directories(ssh-static
                             PUBLIC
                                 $<BUILD_INTERFACE:${libssh_SOURCE_DIR}/include>
                                 $<BUILD_INTERFACE:${libssh_BINARY_DIR}/include>
                                 $<INSTALL_INTERFACE:include>
                             PRIVATE ${LIBSSH_PRIVATE_INCLUDE_DIRS})
  target_link_libraries(ssh-static
                        PUBLIC ${LIBSSH_LINK_LIBRARIES})
  add_library(ssh::static ALIAS ssh-static)

  if (MSVC)
    set(OUTPUT_SUFFIX static)
  else (MSVC)
    set(OUTPUT_SUFFIX )
  endif (MSVC)
  set_target_properties(
    ssh-static
      PROPERTIES
        VERSION
          ${LIBRARY_VERSION}
        SOVERSION
          ${LIBRARY_SOVERSION}
        OUTPUT_NAME
          ssh
        ARCHIVE_OUTPUT_DIRECTORY
          ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_SUFFIX}
  )

  if (WIN32)
    target_compile_definitions(ssh-static PUBLIC "LIBSSH_STATIC")
  endif (WIN32)
    if (WITH_COVERAGE)
        append_coverage_compiler_flags_to_target(ssh-static)
    endif (WITH_COVERAGE)
endif (BUILD_STATIC_LIB)

message(STATUS "Threads_FOUND=${Threads_FOUND}")
