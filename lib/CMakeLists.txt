# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if((CMAKE_COMPILER_IS_GNUCC AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 7) OR
        CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstringop-overflow=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wstringop-overflow=0")
elseif(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
endif()

set(LIBMICROHTTPD_SRC libmicrohttpd/src/microhttpd/base64.c
                      libmicrohttpd/src/microhttpd/basicauth.c
                      libmicrohttpd/src/microhttpd/connection.c
                      # libmicrohttpd/src/microhttpd/connection_https.c
                      libmicrohttpd/src/microhttpd/daemon.c
                      libmicrohttpd/src/microhttpd/digestauth.c
                      libmicrohttpd/src/microhttpd/internal.c
                      libmicrohttpd/src/microhttpd/md5.c
                      libmicrohttpd/src/microhttpd/memorypool.c
                      libmicrohttpd/src/microhttpd/mhd_compat.c
                      libmicrohttpd/src/microhttpd/mhd_itc.c
                      libmicrohttpd/src/microhttpd/mhd_mono_clock.c
                      libmicrohttpd/src/microhttpd/mhd_sockets.c
                      libmicrohttpd/src/microhttpd/mhd_str.c
                      libmicrohttpd/src/microhttpd/mhd_threads.c
                      libmicrohttpd/src/microhttpd/postprocessor.c
                      libmicrohttpd/src/microhttpd/reason_phrase.c
                      libmicrohttpd/src/microhttpd/response.c
                      libmicrohttpd/src/microhttpd/sysfdsetsize.c
                      libmicrohttpd/src/microhttpd/tsearch.c)
add_library(microhttpd STATIC ${LIBMICROHTTPD_SRC})
target_include_directories(microhttpd PRIVATE libmicrohttpd libmicrohttpd/src/include)
if(WIN32)
    target_link_libraries(microhttpd PUBLIC ws2_32)
endif()
if(NOT EMSCRIPTEN)
    target_link_libraries(microhttpd PRIVATE Threads::Threads)
endif()

set(MINIZ_SRC miniz/miniz.c)
add_library(miniz STATIC ${MINIZ_SRC})
target_compile_definitions(miniz PRIVATE -DMINIZ_NO_STDIO
                                         -DMINIZ_NO_TIME
                                         -DMINIZ_NO_ARCHIVE_APIS
                                         -DMINIZ_NO_ARCHIVE_WRITING_APIS
                                         -DMINIZ_NO_ZLIB_APIS
                                         -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES)
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    target_compile_options(miniz PRIVATE -fno-strict-aliasing)
endif()

set(IMGUI_SRC imgui/imgui.cpp
              imgui/imgui_draw.cpp
              imgui/imgui_demo.cpp)
add_library(imgui STATIC ${IMGUI_SRC})

set(LIBSODIUM_SRC libsodium/src/libsodium/crypto_generichash/crypto_generichash.c
                  libsodium/src/libsodium/crypto_generichash/blake2b/generichash_blake2.c
                  libsodium/src/libsodium/crypto_generichash/blake2b/ref/blake2b-compress-ref.c
                  libsodium/src/libsodium/crypto_generichash/blake2b/ref/blake2b-compress-ssse3.c
                  libsodium/src/libsodium/crypto_generichash/blake2b/ref/blake2b-compress-avx2.c
                  libsodium/src/libsodium/crypto_generichash/blake2b/ref/blake2b-compress-sse41.c
                  libsodium/src/libsodium/crypto_generichash/blake2b/ref/generichash_blake2b.c
                  libsodium/src/libsodium/crypto_generichash/blake2b/ref/blake2b-ref.c
                  libsodium/src/libsodium/crypto_kx/crypto_kx.c
                  libsodium/src/libsodium/crypto_sign/crypto_sign.c
                  libsodium/src/libsodium/crypto_sign/ed25519/sign_ed25519.c
                  libsodium/src/libsodium/crypto_sign/ed25519/ref10/obsolete.c
                  libsodium/src/libsodium/crypto_sign/ed25519/ref10/sign.c
                  libsodium/src/libsodium/crypto_sign/ed25519/ref10/keypair.c
                  libsodium/src/libsodium/crypto_sign/ed25519/ref10/open.c
                  libsodium/src/libsodium/crypto_secretbox/crypto_secretbox.c
                  libsodium/src/libsodium/crypto_secretbox/crypto_secretbox_easy.c
                  libsodium/src/libsodium/crypto_secretbox/xsalsa20poly1305/secretbox_xsalsa20poly1305.c
                  libsodium/src/libsodium/crypto_secretbox/xchacha20poly1305/secretbox_xchacha20poly1305.c
                  libsodium/src/libsodium/crypto_pwhash/crypto_pwhash.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/blake2b-long.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/argon2-core.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/argon2-fill-block-avx512f.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/argon2-fill-block-ref.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/argon2-fill-block-ssse3.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/pwhash_argon2i.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/pwhash_argon2id.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/argon2.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/argon2-fill-block-avx2.c
                  libsodium/src/libsodium/crypto_pwhash/argon2/argon2-encoding.c
                  libsodium/src/libsodium/crypto_pwhash/scryptsalsa208sha256/scrypt_platform.c
                  libsodium/src/libsodium/crypto_pwhash/scryptsalsa208sha256/crypto_scrypt-common.c
                  libsodium/src/libsodium/crypto_pwhash/scryptsalsa208sha256/pwhash_scryptsalsa208sha256.c
                  libsodium/src/libsodium/crypto_pwhash/scryptsalsa208sha256/pbkdf2-sha256.c
                  libsodium/src/libsodium/crypto_pwhash/scryptsalsa208sha256/nosse/pwhash_scryptsalsa208sha256_nosse.c
                  libsodium/src/libsodium/crypto_pwhash/scryptsalsa208sha256/sse/pwhash_scryptsalsa208sha256_sse.c
                  libsodium/src/libsodium/crypto_verify/sodium/verify.c
                  libsodium/src/libsodium/crypto_auth/crypto_auth.c
                  libsodium/src/libsodium/crypto_auth/hmacsha512/auth_hmacsha512.c
                  libsodium/src/libsodium/crypto_auth/hmacsha512256/auth_hmacsha512256.c
                  libsodium/src/libsodium/crypto_auth/hmacsha256/auth_hmacsha256.c
                  libsodium/src/libsodium/crypto_kdf/crypto_kdf.c
                  libsodium/src/libsodium/crypto_kdf/blake2b/kdf_blake2b.c
                  libsodium/src/libsodium/crypto_shorthash/crypto_shorthash.c
                  libsodium/src/libsodium/crypto_shorthash/siphash24/shorthash_siphash24.c
                  libsodium/src/libsodium/crypto_shorthash/siphash24/shorthash_siphashx24.c
                  libsodium/src/libsodium/crypto_shorthash/siphash24/ref/shorthash_siphashx24_ref.c
                  libsodium/src/libsodium/crypto_shorthash/siphash24/ref/shorthash_siphash24_ref.c
                  libsodium/src/libsodium/crypto_scalarmult/crypto_scalarmult.c
                  libsodium/src/libsodium/crypto_scalarmult/ed25519/ref10/scalarmult_ed25519_ref10.c
                  libsodium/src/libsodium/crypto_scalarmult/curve25519/scalarmult_curve25519.c
                  libsodium/src/libsodium/crypto_scalarmult/curve25519/sandy2x/curve25519_sandy2x.c
                  libsodium/src/libsodium/crypto_scalarmult/curve25519/sandy2x/fe_frombytes_sandy2x.c
                  libsodium/src/libsodium/crypto_scalarmult/curve25519/sandy2x/fe51_invert.c
                  libsodium/src/libsodium/crypto_scalarmult/curve25519/ref10/x25519_ref10.c
                  libsodium/src/libsodium/crypto_onetimeauth/crypto_onetimeauth.c
                  libsodium/src/libsodium/crypto_onetimeauth/poly1305/onetimeauth_poly1305.c
                  libsodium/src/libsodium/crypto_onetimeauth/poly1305/donna/poly1305_donna.c
                  libsodium/src/libsodium/crypto_onetimeauth/poly1305/sse2/poly1305_sse2.c
                  libsodium/src/libsodium/randombytes/randombytes.c
                  libsodium/src/libsodium/randombytes/sysrandom/randombytes_sysrandom.c
                  libsodium/src/libsodium/randombytes/salsa20/randombytes_salsa20_random.c
                  libsodium/src/libsodium/randombytes/nativeclient/randombytes_nativeclient.c
                  libsodium/src/libsodium/crypto_box/crypto_box_easy.c
                  libsodium/src/libsodium/crypto_box/crypto_box_seal.c
                  libsodium/src/libsodium/crypto_box/crypto_box.c
                  libsodium/src/libsodium/crypto_box/curve25519xsalsa20poly1305/box_curve25519xsalsa20poly1305.c
                  libsodium/src/libsodium/crypto_box/curve25519xchacha20poly1305/box_curve25519xchacha20poly1305.c
                  libsodium/src/libsodium/crypto_box/curve25519xchacha20poly1305/box_seal_curve25519xchacha20poly1305.c
                  libsodium/src/libsodium/sodium/codecs.c
                  libsodium/src/libsodium/sodium/runtime.c
                  libsodium/src/libsodium/sodium/core.c
                  libsodium/src/libsodium/sodium/utils.c
                  libsodium/src/libsodium/sodium/version.c
                  libsodium/src/libsodium/crypto_stream/crypto_stream.c
                  libsodium/src/libsodium/crypto_stream/xchacha20/stream_xchacha20.c
                  libsodium/src/libsodium/crypto_stream/chacha20/stream_chacha20.c
                  libsodium/src/libsodium/crypto_stream/chacha20/ref/chacha20_ref.c
                  libsodium/src/libsodium/crypto_stream/chacha20/dolbeau/chacha20_dolbeau-avx2.c
                  libsodium/src/libsodium/crypto_stream/chacha20/dolbeau/chacha20_dolbeau-ssse3.c
                  libsodium/src/libsodium/crypto_stream/salsa20/stream_salsa20.c
                  libsodium/src/libsodium/crypto_stream/salsa20/ref/salsa20_ref.c
                  libsodium/src/libsodium/crypto_stream/salsa20/xmm6int/salsa20_xmm6int-avx2.c
                  libsodium/src/libsodium/crypto_stream/salsa20/xmm6int/salsa20_xmm6int-sse2.c
                  libsodium/src/libsodium/crypto_stream/salsa20/xmm6/salsa20_xmm6.c
                  libsodium/src/libsodium/crypto_stream/salsa2012/stream_salsa2012.c
                  libsodium/src/libsodium/crypto_stream/salsa2012/ref/stream_salsa2012_ref.c
                  libsodium/src/libsodium/crypto_stream/salsa208/stream_salsa208.c
                  libsodium/src/libsodium/crypto_stream/salsa208/ref/stream_salsa208_ref.c
                  libsodium/src/libsodium/crypto_stream/xsalsa20/stream_xsalsa20.c
                  libsodium/src/libsodium/crypto_hash/crypto_hash.c
                  libsodium/src/libsodium/crypto_hash/sha512/hash_sha512.c
                  libsodium/src/libsodium/crypto_hash/sha512/cp/hash_sha512_cp.c
                  libsodium/src/libsodium/crypto_hash/sha256/hash_sha256.c
                  libsodium/src/libsodium/crypto_hash/sha256/cp/hash_sha256_cp.c
                  libsodium/src/libsodium/crypto_aead/xchacha20poly1305/sodium/aead_xchacha20poly1305.c
                  libsodium/src/libsodium/crypto_aead/aes256gcm/aesni/aead_aes256gcm_aesni.c
                  libsodium/src/libsodium/crypto_aead/chacha20poly1305/sodium/aead_chacha20poly1305.c
                  libsodium/src/libsodium/crypto_secretstream/xchacha20poly1305/secretstream_xchacha20poly1305.c
                  libsodium/src/libsodium/crypto_core/salsa/ref/core_salsa_ref.c
                  libsodium/src/libsodium/crypto_core/hchacha20/core_hchacha20.c
                  libsodium/src/libsodium/crypto_core/hsalsa20/core_hsalsa20.c
                  libsodium/src/libsodium/crypto_core/hsalsa20/ref2/core_hsalsa20_ref2.c
                  libsodium/src/libsodium/crypto_core/ed25519/core_ed25519.c
                  libsodium/src/libsodium/crypto_core/ed25519/ref10/ed25519_ref10.c)
add_library(sodium STATIC ${LIBSODIUM_SRC})
target_include_directories(sodium PRIVATE libsodium/src/libsodium/include
                                          libsodium/src/libsodium/include/sodium)
target_compile_definitions(sodium PUBLIC -DSODIUM_STATIC -DCONFIGURED=1)
if(NOT EMSCRIPTEN)
    if(WIN32)
        target_compile_definitions(sodium PUBLIC -DHAVE_PTHREAD)
    endif()
    target_link_libraries(sodium PRIVATE Threads::Threads)
endif()
