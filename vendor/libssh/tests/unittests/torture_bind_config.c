/*
 * torture_bind_config.c - Tests for server side configuration
 *
 * This file is part of the SSH Library
 *
 * Copyright (c) 2019 by Red Hat, Inc.
 *
 * Author: Anderson Toshiyuki Sasaki <ansasaki@redhat.com>
 *
 * The SSH Library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * The SSH Library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the SSH Library; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include "config.h"

#define LIBSSH_STATIC

#include "torture.h"
#include "torture_key.h"

#include <libssh/bind_config.h>
#include <libssh/bind.h>

extern LIBSSH_THREAD int ssh_log_level;

#define LOGLEVEL "verbose"
#define LOGLEVEL2 "fatal"
#define LOGLEVEL3 "DEBUG1"
#define LOGLEVEL4 "DEBUG2"
#define LISTEN_ADDRESS "::1"
#define LISTEN_ADDRESS2 "::2"
#define KEXALGORITHMS "ecdh-sha2-nistp521,diffie-hellman-group16-sha512,diffie-hellman-group18-sha512,diffie-hellman-group14-sha1"
#define KEXALGORITHMS2 "ecdh-sha2-nistp521"
#define CIPHERS "aes128-ctr,aes192-ctr,aes256-ctr"
#define CIPHERS2 "aes256-ctr"
#define HOSTKEYALGORITHMS "ssh-ed25519,ecdsa-sha2-nistp521,ssh-rsa"
#define HOSTKEYALGORITHMS_UNKNOWN "ssh-ed25519,ecdsa-sha2-nistp521,unknown,ssh-rsa"
#define HOSTKEYALGORITHMS2 "rsa-sha2-256"
#define PUBKEYACCEPTEDTYPES "rsa-sha2-512,ssh-rsa,ecdsa-sha2-nistp521"
#define PUBKEYACCEPTEDTYPES_UNKNOWN "rsa-sha2-512,ssh-rsa,unknown,ecdsa-sha2-nistp521"
#define PUBKEYACCEPTEDTYPES2 "rsa-sha2-256,ssh-rsa"
#define MACS "hmac-sha1,hmac-sha2-256,hmac-sha2-512,hmac-sha1-etm@openssh.com,hmac-sha2-256-etm@openssh.com,hmac-sha2-512-etm@openssh.com"
#define MACS2 "hmac-sha1"

#ifdef HAVE_DSA
#define LIBSSH_DSA_TESTKEY        "libssh_testkey.id_dsa"
#endif
#define LIBSSH_RSA_TESTKEY        "libssh_testkey.id_rsa"
#define LIBSSH_ED25519_TESTKEY    "libssh_testkey.id_ed25519"
#ifdef HAVE_ECC
#define LIBSSH_ECDSA_521_TESTKEY  "libssh_testkey.id_ecdsa521"
#endif

#define LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS "libssh_test_bind_config_listenaddress"
#define LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_STRING "ListenAddress "LISTEN_ADDRESS"\n"
#define LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS2 "libssh_test_bind_config_listenaddress2"
#define LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS2_STRING "ListenAddress "LISTEN_ADDRESS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE "libssh_test_bind_config_listenaddress_twice"
#define LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_STRING \
    "ListenAddress "LISTEN_ADDRESS"\n" \
    "ListenAddress "LISTEN_ADDRESS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_REC "libssh_test_bind_config_listenaddress_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_REC_STRING \
    "ListenAddress "LISTEN_ADDRESS"\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_PORT "libssh_test_bind_config_port"
#define LIBSSH_TEST_BIND_CONFIG_PORT_STRING "Port 123\n"
#define LIBSSH_TEST_BIND_CONFIG_PORT2 "libssh_test_bind_config_port2"
#define LIBSSH_TEST_BIND_CONFIG_PORT2_STRING "Port 456\n"
#define LIBSSH_TEST_BIND_CONFIG_PORT_TWICE "libssh_test_bind_config_port_twice"
#define LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_STRING \
    "Port 123\n" \
    "Port 456\n"
#define LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_REC "libssh_test_bind_config_port_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_REC_STRING \
    "Port 123\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_PORT2"\n"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY "libssh_test_bind_config_hostkey"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_STRING "HostKey "LIBSSH_ECDSA_521_TESTKEY"\n"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY2 "libssh_test_bind_config_hostkey2"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY2_STRING "HostKey "LIBSSH_RSA_TESTKEY"\n"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE "libssh_test_bind_config_hostkey_twice"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_STRING \
    "HostKey "LIBSSH_ECDSA_521_TESTKEY"\n" \
    "HostKey "LIBSSH_RSA_TESTKEY"\n"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_REC "libssh_test_bind_config_hostkey_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_REC_STRING \
    "HostKey "LIBSSH_ECDSA_521_TESTKEY"\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_HOSTKEY2"\n"
#define LIBSSH_TEST_BIND_CONFIG_LOGLEVEL "libssh_test_bind_config_loglevel"
#define LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_STRING "LogLevel "LOGLEVEL"\n"
#define LIBSSH_TEST_BIND_CONFIG_LOGLEVEL1 "libssh_test_bind_config_loglevel2"
#define LIBSSH_TEST_BIND_CONFIG_LOGLEVEL1_STRING "LogLevel "LOGLEVEL2"\n"
#define LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE "libssh_test_bind_config_loglevel_twice"
#define LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_STRING \
    "LogLevel "LOGLEVEL"\n" \
    "LogLevel "LOGLEVEL2"\n"
#define LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_REC "libssh_test_bind_config_loglevel_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_REC_STRING \
    "LogLevel "LOGLEVEL"\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_LOGLEVEL1"\n"
#define LIBSSH_TEST_BIND_CONFIG_CIPHERS "libssh_test_bind_config_ciphers"
#define LIBSSH_TEST_BIND_CONFIG_CIPHERS_STRING "Ciphers "CIPHERS"\n"
#define LIBSSH_TEST_BIND_CONFIG_CIPHERS2 "libssh_test_bind_config_ciphers2"
#define LIBSSH_TEST_BIND_CONFIG_CIPHERS2_STRING "Ciphers "CIPHERS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE "libssh_test_bind_config_ciphers_twice"
#define LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_STRING \
    "Ciphers "CIPHERS"\n" \
    "Ciphers "CIPHERS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_REC "libssh_test_bind_config_ciphers_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_REC_STRING \
    "Ciphers "CIPHERS"\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_CIPHERS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_MACS "libssh_test_bind_config_macs"
#define LIBSSH_TEST_BIND_CONFIG_MACS_STRING "MACs "MACS"\n"
#define LIBSSH_TEST_BIND_CONFIG_MACS2 "libssh_test_bind_config_macs2"
#define LIBSSH_TEST_BIND_CONFIG_MACS2_STRING "MACs "MACS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_MACS_TWICE "libssh_test_bind_config_macs_twice"
#define LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_STRING \
    "MACs "MACS"\n" \
    "MACs "MACS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_REC "libssh_test_bind_config_macs_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_REC_STRING \
    "MACs "MACS"\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_MACS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS "libssh_test_bind_config_kexalgorithms"
#define LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_STRING "KexAlgorithms "KEXALGORITHMS"\n"
#define LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS2 "libssh_test_bind_config_kexalgorithms2"
#define LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS2_STRING "KexAlgorithms "KEXALGORITHMS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE "libssh_test_bind_config_kexalgorithms_twice"
#define LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_STRING \
    "KexAlgorithms "KEXALGORITHMS"\n" \
    "KexAlgorithms "KEXALGORITHMS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_REC "libssh_test_bind_config_kexalgorithms_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_REC_STRING \
    "KexAlgorithms "KEXALGORITHMS"\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS2"\n"

#define LIBSSH_TEST_BIND_CONFIG_FULL "libssh_test_bind_config_full"
#define LIBSSH_TEST_BIND_CONFIG_INCLUDE "libssh_test_bind_config_include"
#define LIBSSH_TEST_BIND_CONFIG_INCLUDE_RECURSIVE "libssh_test_bind_config_include_recursive"
#define LIBSSH_TEST_BIND_CONFIG_INCLUDE_RECURSIVE_LOOP "libssh_test_bind_config_include_recursive_loop"
#define LIBSSH_TEST_BIND_CONFIG_CORNER_CASES "libssh_test_bind_config_corner_cases"

#define LIBSSH_TEST_BIND_CONFIG_MATCH_ALL "libssh_test_bind_config_match_all"
#define LIBSSH_TEST_BIND_CONFIG_MATCH_TWICE "libssh_test_bind_config_match_twice"
#define LIBSSH_TEST_BIND_CONFIG_MATCH_UNSUPPORTED "libssh_test_bind_config_match_unsupported"
#define LIBSSH_TEST_BIND_CONFIG_MATCH_NOT_ALLOWED "libssh_test_bind_config_match_not_allowed"
#define LIBSSH_TEST_BIND_CONFIG_MATCH_CORNER_CASES "libssh_test_bind_config_match_corner_cases"
#define LIBSSH_TEST_BIND_CONFIG_MATCH_INVALID "libssh_test_bind_config_match_invalid"
#define LIBSSH_TEST_BIND_CONFIG_MATCH_INVALID2 "libssh_test_bind_config_match_invalid2"

#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED "libssh_test_bind_config_pubkey"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_STRING "PubkeyAcceptedKeyTypes "PUBKEYACCEPTEDTYPES"\n"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED2 "libssh_test_bind_config_pubkey2"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED2_STRING "PubkeyAcceptedKeyTypes "PUBKEYACCEPTEDTYPES2"\n"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE "libssh_test_bind_config_pubkey_twice"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_STRING \
    "PubkeyAcceptedKeyTypes "PUBKEYACCEPTEDTYPES"\n" \
    "PubkeyAcceptedKeyTypes "PUBKEYACCEPTEDTYPES2"\n"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_REC "libssh_test_bind_config_pubkey_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_REC_STRING \
    "PubkeyAcceptedKeyTypes "PUBKEYACCEPTEDTYPES2"\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS"\n"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_UNKNOWN "libssh_test_bind_config_pubkey_unknown"
#define LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_UNKNOWN_STRING "PubkeyAcceptedKeyTypes "PUBKEYACCEPTEDTYPES_UNKNOWN"\n"

#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS "libssh_test_bind_config_hostkey_alg"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_STRING "HostKeyAlgorithms "HOSTKEYALGORITHMS"\n"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS2 "libssh_test_bind_config_hostkey_alg2"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS2_STRING "HostKeyAlgorithms "HOSTKEYALGORITHMS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE "libssh_test_bind_config_hostkey_alg_twice"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_STRING \
    "HostKeyAlgorithms "HOSTKEYALGORITHMS"\n" \
    "HostKeyAlgorithms "HOSTKEYALGORITHMS2"\n"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_REC "libssh_test_bind_config_hostkey_alg_twice_rec"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_REC_STRING \
    "HostKeyAlgorithms "HOSTKEYALGORITHMS2"\n" \
    "Include "LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS"\n"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_UNKNOWN "libssh_test_bind_config_hostkey_alg_unknown"
#define LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_UNKNOWN_STRING "HostKeyAlgorithms "HOSTKEYALGORITHMS_UNKNOWN"\n"

const char template[] = "temp_dir_XXXXXX";

struct bind_st {
    char *cwd;
    char *temp_dir;
    ssh_bind bind;
};

static int setup_config_files(void **state)
{
    struct bind_st *test_state = NULL;
    char *cwd = NULL;
    char *tmp_dir = NULL;
    int rc = 0;

    test_state = (struct bind_st *)malloc(sizeof(struct bind_st));
    assert_non_null(test_state);

    cwd = torture_get_current_working_dir();
    assert_non_null(cwd);

    tmp_dir = torture_make_temp_dir(template);
    assert_non_null(tmp_dir);

    test_state->cwd = cwd;
    test_state->temp_dir = tmp_dir;

    *state = test_state;

    rc = torture_change_dir(tmp_dir);
    assert_int_equal(rc, 0);

    printf("Changed directory to: %s\n", tmp_dir);

    /* For ed25519 the test keys are not available in legacy PEM format. Using
     * the new OpenSSH format for all algorithms */
    torture_write_file(LIBSSH_RSA_TESTKEY,
                       torture_get_openssh_testkey(SSH_KEYTYPE_RSA, 0));

    torture_write_file(LIBSSH_ED25519_TESTKEY,
                       torture_get_openssh_testkey(SSH_KEYTYPE_ED25519, 0));
