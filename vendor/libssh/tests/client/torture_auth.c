/*
 * This file is part of the SSH Library
 *
 * Copyright (c) 2010 by Aris Adamantiadis
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
#include "libssh/libssh.h"
#include "libssh/priv.h"
#include "libssh/session.h"

#include <errno.h>
#include <sys/types.h>
#include <pwd.h>

/* agent_is_running */
#include "agent.c"

static int sshd_setup(void **state)
{
    torture_setup_sshd_server(state, true);

    return 0;
}

static int sshd_teardown(void **state) {
    torture_teardown_sshd_server(state);

    return 0;
}

static int session_setup(void **state)
{
    struct torture_state *s = *state;
    int verbosity = torture_libssh_verbosity();
    const char *all_keytypes = NULL;
    struct passwd *pwd;
    bool b = false;
    int rc;

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    rc = setuid(pwd->pw_uid);
    assert_return_code(rc, errno);

    s->ssh.session = ssh_new();
    assert_non_null(s->ssh.session);

    ssh_options_set(s->ssh.session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    ssh_options_set(s->ssh.session, SSH_OPTIONS_HOST, TORTURE_SSH_SERVER);
    /* Make sure no other configuration options from system will get used */
    rc = ssh_options_set(s->ssh.session, SSH_OPTIONS_PROCESS_CONFIG, &b);
    assert_ssh_return_code(s->ssh.session, rc);

    /* Enable all hostkeys */
    all_keytypes = ssh_kex_get_supported_method(SSH_HOSTKEYS);
    rc = ssh_options_set(s->ssh.session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES, all_keytypes);
    assert_ssh_return_code(s->ssh.session, rc);

    return 0;
}

static int session_teardown(void **state)
{
    struct torture_state *s = *state;

    ssh_disconnect(s->ssh.session);
    ssh_free(s->ssh.session);

    return 0;
}

static int pubkey_setup(void **state)
{
    int rc;

    rc = session_setup(state);
    if (rc != 0) {
        return rc;
    }

    /* Make sure we do not interfere with another ssh-agent */
    unsetenv("SSH_AUTH_SOCK");
    unsetenv("SSH_AGENT_PID");

    return 0;
}

static int agent_setup(void **state)
{
    struct torture_state *s = *state;
    char ssh_agent_cmd[4096];
    char ssh_agent_sock[1024];
    char ssh_agent_pidfile[1024];
    char bob_ssh_key[1024];
    struct passwd *pwd;
    int rc;

    rc = pubkey_setup(state);
    if (rc != 0) {
        return rc;
    }

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    snprintf(ssh_agent_sock,
             sizeof(ssh_agent_sock),
             "%s/agent.sock",
             s->socket_dir);

    snprintf(ssh_agent_pidfile,
             sizeof(ssh_agent_pidfile),
             "%s/agent.pid",
             s->socket_dir);

    /* Production ready code!!! */
    snprintf(ssh_agent_cmd,
             sizeof(ssh_agent_cmd),
             "eval `ssh-agent -a %s`; echo $SSH_AGENT_PID > %s",
             ssh_agent_sock, ssh_agent_pidfile);

    /* run ssh-agent and ssh-add as the normal user */
    unsetenv("UID_WRAPPER_ROOT");

    rc = system(ssh_agent_cmd);
    assert_return_code(rc, errno);

    setenv("SSH_AUTH_SOCK", ssh_agent_sock, 1);
    setenv("TORTURE_SSH_AGENT_PIDFILE", ssh_agent_pidfile, 1);

    snprintf(bob_ssh_key,
             sizeof(bob_ssh_key),
             "ssh-add %s/.ssh/id_rsa",
             pwd->pw_dir);

    rc = system(bob_ssh_key);
    assert_return_code(rc, errno);

    return 0;
}

static int agent_cert_setup(void **state)
{
    char bob_alt_ssh_key[1024];
    struct passwd *pwd;
    int rc;

    rc = agent_setup(state);
    if (rc != 0) {
        return rc;
    }

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    /* remove all keys, load alternative key + cert */
    snprintf(bob_alt_ssh_key,
             sizeof(bob_alt_ssh_key),
             "ssh-add -D && ssh-add %s/.ssh_cert/id_rsa",
             pwd->pw_dir);

    rc = system(bob_alt_ssh_key);
    assert_return_code(rc, errno);

    return 0;
}

static int agent_teardown(void **state)
{
    const char *ssh_agent_pidfile;
    int rc;

    rc = session_teardown(state);
    if (rc != 0) {
        return rc;
    }

    ssh_agent_pidfile = getenv("TORTURE_SSH_AGENT_PIDFILE");
    assert_non_null(ssh_agent_pidfile);

    /* kill agent pid */
    rc = torture_terminate_process(ssh_agent_pidfile);
    assert_return_code(rc, errno);

    unlink(ssh_agent_pidfile);

    unsetenv("TORTURE_SSH_AGENT_PIDFILE");
    unsetenv("SSH_AUTH_SOCK");

    return 0;
}

static void torture_auth_none(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_BOB);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_none(session,NULL);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
}