#ifdef HAVE_ECC
    torture_write_file(LIBSSH_ECDSA_521_TESTKEY,
                       torture_get_openssh_testkey(SSH_KEYTYPE_ECDSA_P521, 0));
#endif
#ifdef HAVE_DSA
    torture_write_file(LIBSSH_DSA_TESTKEY,
                       torture_get_openssh_testkey(SSH_KEYTYPE_DSS, 0));
#endif

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS,
                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS2,
                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS2_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_REC_STRING);

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PORT,
                       LIBSSH_TEST_BIND_CONFIG_PORT_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PORT2,
                       LIBSSH_TEST_BIND_CONFIG_PORT2_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PORT_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_REC_STRING);

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY2,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY2_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_REC_STRING);

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_LOGLEVEL,
                       LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_LOGLEVEL1,
                       LIBSSH_TEST_BIND_CONFIG_LOGLEVEL1_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_REC_STRING);

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_CIPHERS,
                       LIBSSH_TEST_BIND_CONFIG_CIPHERS_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_CIPHERS2,
                       LIBSSH_TEST_BIND_CONFIG_CIPHERS2_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_REC_STRING);

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MACS,
                       LIBSSH_TEST_BIND_CONFIG_MACS_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MACS2,
                       LIBSSH_TEST_BIND_CONFIG_MACS2_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MACS_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_REC_STRING);

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS,
                       LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS2,
                       LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS2_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_REC_STRING);

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_FULL,
                       "ListenAddress "LISTEN_ADDRESS"\n"
                       "Port 123\n"
                       "HostKey "LIBSSH_ECDSA_521_TESTKEY"\n"
                       "LogLevel "LOGLEVEL"\n"
                       "Ciphers "CIPHERS"\n"
                       "MACs "MACS"\n"
                       "KexAlgorithms "KEXALGORITHMS"\n");

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_INCLUDE,
                       "Include "LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS"\n"
                       "Include "LIBSSH_TEST_BIND_CONFIG_PORT"\n"
                       "Include "LIBSSH_TEST_BIND_CONFIG_HOSTKEY"\n"
                       "Include "LIBSSH_TEST_BIND_CONFIG_LOGLEVEL"\n"
                       "Include "LIBSSH_TEST_BIND_CONFIG_CIPHERS"\n"
                       "Include "LIBSSH_TEST_BIND_CONFIG_MACS"\n"
                       "Include "LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS"\n");

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_INCLUDE_RECURSIVE,
                       "Include "LIBSSH_TEST_BIND_CONFIG_INCLUDE"\n");

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_INCLUDE_RECURSIVE_LOOP,
                       "Include "LIBSSH_TEST_BIND_CONFIG_INCLUDE_RECURSIVE_LOOP"\n");

    /* Unsupported options and corner cases */
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_CORNER_CASES,
                       "\n" /* empty line */
                       "# comment line\n"
                       "  # comment line not starting with hash\n"
                       "UnknownConfigurationOption yes\n"
                       "Ciphers "CIPHERS2"\n");

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MATCH_ALL,
                       "Include "LIBSSH_TEST_BIND_CONFIG_FULL"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL2"\n");
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MATCH_TWICE,
                       "Include "LIBSSH_TEST_BIND_CONFIG_FULL"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL3"\n");
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MATCH_UNSUPPORTED,
                       "Include "LIBSSH_TEST_BIND_CONFIG_FULL"\n"
                       "Match User alice\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match Group sftp_users\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match Host 192.168.0.*\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match LocalAddress 172.30.1.5\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match LocalPort 42\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match Rdomain 4\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match Address 10.0.0.10\n"
                       "\tLogLevel "LOGLEVEL2"\n");
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MATCH_NOT_ALLOWED,
                       "Include "LIBSSH_TEST_BIND_CONFIG_FULL"\n"
                       "Match All\n"
                       "\tListenAddress "LISTEN_ADDRESS2"\n"
                       "\tPort 456\n"
                       "\tHostKey "LIBSSH_RSA_TESTKEY"\n"
                       "\tCiphers "CIPHERS2"\n"
                       "\tMACs "MACS2"\n"
                       "\tKexAlgorithms "KEXALGORITHMS2"\n");
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MATCH_CORNER_CASES,
                       "Include "LIBSSH_TEST_BIND_CONFIG_FULL"\n"
                       "Match User alice\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL3"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL"\n");
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MATCH_INVALID,
                       "Include "LIBSSH_TEST_BIND_CONFIG_FULL"\n"
                       "Match User alice All\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL3"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL4"\n");
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_MATCH_INVALID2,
                       "Include "LIBSSH_TEST_BIND_CONFIG_FULL"\n"
                       "Match All User alice\n"
                       "\tLogLevel "LOGLEVEL2"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL3"\n"
                       "Match All\n"
                       "\tLogLevel "LOGLEVEL4"\n");

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED,
                       LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED2,
                       LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED2_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_REC_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_UNKNOWN,
                       LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_UNKNOWN_STRING);

    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS2,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS2_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_REC,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_REC_STRING);
    torture_write_file(LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_UNKNOWN,
                       LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_UNKNOWN_STRING);
    return 0;
}