static void torture_auth_none_nonblocking(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }

    ssh_set_blocking(session,0);

    do {
        rc = ssh_userauth_none(session,NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_DENIED);
    assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);

}

static void torture_auth_pubkey(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    char bob_ssh_key[1024];
    ssh_key privkey = NULL;
    struct passwd *pwd = NULL;
    int rc;

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    snprintf(bob_ssh_key,
             sizeof(bob_ssh_key),
             "%s/.ssh/id_rsa",
             pwd->pw_dir);

    /* Authenticate as alice with bob his pubkey */
    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_none(session, NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }

    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    rc = ssh_pki_import_privkey_file(bob_ssh_key, NULL, NULL, NULL, &privkey);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_try_publickey(session, NULL, privkey);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    rc = ssh_userauth_publickey(session, NULL, privkey);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    SSH_KEY_FREE(privkey);
}

static void torture_auth_pubkey_nonblocking(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    char bob_ssh_key[1024];
    ssh_key privkey = NULL;
    struct passwd *pwd = NULL;
    int rc;

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    snprintf(bob_ssh_key,
             sizeof(bob_ssh_key),
             "%s/.ssh/id_rsa",
             pwd->pw_dir);

    /* Authenticate as alice with bob his pubkey */
    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    ssh_set_blocking(session, 0);

    do {
        rc = ssh_userauth_none(session,NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_DENIED);
    assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);

    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    rc = ssh_pki_import_privkey_file(bob_ssh_key, NULL, NULL, NULL, &privkey);
    assert_int_equal(rc, SSH_OK);

    do {
        rc = ssh_userauth_try_publickey(session, NULL, privkey);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    do {
        rc = ssh_userauth_publickey(session, NULL, privkey);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    SSH_KEY_FREE(privkey);
}

static void torture_auth_autopubkey(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    /* Authenticate as alice with bob his pubkey */
    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_none(session,NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);
}

struct torture_auth_autopubkey_protected_data {
    ssh_session session;
    int n_calls;
};

static int
torture_auth_autopubkey_protected_auth_function (const char *prompt, char *buf, size_t len,
                                                 int echo, int verify, void *userdata)
{
    int rc;
    char *id, *expected_id;
    struct torture_auth_autopubkey_protected_data *data = userdata;

    assert_true(prompt != NULL);
    assert_int_equal(echo, 0);
    assert_int_equal(verify, 0);

    expected_id = ssh_path_expand_escape(data->session, "%d/id_rsa_protected");
    assert_true(expected_id != NULL);

    rc = ssh_userauth_publickey_auto_get_current_identity(data->session, &id);
    assert_int_equal(rc, SSH_OK);

    assert_string_equal(expected_id, id);

    ssh_string_free_char(id);
    ssh_string_free_char(expected_id);

    data->n_calls += 1;
    strncpy(buf, "secret", len);
    return 0;
}

static void torture_auth_autopubkey_protected(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    char *id;
    int rc;

    struct torture_auth_autopubkey_protected_data data = {
        .session = session,
        .n_calls = 0
    };

    struct ssh_callbacks_struct callbacks = {
        .userdata = &data,
        .auth_function = torture_auth_autopubkey_protected_auth_function
    };

    /* no session pointer */
    rc = ssh_userauth_publickey_auto_get_current_identity(NULL, &id);
    assert_int_equal(rc, SSH_ERROR);

    /* no result pointer */
    rc = ssh_userauth_publickey_auto_get_current_identity(session, NULL);
    assert_int_equal(rc, SSH_ERROR);

    /* no auto auth going on */
    rc = ssh_userauth_publickey_auto_get_current_identity(session, &id);
    assert_int_equal(rc, SSH_ERROR);

    ssh_callbacks_init(&callbacks);
    ssh_set_callbacks(session, &callbacks);

    /* Authenticate as alice with bob his pubkey */
    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    /* Try id_rsa_protected first.
     */
    rc = ssh_options_set(session, SSH_OPTIONS_IDENTITY, "%d/id_rsa_protected");
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_none(session,NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    assert_int_equal (data.n_calls, 1);
}

static void torture_auth_autopubkey_nonblocking(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    ssh_set_blocking(session,0);
    do {
      rc = ssh_userauth_none(session, NULL);
    } while (rc == SSH_AUTH_AGAIN);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }

    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    do {
        rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);
}

static void torture_auth_kbdint(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_BOB);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_none(session,NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_INTERACTIVE);

    rc = ssh_userauth_kbdint(session, NULL, NULL);
    assert_int_equal(rc, SSH_AUTH_INFO);
    assert_int_equal(ssh_userauth_kbdint_getnprompts(session), 1);

    rc = ssh_userauth_kbdint_setanswer(session, 0, TORTURE_SSH_USER_BOB_PASSWORD);
    assert_false(rc < 0);

    rc = ssh_userauth_kbdint(session, NULL, NULL);
    /* Sometimes, SSH server send an empty query at the end of exchange */
    if(rc == SSH_AUTH_INFO) {
        assert_int_equal(ssh_userauth_kbdint_getnprompts(session), 0);
        rc = ssh_userauth_kbdint(session, NULL, NULL);
    }
    assert_int_equal(rc, SSH_AUTH_SUCCESS);
}

static void torture_auth_kbdint_nonblocking(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_BOB);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    ssh_set_blocking(session,0);
    do {
      rc = ssh_userauth_none(session, NULL);
    } while (rc == SSH_AUTH_AGAIN);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_INTERACTIVE);

    do {
        rc = ssh_userauth_kbdint(session, NULL, NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_INFO);
    assert_int_equal(ssh_userauth_kbdint_getnprompts(session), 1);
    rc = ssh_userauth_kbdint_setanswer(session, 0, TORTURE_SSH_USER_BOB_PASSWORD);
    assert_false(rc < 0);

    do {
        rc = ssh_userauth_kbdint(session, NULL, NULL);
    } while (rc == SSH_AUTH_AGAIN);
    /* Sometimes, SSH server send an empty query at the end of exchange */
    if(rc == SSH_AUTH_INFO) {
        assert_int_equal(ssh_userauth_kbdint_getnprompts(session), 0);
        do {
            rc = ssh_userauth_kbdint(session, NULL, NULL);
        } while (rc == SSH_AUTH_AGAIN);
    }
    assert_int_equal(rc, SSH_AUTH_SUCCESS);
}