static int sshbind_setup(void **state)
{
    int rc;
    struct bind_st *test_state = NULL;

    rc = setup_config_files((void **)&test_state);
    assert_int_equal(rc, 0);
    assert_non_null(test_state);

    test_state->bind = ssh_bind_new();
    assert_non_null(test_state->bind);

    *state = test_state;

    return 0;
}

static int sshbind_teardown(void **state)
{
    struct bind_st *test_state = NULL;
    int rc;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);

    assert_non_null(test_state);
    assert_non_null(test_state->cwd);
    assert_non_null(test_state->temp_dir);
    assert_non_null(test_state->bind);

    rc = torture_change_dir(test_state->cwd);
    assert_int_equal(rc, 0);

    rc = torture_rmdirs(test_state->temp_dir);
    assert_int_equal(rc, 0);

    SAFE_FREE(test_state->temp_dir);
    SAFE_FREE(test_state->cwd);
    ssh_bind_free(test_state->bind);
    SAFE_FREE(test_state);

    return 0;
}

/**
 * @brief helper function loading configuration from either file or string
 */
static void
_parse_config(ssh_bind bind,
              const char *file,
              const char *string,
              int expected)
{
    int ret = -1;

    /* make sure either config file or config string is given,
     * not both */
    assert_int_not_equal(file == NULL, string == NULL);

    if (file != NULL) {
        ret = ssh_bind_config_parse_file(bind, file);
    } else if (string != NULL) {
        ret = ssh_bind_config_parse_string(bind, string);
    } else {
        /* should not happen */
        fail();
    }

    /* make sure parsing went as expected */
    assert_return_code(ret, expected);
}


static void
torture_bind_config_listen_address(void **state,
                                   const char *file,
                                   const char *string,
                                   const char *expect)
{
    struct bind_st *test_state;
    ssh_bind bind;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    _parse_config(bind, file, string, SSH_OK);

    assert_non_null(bind->bindaddr);
    assert_string_equal(bind->bindaddr, expect);
}

static void torture_bind_config_listen_address_file(void **state)
{
    torture_bind_config_listen_address(state,
                                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS,
                                       NULL,
                                       LISTEN_ADDRESS);
}

static void torture_bind_config_listen_address_string(void **state)
{
    torture_bind_config_listen_address(state,
                                       NULL,
                                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_STRING,
                                       LISTEN_ADDRESS);
}

static void torture_bind_config_listen_address2_file(void **state)
{
    torture_bind_config_listen_address(state,
                                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS2,
                                       NULL,
                                       LISTEN_ADDRESS2);
}

static void torture_bind_config_listen_address2_string(void **state)
{
    torture_bind_config_listen_address(state,
                                       NULL,
                                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS2_STRING,
                                       LISTEN_ADDRESS2);
}

static void torture_bind_config_listen_address_twice_file(void **state)
{
    torture_bind_config_listen_address(state,
                                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE,
                                       NULL,
                                       LISTEN_ADDRESS);
}

static void torture_bind_config_listen_address_twice_string(void **state)
{
    torture_bind_config_listen_address(state,
                                       NULL,
                                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_STRING,
                                       LISTEN_ADDRESS);
}

static void torture_bind_config_listen_address_twice_rec_file(void **state)
{
    torture_bind_config_listen_address(state,
                                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_REC,
                                       NULL,
                                       LISTEN_ADDRESS);
}

static void torture_bind_config_listen_address_twice_rec_string(void **state)
{
    torture_bind_config_listen_address(state,
                                       NULL,
                                       LIBSSH_TEST_BIND_CONFIG_LISTENADDRESS_TWICE_REC_STRING,
                                       LISTEN_ADDRESS);
}

static void
torture_bind_config_port(void **state,
                         const char *file,
                         const char *string,
                         int expect)
{
    struct bind_st *test_state;
    ssh_bind bind;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    _parse_config(bind, file, string, SSH_OK);

    assert_int_equal(bind->bindport, expect);
}

static void torture_bind_config_port_file(void **state)
{
    torture_bind_config_port(state,
                             LIBSSH_TEST_BIND_CONFIG_PORT,
                             NULL,
                             123);
}

static void torture_bind_config_port_string(void **state)
{
    torture_bind_config_port(state,
                             NULL,
                             LIBSSH_TEST_BIND_CONFIG_PORT_STRING,
                             123);
}

static void torture_bind_config_port2_file(void **state)
{
    torture_bind_config_port(state,
                             LIBSSH_TEST_BIND_CONFIG_PORT2,
                             NULL,
                             456);
}

static void torture_bind_config_port2_string(void **state)
{
    torture_bind_config_port(state,
                             NULL,
                             LIBSSH_TEST_BIND_CONFIG_PORT2_STRING,
                             456);
}

static void torture_bind_config_port_twice_file(void **state)
{
    torture_bind_config_port(state,
                             LIBSSH_TEST_BIND_CONFIG_PORT_TWICE,
                             NULL,
                             123);
}

static void torture_bind_config_port_twice_string(void **state)
{
    torture_bind_config_port(state,
                             NULL,
                             LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_STRING,
                             123);
}

static void torture_bind_config_port_twice_rec_file(void **state)
{
    torture_bind_config_port(state,
                             LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_REC,
                             NULL,
                             123);
}

static void torture_bind_config_port_twice_rec_string(void **state)
{
    torture_bind_config_port(state,
                             NULL,
                             LIBSSH_TEST_BIND_CONFIG_PORT_TWICE_REC_STRING,
                             123);
}

static void
torture_bind_config_hostkey(void **state,
                            const char *file,
                            const char *string)
{
    struct bind_st *test_state;
    ssh_bind bind;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    _parse_config(bind, file, string, SSH_OK);

    assert_non_null(bind->ecdsakey);
    assert_string_equal(bind->ecdsakey, LIBSSH_ECDSA_521_TESTKEY);
}

static void
torture_bind_config_hostkey2(void **state,
                             const char *file,
                             const char *string)
{
    struct bind_st *test_state;
    ssh_bind bind;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    _parse_config(bind, file, string, SSH_OK);

    assert_non_null(bind->ecdsakey);
    assert_string_equal(bind->ecdsakey, LIBSSH_ECDSA_521_TESTKEY);
    assert_non_null(bind->rsakey);
    assert_string_equal(bind->rsakey, LIBSSH_RSA_TESTKEY);
}