static void torture_auth_password(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_BOB);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_none(session, NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_AUTH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PASSWORD);

    rc = ssh_userauth_password(session, NULL, TORTURE_SSH_USER_BOB_PASSWORD);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);
}

static void torture_auth_password_nonblocking(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_BOB);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    ssh_set_blocking(session,0);
    do {
      rc = ssh_userauth_none(session, NULL);
    } while (rc == SSH_AUTH_AGAIN);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_AUTH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }

    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PASSWORD);

    do {
      rc = ssh_userauth_password(session, NULL, TORTURE_SSH_USER_BOB_PASSWORD);
    } while(rc==SSH_AUTH_AGAIN);

    assert_int_equal(rc, SSH_AUTH_SUCCESS);
}

static void torture_auth_agent(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    if (!ssh_agent_is_running(session)){
        print_message("*** Agent not running. Test ignored\n");
        return;
    }
    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_none(session,NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    rc = ssh_userauth_agent(session, NULL);
    assert_ssh_return_code(session, rc);
}

static void torture_auth_agent_nonblocking(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    if (!ssh_agent_is_running(session)){
        print_message("*** Agent not running. Test ignored\n");
        return;
    }
    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_none(session,NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    ssh_set_blocking(session,0);

    do {
      rc = ssh_userauth_agent(session, NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_ssh_return_code(session, rc);
}

static void torture_auth_cert(void **state) {
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    ssh_key privkey = NULL;
    ssh_key cert = NULL;
    char bob_ssh_key[1024];
    char bob_ssh_cert[2048];
    struct passwd *pwd;
    int rc;

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    snprintf(bob_ssh_key,
             sizeof(bob_ssh_key),
             "%s/.ssh_cert/id_rsa",
             pwd->pw_dir);
    snprintf(bob_ssh_cert,
             sizeof(bob_ssh_cert),
             "%s-cert.pub",
             bob_ssh_key);

    /* cert has been signed for login as alice */
    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_connect(session);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_pki_import_privkey_file(bob_ssh_key, NULL, NULL, NULL, &privkey);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_pki_import_cert_file(bob_ssh_cert, &cert);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_pki_copy_cert_to_privkey(cert, privkey);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_try_publickey(session, NULL, cert);
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_publickey(session, NULL, privkey);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    SSH_KEY_FREE(privkey);
    SSH_KEY_FREE(cert);
}

static void torture_auth_agent_cert(void **state)
{
#if OPENSSH_VERSION_MAJOR < 8
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    /* Skip this test if in FIPS mode.
     *
     * OpenSSH agent has a bug which makes it to not use SHA2 in signatures when
     * using certificates. It always uses SHA1.
     *
     * This should be removed as soon as OpenSSH agent bug is fixed.
     * (see https://gitlab.com/libssh/libssh-mirror/merge_requests/34) */
    if (ssh_fips_mode()) {
        skip();
    } else {
        /* After the bug is solved, this also should be removed */
        rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                             "ssh-rsa-cert-v01@openssh.com");
        assert_int_equal(rc, SSH_OK);
    }
#endif /* OPENSSH_VERSION_MAJOR < 8 */

    /* Setup loads a different key, tests are exactly the same. */
    torture_auth_agent(state);
}

static void torture_auth_agent_cert_nonblocking(void **state)
{
#if OPENSSH_VERSION_MAJOR < 8
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    /* Skip this test if in FIPS mode.
     *
     * OpenSSH agent has a bug which makes it to not use SHA2 in signatures when
     * using certificates. It always uses SHA1.
     *
     * This should be removed as soon as OpenSSH agent bug is fixed.
     * (see https://gitlab.com/libssh/libssh-mirror/merge_requests/34) */
    if (ssh_fips_mode()) {
        skip();
    } else {
        /* After the bug is solved, this also should be removed */
        rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                             "ssh-rsa-cert-v01@openssh.com");
        assert_int_equal(rc, SSH_OK);
    }
#endif /* OPENSSH_VERSION_MAJOR < 8 */

    torture_auth_agent_nonblocking(state);
}

static void torture_auth_pubkey_types(void **state)
{
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_ssh_return_code(session, rc);

    rc = ssh_connect(session);
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_none(session, NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    /* Disable RSA key types for authentication */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ecdsa-sha2-nistp384");
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* Now enable it and retry */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "rsa-sha2-512,ssh-rsa");
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);
}

static void torture_auth_pubkey_types_ecdsa(void **state)
{
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_ssh_return_code(session, rc);

    rc = ssh_connect(session);
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_none(session, NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    /* We have only the 256b key -- whitelisting only larger should fail */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ecdsa-sha2-nistp384");
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* Verify we can use also ECDSA keys with their various names */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ecdsa-sha2-nistp256");
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

}

static void torture_auth_pubkey_types_ed25519(void **state)
{
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    char bob_ssh_key[1024];
    ssh_key privkey = NULL;
    struct passwd *pwd;
    int rc;

    if (ssh_fips_mode()) {
        skip();
    }

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    snprintf(bob_ssh_key,
             sizeof(bob_ssh_key),
             "%s/.ssh/id_ed25519",
             pwd->pw_dir);

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_ssh_return_code(session, rc);

    rc = ssh_connect(session);
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_none(session, NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    /* Import the ED25519 private key */
    rc = ssh_pki_import_privkey_file(bob_ssh_key, NULL, NULL, NULL, &privkey);
    assert_int_equal(rc, SSH_OK);

    /* Enable only RSA keys -- authentication should fail */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ssh-rsa");
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_publickey(session, NULL, privkey);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* Verify we can use also ed25519 keys */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ssh-ed25519");
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_publickey(session, NULL, privkey);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    SSH_KEY_FREE(privkey);
}

static void torture_auth_pubkey_types_nonblocking(void **state)
{
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_ssh_return_code(session, rc);

    rc = ssh_connect(session);
    assert_ssh_return_code(session, rc);

    ssh_set_blocking(session, 0);
    do {
      rc = ssh_userauth_none(session, NULL);
    } while (rc == SSH_AUTH_AGAIN);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }

    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    /* Disable RSA key types for authentication */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ecdsa-sha2-nistp521");
    assert_ssh_return_code(session, rc);

    do {
        rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* Now enable it and retry */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "rsa-sha2-512,ssh-rsa");
    assert_ssh_return_code(session, rc);

    do {
        rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

}

static void torture_auth_pubkey_types_ecdsa_nonblocking(void **state)
{
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    int rc;

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_ssh_return_code(session, rc);

    rc = ssh_connect(session);
    assert_ssh_return_code(session, rc);

    ssh_set_blocking(session, 0);
    do {
      rc = ssh_userauth_none(session, NULL);
    } while (rc == SSH_AUTH_AGAIN);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }

    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    /* We have only the 256b key -- whitelisting only larger should fail */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ecdsa-sha2-nistp384");
    assert_ssh_return_code(session, rc);

    do {
        rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* Verify we can use also ECDSA key to authenticate */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ecdsa-sha2-nistp256");
    assert_ssh_return_code(session, rc);

    do {
        rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

}

static void torture_auth_pubkey_types_ed25519_nonblocking(void **state)
{
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    char bob_ssh_key[1024];
    ssh_key privkey = NULL;
    struct passwd *pwd;
    int rc;

    if (ssh_fips_mode()) {
        skip();
    }

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    snprintf(bob_ssh_key,
             sizeof(bob_ssh_key),
             "%s/.ssh/id_ed25519",
             pwd->pw_dir);

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_ssh_return_code(session, rc);

    rc = ssh_connect(session);
    assert_ssh_return_code(session, rc);

    ssh_set_blocking(session, 0);
    do {
      rc = ssh_userauth_none(session, NULL);
    } while (rc == SSH_AUTH_AGAIN);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }

    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    /* Import the ED25519 private key */
    rc = ssh_pki_import_privkey_file(bob_ssh_key, NULL, NULL, NULL, &privkey);
    assert_int_equal(rc, SSH_OK);

    /* Enable only RSA keys -- authentication should fail */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ssh-rsa");
    assert_ssh_return_code(session, rc);

    do {
        rc = ssh_userauth_publickey(session, NULL, privkey);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* Verify we can use also ED25519 key to authenticate */
    rc = ssh_options_set(session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES,
                         "ssh-ed25519");
    assert_ssh_return_code(session, rc);

    do {
        rc = ssh_userauth_publickey(session, NULL, privkey);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    SSH_KEY_FREE(privkey);
}

static void torture_auth_pubkey_rsa_key_size(void **state)
{
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    char bob_ssh_key[1024];
    ssh_key privkey = NULL;
    struct passwd *pwd;
    int rc;
    unsigned int limit = 4096;

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    snprintf(bob_ssh_key,
             sizeof(bob_ssh_key),
             "%s/.ssh/id_rsa",
             pwd->pw_dir);

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_ssh_return_code(session, rc);

    rc = ssh_connect(session);
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_none(session, NULL);
    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }
    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    /* set unreasonable large minimum key size to trigger the condition */
    rc = ssh_options_set(session, SSH_OPTIONS_RSA_MIN_SIZE, &limit); /* larger than the test key */
    assert_ssh_return_code(session, rc);

    /* Import the RSA private key */
    rc = ssh_pki_import_privkey_file(bob_ssh_key, NULL, NULL, NULL, &privkey);
    assert_int_equal(rc, SSH_OK);

    rc = ssh_userauth_publickey(session, NULL, privkey);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* revert to default values which should work also in FIPS mode */
    limit = 0;
    rc = ssh_options_set(session, SSH_OPTIONS_RSA_MIN_SIZE, &limit);
    assert_ssh_return_code(session, rc);

    rc = ssh_userauth_publickey(session, NULL, privkey);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    SSH_KEY_FREE(privkey);
}

static void torture_auth_pubkey_rsa_key_size_nonblocking(void **state)
{
    struct torture_state *s = *state;
    ssh_session session = s->ssh.session;
    char bob_ssh_key[1024];
    ssh_key privkey = NULL;
    struct passwd *pwd;
    int rc;
    unsigned int limit = 4096;

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    snprintf(bob_ssh_key,
             sizeof(bob_ssh_key),
             "%s/.ssh/id_rsa",
             pwd->pw_dir);

    rc = ssh_options_set(session, SSH_OPTIONS_USER, TORTURE_SSH_USER_ALICE);
    assert_ssh_return_code(session, rc);

    rc = ssh_connect(session);
    assert_ssh_return_code(session, rc);

    ssh_set_blocking(session, 0);
    do {
      rc = ssh_userauth_none(session, NULL);
    } while (rc == SSH_AUTH_AGAIN);

    /* This request should return a SSH_REQUEST_DENIED error */
    if (rc == SSH_ERROR) {
        assert_int_equal(ssh_get_error_code(session), SSH_REQUEST_DENIED);
    }

    rc = ssh_userauth_list(session, NULL);
    assert_true(rc & SSH_AUTH_METHOD_PUBLICKEY);

    /* set unreasonable large minimum key size to trigger the condition */
    rc = ssh_options_set(session, SSH_OPTIONS_RSA_MIN_SIZE, &limit); /* larger than the test key */
    assert_ssh_return_code(session, rc);

    /* Import the RSA private key */
    rc = ssh_pki_import_privkey_file(bob_ssh_key, NULL, NULL, NULL, &privkey);
    assert_int_equal(rc, SSH_OK);

    do {
        rc = ssh_userauth_publickey(session, NULL, privkey);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_DENIED);

    /* revert to default values which should work also in FIPS mode */
    limit = 0;
    rc = ssh_options_set(session, SSH_OPTIONS_RSA_MIN_SIZE, &limit);
    assert_ssh_return_code(session, rc);

    do {
        rc = ssh_userauth_publickey(session, NULL, privkey);
    } while (rc == SSH_AUTH_AGAIN);
    assert_int_equal(rc, SSH_AUTH_SUCCESS);

    SSH_KEY_FREE(privkey);
}

int torture_run_tests(void) {
    int rc;
    struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(torture_auth_none,
                                        session_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_none_nonblocking,
                                        session_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_password,
                                        session_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_password_nonblocking,
                                        session_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_kbdint,
                                        session_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_kbdint_nonblocking,
                                        session_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_nonblocking,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_autopubkey,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_autopubkey_protected,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_autopubkey_nonblocking,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_agent,
                                        agent_setup,
                                        agent_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_agent_nonblocking,
                                        agent_setup,
                                        agent_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_cert,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_agent_cert,
                                        agent_cert_setup,
                                        agent_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_agent_cert_nonblocking,
                                        agent_cert_setup,
                                        agent_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_types,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_types_nonblocking,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_types_ecdsa,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_types_ecdsa_nonblocking,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_types_ed25519,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_types_ed25519_nonblocking,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_rsa_key_size,
                                        pubkey_setup,
                                        session_teardown),
        cmocka_unit_test_setup_teardown(torture_auth_pubkey_rsa_key_size_nonblocking,
                                        pubkey_setup,
                                        session_teardown),
    };

    ssh_init();
    torture_filter_tests(tests);
    rc = cmocka_run_group_tests(tests, sshd_setup, sshd_teardown);
    ssh_finalize();

    return rc;
}