static void torture_bind_config_hostkey_file(void **state)
{
    torture_bind_config_hostkey(state, LIBSSH_TEST_BIND_CONFIG_HOSTKEY, NULL);
}

static void torture_bind_config_hostkey_string(void **state)
{
    torture_bind_config_hostkey(state, NULL, LIBSSH_TEST_BIND_CONFIG_HOSTKEY_STRING);
}

static void torture_bind_config_hostkey_twice_file(void **state)
{
    torture_bind_config_hostkey2(state, LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE, NULL);
}

static void torture_bind_config_hostkey_twice_string(void **state)
{
    torture_bind_config_hostkey2(state, NULL, LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_STRING);
}

static void torture_bind_config_hostkey_twice_rec_file(void **state)
{
    torture_bind_config_hostkey2(state, LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_REC, NULL);
}

static void torture_bind_config_hostkey_twice_rec_string(void **state)
{
    torture_bind_config_hostkey2(state, NULL, LIBSSH_TEST_BIND_CONFIG_HOSTKEY_TWICE_REC_STRING);
}

static void torture_bind_config_hostkey_separately(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    rc = ssh_bind_config_parse_file(bind, LIBSSH_TEST_BIND_CONFIG_HOSTKEY);
    assert_int_equal(rc, 0);
    assert_non_null(bind->ecdsakey);
    assert_string_equal(bind->ecdsakey, LIBSSH_ECDSA_521_TESTKEY);

    rc = ssh_bind_config_parse_file(bind, LIBSSH_TEST_BIND_CONFIG_HOSTKEY2);
    assert_int_equal(rc, 0);
    assert_non_null(bind->rsakey);
    assert_string_equal(bind->rsakey, LIBSSH_RSA_TESTKEY);
    assert_non_null(bind->ecdsakey);
    assert_string_equal(bind->ecdsakey, LIBSSH_ECDSA_521_TESTKEY);
}

static void
torture_bind_config_loglevel(void **state,
                             const char *file,
                             const char *string,
                             int expect)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int previous_level, new_level, rc;

    previous_level = ssh_get_log_level();

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    _parse_config(bind, file, string, SSH_OK);

    new_level = ssh_get_log_level();
    assert_int_equal(new_level, expect);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_loglevel_file(void **state)
{
    torture_bind_config_loglevel(state,
                                 LIBSSH_TEST_BIND_CONFIG_LOGLEVEL,
                                 NULL,
                                 2);
}

static void torture_bind_config_loglevel_string(void **state)
{
    torture_bind_config_loglevel(state,
                                 NULL,
                                 LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_STRING,
                                 2);
}

static void torture_bind_config_loglevel1_file(void **state)
{
    torture_bind_config_loglevel(state,
                                 LIBSSH_TEST_BIND_CONFIG_LOGLEVEL1,
                                 NULL,
                                 1);
}

static void torture_bind_config_loglevel1_string(void **state)
{
    torture_bind_config_loglevel(state,
                                 NULL,
                                 LIBSSH_TEST_BIND_CONFIG_LOGLEVEL1_STRING,
                                 1);
}

static void torture_bind_config_loglevel_twice_file(void **state)
{
    torture_bind_config_loglevel(state,
                                 LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE,
                                 NULL,
                                 2);
}

static void torture_bind_config_loglevel_twice_string(void **state)
{
    torture_bind_config_loglevel(state,
                                 NULL,
                                 LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_STRING,
                                 2);
}

static void torture_bind_config_loglevel_twice_rec_file(void **state)
{
    torture_bind_config_loglevel(state,
                                 LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_REC,
                                 NULL,
                                 2);
}

static void torture_bind_config_loglevel_twice_rec_string(void **state)
{
    torture_bind_config_loglevel(state,
                                 NULL,
                                 LIBSSH_TEST_BIND_CONFIG_LOGLEVEL_TWICE_REC_STRING,
                                 2);
}

static void
torture_bind_config_ciphers(void **state,
                            const char *file,
                            const char *string,
                            const char *expect)
{
    struct bind_st *test_state;
    ssh_bind bind;
    char *fips_ciphers = NULL;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    if (ssh_fips_mode()) {
        fips_ciphers = ssh_keep_fips_algos(SSH_CRYPT_C_S, expect);
        assert_non_null(fips_ciphers);
    }

    _parse_config(bind, file, string, SSH_OK);

    assert_non_null(bind->wanted_methods[SSH_CRYPT_C_S]);
    assert_non_null(bind->wanted_methods[SSH_CRYPT_S_C]);
    if (ssh_fips_mode()) {
        assert_string_equal(bind->wanted_methods[SSH_CRYPT_C_S], fips_ciphers);
        assert_string_equal(bind->wanted_methods[SSH_CRYPT_S_C], fips_ciphers);
        SAFE_FREE(fips_ciphers);
    } else {
        assert_string_equal(bind->wanted_methods[SSH_CRYPT_C_S], expect);
        assert_string_equal(bind->wanted_methods[SSH_CRYPT_S_C], expect);
    }
}

static void torture_bind_config_ciphers_file(void **state)
{
    torture_bind_config_ciphers(state,
                                LIBSSH_TEST_BIND_CONFIG_CIPHERS,
                                NULL,
                                CIPHERS);
}

static void torture_bind_config_ciphers_string(void **state)
{
    torture_bind_config_ciphers(state,
                                NULL,
                                LIBSSH_TEST_BIND_CONFIG_CIPHERS_STRING,
                                CIPHERS);
}

static void torture_bind_config_ciphers2_file(void **state)
{
    torture_bind_config_ciphers(state,
                                LIBSSH_TEST_BIND_CONFIG_CIPHERS2,
                                NULL,
                                CIPHERS2);
}

static void torture_bind_config_ciphers2_string(void **state)
{
    torture_bind_config_ciphers(state,
                                NULL,
                                LIBSSH_TEST_BIND_CONFIG_CIPHERS2_STRING,
                                CIPHERS2);
}

static void torture_bind_config_ciphers_twice_file(void **state)
{
    torture_bind_config_ciphers(state,
                                LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE,
                                NULL,
                                CIPHERS);
}

static void torture_bind_config_ciphers_twice_string(void **state)
{
    torture_bind_config_ciphers(state,
                                NULL,
                                LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_STRING,
                                CIPHERS);
}

static void torture_bind_config_ciphers_twice_rec_file(void **state)
{
    torture_bind_config_ciphers(state,
                                LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_REC,
                                NULL,
                                CIPHERS);
}

static void torture_bind_config_ciphers_twice_rec_string(void **state)
{
    torture_bind_config_ciphers(state,
                                NULL,
                                LIBSSH_TEST_BIND_CONFIG_CIPHERS_TWICE_REC_STRING,
                                CIPHERS);
}

static void
torture_bind_config_macs(void **state,
                         const char *file,
                         const char *string,
                         const char *expect)
{
    struct bind_st *test_state;
    ssh_bind bind;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    _parse_config(bind, file, string, SSH_OK);

    assert_non_null(bind->wanted_methods[SSH_MAC_C_S]);
    assert_non_null(bind->wanted_methods[SSH_MAC_S_C]);
    assert_string_equal(bind->wanted_methods[SSH_MAC_C_S], expect);
    assert_string_equal(bind->wanted_methods[SSH_MAC_S_C], expect);
}

static void torture_bind_config_macs_file(void **state)
{
    torture_bind_config_macs(state,
                             LIBSSH_TEST_BIND_CONFIG_MACS,
                             NULL,
                             MACS);
}

static void torture_bind_config_macs_string(void **state)
{
    torture_bind_config_macs(state,
                             NULL,
                             LIBSSH_TEST_BIND_CONFIG_MACS_STRING,
                             MACS);
}

static void torture_bind_config_macs2_file(void **state)
{
    torture_bind_config_macs(state,
                             LIBSSH_TEST_BIND_CONFIG_MACS2,
                             NULL,
                             MACS2);
}

static void torture_bind_config_macs2_string(void **state)
{
    torture_bind_config_macs(state,
                             NULL,
                             LIBSSH_TEST_BIND_CONFIG_MACS2_STRING,
                             MACS2);
}

static void torture_bind_config_macs_twice_file(void **state)
{
    torture_bind_config_macs(state,
                             LIBSSH_TEST_BIND_CONFIG_MACS_TWICE,
                             NULL,
                             MACS);
}

static void torture_bind_config_macs_twice_string(void **state)
{
    torture_bind_config_macs(state,
                             NULL,
                             LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_STRING,
                             MACS);
}

static void torture_bind_config_macs_twice_rec_file(void **state)
{
    torture_bind_config_macs(state,
                             LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_REC,
                             NULL,
                             MACS);
}

static void torture_bind_config_macs_twice_rec_string(void **state)
{
    torture_bind_config_macs(state,
                             NULL,
                             LIBSSH_TEST_BIND_CONFIG_MACS_TWICE_REC_STRING,
                             MACS);
}

static void
torture_bind_config_kexalgorithms(void **state,
                                  const char *file,
                                  const char *string,
                                  const char *expect)
{
    struct bind_st *test_state;
    ssh_bind bind;
    char *fips_kex = NULL;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    if (ssh_fips_mode()) {
        fips_kex = ssh_keep_fips_algos(SSH_KEX, expect);
        assert_non_null(fips_kex);
    }

    _parse_config(bind, file, string, SSH_OK);

    assert_non_null(bind->wanted_methods[SSH_KEX]);
    if (ssh_fips_mode()) {
        assert_string_equal(bind->wanted_methods[SSH_KEX], fips_kex);
        SAFE_FREE(fips_kex);
    } else {
        assert_string_equal(bind->wanted_methods[SSH_KEX], expect);
    }
}

static void torture_bind_config_kexalgorithms_file(void **state)
{
    torture_bind_config_kexalgorithms(state,
                                      LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS,
                                      NULL,
                                      KEXALGORITHMS);
}

static void torture_bind_config_kexalgorithms_string(void **state)
{
    torture_bind_config_kexalgorithms(state,
                                      NULL,
                                      LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_STRING,
                                      KEXALGORITHMS);
}

static void torture_bind_config_kexalgorithms2_file(void **state)
{
    torture_bind_config_kexalgorithms(state,
                                      LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS2,
                                      NULL,
                                      KEXALGORITHMS2);
}

static void torture_bind_config_kexalgorithms2_string(void **state)
{
    torture_bind_config_kexalgorithms(state,
                                      NULL,
                                      LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS2_STRING,
                                      KEXALGORITHMS2);
}

static void torture_bind_config_kexalgorithms_twice_file(void **state)
{
    torture_bind_config_kexalgorithms(state,
                                      LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE,
                                      NULL,
                                      KEXALGORITHMS);
}

static void torture_bind_config_kexalgorithms_twice_string(void **state)
{
    torture_bind_config_kexalgorithms(state,
                                      NULL,
                                      LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_STRING,
                                      KEXALGORITHMS);
}

static void torture_bind_config_kexalgorithms_twice_rec_file(void **state)
{
    torture_bind_config_kexalgorithms(state,
                                      LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_REC,
                                      NULL,
                                      KEXALGORITHMS);
}

static void torture_bind_config_kexalgorithms_twice_rec_string(void **state)
{
    torture_bind_config_kexalgorithms(state,
                                      NULL,
                                      LIBSSH_TEST_BIND_CONFIG_KEXALGORITHMS_TWICE_REC_STRING,
                                      KEXALGORITHMS);
}

static void
torture_bind_config_pubkey_accepted(void **state,
                                    const char *file,
                                    const char *string,
                                    const char *expect)
{
    struct bind_st *test_state;
    ssh_bind bind;
    char *fips_pubkeys = NULL;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    if (ssh_fips_mode()) {
        fips_pubkeys = ssh_keep_fips_algos(SSH_HOSTKEYS, expect);
        assert_non_null(fips_pubkeys);
    }

    _parse_config(bind, file, string, SSH_OK);

    assert_non_null(bind->pubkey_accepted_key_types);
    if (ssh_fips_mode()) {
        assert_string_equal(bind->pubkey_accepted_key_types, fips_pubkeys);
        SAFE_FREE(fips_pubkeys);
    } else {
        assert_string_equal(bind->pubkey_accepted_key_types, expect);
    }
}

static void torture_bind_config_pubkey_accepted_file(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED,
                                        NULL,
                                        PUBKEYACCEPTEDTYPES);
}

static void torture_bind_config_pubkey_accepted_string(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        NULL,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_STRING,
                                        PUBKEYACCEPTEDTYPES);
}

static void torture_bind_config_pubkey_accepted_twice_file(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE,
                                        NULL,
                                        PUBKEYACCEPTEDTYPES);
}

static void torture_bind_config_pubkey_accepted_twice_string(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        NULL,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_STRING,
                                        PUBKEYACCEPTEDTYPES);
}

static void torture_bind_config_pubkey_accepted_twice_rec_file(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_REC,
                                        NULL,
                                        PUBKEYACCEPTEDTYPES2);
}

static void torture_bind_config_pubkey_accepted_twice_rec_string(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        NULL,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_TWICE_REC_STRING,
                                        PUBKEYACCEPTEDTYPES2);
}

static void torture_bind_config_pubkey_accepted_unknown_file(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_UNKNOWN,
                                        NULL,
                                        PUBKEYACCEPTEDTYPES);
}

static void torture_bind_config_pubkey_accepted_unknown_string(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        NULL,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED_UNKNOWN_STRING,
                                        PUBKEYACCEPTEDTYPES);
}

static void torture_bind_config_pubkey_accepted2_file(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED2,
                                        NULL,
                                        PUBKEYACCEPTEDTYPES2);
}

static void torture_bind_config_pubkey_accepted2_string(void **state)
{
    torture_bind_config_pubkey_accepted(state,
                                        NULL,
                                        LIBSSH_TEST_BIND_CONFIG_PUBKEY_ACCEPTED2_STRING,
                                        PUBKEYACCEPTEDTYPES2);
}

static void
torture_bind_config_hostkey_algorithms(void **state,
                                       const char *file,
                                       const char *string,
                                       const char *expect)
{
    struct bind_st *test_state;
    ssh_bind bind;
    char *fips_hostkey = NULL;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    if (ssh_fips_mode()) {
        fips_hostkey = ssh_keep_fips_algos(SSH_HOSTKEYS, expect);
        assert_non_null(fips_hostkey);
    }

    _parse_config(bind, file, string, SSH_OK);

    assert_non_null(bind->wanted_methods[SSH_HOSTKEYS]);
    if (ssh_fips_mode()) {
        assert_string_equal(bind->wanted_methods[SSH_HOSTKEYS], fips_hostkey);
        SAFE_FREE(fips_hostkey);
    } else {
        assert_string_equal(bind->wanted_methods[SSH_HOSTKEYS], expect);
    }
}

static void torture_bind_config_hostkey_algorithms_file(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS,
                                           NULL,
                                           HOSTKEYALGORITHMS);
}

static void torture_bind_config_hostkey_algorithms_string(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           NULL,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_STRING,
                                           HOSTKEYALGORITHMS);
}

static void torture_bind_config_hostkey_algorithms_twice_file(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE,
                                           NULL,
                                           HOSTKEYALGORITHMS);
}

static void torture_bind_config_hostkey_algorithms_twice_string(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           NULL,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_STRING,
                                           HOSTKEYALGORITHMS);
}

static void torture_bind_config_hostkey_algorithms_twice_rec_file(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_REC,
                                           NULL,
                                           HOSTKEYALGORITHMS2);
}

static void torture_bind_config_hostkey_algorithms_twice_rec_string(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           NULL,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_TWICE_REC_STRING,
                                           HOSTKEYALGORITHMS2);
}

static void torture_bind_config_hostkey_algorithms2_file(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS2,
                                           NULL,
                                           HOSTKEYALGORITHMS2);
}

static void torture_bind_config_hostkey_algorithms2_string(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           NULL,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS2_STRING,
                                           HOSTKEYALGORITHMS2);
}

static void torture_bind_config_hostkey_algorithms_unknown_file(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_UNKNOWN,
                                           NULL,
                                           HOSTKEYALGORITHMS);
}

static void torture_bind_config_hostkey_algorithms_unknown_string(void **state)
{
    torture_bind_config_hostkey_algorithms(state,
                                           NULL,
                                           LIBSSH_TEST_BIND_CONFIG_HOSTKEY_ALGORITHMS_UNKNOWN_STRING,
                                           HOSTKEYALGORITHMS);
}

static int assert_full_bind_config(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int new_level;

    char *fips_ciphers = NULL;
    char *fips_kex = NULL;

    if (ssh_fips_mode()) {
        fips_ciphers = ssh_keep_fips_algos(SSH_CRYPT_C_S, CIPHERS);
        assert_non_null(fips_ciphers);
        fips_kex = ssh_keep_fips_algos(SSH_KEX, KEXALGORITHMS);
        assert_non_null(fips_kex);
    }

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    new_level = ssh_get_log_level();
    assert_int_equal(new_level, 2);

    assert_non_null(bind->bindaddr);
    assert_string_equal(bind->bindaddr, LISTEN_ADDRESS);

    assert_int_equal(bind->bindport, 123);

    assert_non_null(bind->ecdsakey);
    assert_string_equal(bind->ecdsakey, LIBSSH_ECDSA_521_TESTKEY);

    assert_non_null(bind->wanted_methods[SSH_CRYPT_C_S]);
    if (ssh_fips_mode()) {
        assert_string_equal(bind->wanted_methods[SSH_CRYPT_C_S], fips_ciphers);
    } else {
        assert_string_equal(bind->wanted_methods[SSH_CRYPT_C_S], CIPHERS);
    }

    assert_non_null(bind->wanted_methods[SSH_CRYPT_S_C]);
    if (ssh_fips_mode()) {
        assert_string_equal(bind->wanted_methods[SSH_CRYPT_S_C], fips_ciphers);
    } else {
        assert_string_equal(bind->wanted_methods[SSH_CRYPT_S_C], CIPHERS);
    }

    assert_non_null(bind->wanted_methods[SSH_MAC_S_C]);
    assert_string_equal(bind->wanted_methods[SSH_MAC_S_C], MACS);

    assert_non_null(bind->wanted_methods[SSH_MAC_C_S]);
    assert_string_equal(bind->wanted_methods[SSH_MAC_C_S], MACS);

    assert_non_null(bind->wanted_methods[SSH_KEX]);
    if (ssh_fips_mode()) {
        assert_string_equal(bind->wanted_methods[SSH_KEX], fips_kex);
    } else {
        assert_string_equal(bind->wanted_methods[SSH_KEX], KEXALGORITHMS);
    }

    SAFE_FREE(fips_ciphers);
    SAFE_FREE(fips_kex);

    return 0;
}

static void torture_bind_config_full(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind, LIBSSH_TEST_BIND_CONFIG_FULL);
    assert_int_equal(rc, 0);

    rc = assert_full_bind_config(state);
    assert_int_equal(rc, 0);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_include(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind, LIBSSH_TEST_BIND_CONFIG_INCLUDE);
    assert_int_equal(rc, 0);

    rc = assert_full_bind_config(state);
    assert_int_equal(rc, 0);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_include_recursive(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_INCLUDE_RECURSIVE);
    assert_int_equal(rc, 0);

    rc = assert_full_bind_config(state);
    assert_int_equal(rc, 0);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_include_recursive_loop(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_INCLUDE_RECURSIVE_LOOP);
    assert_int_equal(rc, 0);
}

/**
 * @brief Verify the configuration parser does not choke on unknown
 * or unsupported configuration options
 */
static void torture_bind_config_corner_cases(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    rc = ssh_bind_config_parse_file(bind, LIBSSH_TEST_BIND_CONFIG_CORNER_CASES);
    assert_int_equal(rc, 0);

    assert_non_null(bind->wanted_methods[SSH_CRYPT_C_S]);
    assert_string_equal(bind->wanted_methods[SSH_CRYPT_C_S], CIPHERS2);

    assert_non_null(bind->wanted_methods[SSH_CRYPT_S_C]);
    assert_string_equal(bind->wanted_methods[SSH_CRYPT_S_C], CIPHERS2);
}

static void torture_bind_config_match_all(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level, new_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_MATCH_ALL);
    assert_int_equal(rc, 0);

    new_level = ssh_get_log_level();
    assert_int_equal(new_level, 1);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_match_twice(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level, new_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_MATCH_TWICE);
    assert_int_equal(rc, 0);

    new_level = ssh_get_log_level();
    assert_int_equal(new_level, 1);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_match_unsupported(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_MATCH_UNSUPPORTED);
    assert_int_equal(rc, 0);

    rc = assert_full_bind_config(state);
    assert_int_equal(rc, 0);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_match_not_allowed(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_MATCH_NOT_ALLOWED);
    assert_int_equal(rc, 0);

    rc = assert_full_bind_config(state);
    assert_int_equal(rc, 0);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_match_corner_cases(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level, new_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_MATCH_CORNER_CASES);
    assert_int_equal(rc, 0);

    new_level = ssh_get_log_level();
    assert_int_equal(new_level, 3);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

static void torture_bind_config_match_invalid(void **state)
{
    struct bind_st *test_state;
    ssh_bind bind;
    int rc;
    int previous_level;

    assert_non_null(state);
    test_state = *((struct bind_st **)state);
    assert_non_null(test_state);
    assert_non_null(test_state->bind);
    bind = test_state->bind;

    previous_level = ssh_get_log_level();

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_MATCH_INVALID);
    assert_int_equal(rc, -1);

    rc = ssh_bind_config_parse_file(bind,
            LIBSSH_TEST_BIND_CONFIG_MATCH_INVALID2);
    assert_int_equal(rc, -1);

    rc = ssh_set_log_level(previous_level);
    assert_int_equal(rc, SSH_OK);
}

int torture_run_tests(void)
{
    int rc;
    struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(torture_bind_config_listen_address_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_listen_address_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_listen_address2_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_listen_address2_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_listen_address_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_listen_address_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_listen_address_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_listen_address_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_port_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_port_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_port2_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_port2_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_port_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_port_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_port_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_port_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_separately,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_loglevel_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_loglevel_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_loglevel1_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_loglevel1_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_loglevel_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_loglevel_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_loglevel_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_loglevel_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_ciphers_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_ciphers_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_ciphers2_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_ciphers2_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_ciphers_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_ciphers_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_ciphers_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_ciphers_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_macs_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_macs_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_macs2_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_macs2_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_macs_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_macs_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_macs_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_macs_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_kexalgorithms_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_kexalgorithms_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_kexalgorithms2_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_kexalgorithms2_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_kexalgorithms_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_kexalgorithms_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_kexalgorithms_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_kexalgorithms_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_full,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_include,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_include_recursive,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_include_recursive_loop,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_corner_cases,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_match_all,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_match_twice,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_match_unsupported,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_match_not_allowed,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_match_corner_cases,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_match_invalid,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted2_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted2_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted_unknown_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_pubkey_accepted_unknown_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms_twice_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms_twice_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms_twice_rec_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms_twice_rec_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms2_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms2_string,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms_unknown_file,
                sshbind_setup, sshbind_teardown),
        cmocka_unit_test_setup_teardown(torture_bind_config_hostkey_algorithms_unknown_string,
                sshbind_setup, sshbind_teardown),
    };

    ssh_init();
    torture_filter_tests(tests);
    rc = cmocka_run_group_tests(tests, NULL, NULL);
    ssh_finalize();
    return rc;
}
