#!/bin/bash

# The client keys are stored in a SoftHSM device.

TESTDIR=$1
PRIVKEY=$2
OBJNAME=$3
TOKENLABEL=$3 # yeah. The same as object label
LOADPUBLIC=$4
LIBSOFTHSM_PATH=$5
P11_KIT_CLIENT=$6
shift 5

PUBKEY="$PRIVKEY.pub"

echo "TESTDIR: $TESTDIR"
echo "PRIVKEY: $PRIVKEY"
echo "PUBKEY: $PUBKEY"
echo "OBJNAME: $OBJNAME"
echo "TOKENLABEL: $TOKENLABEL"
echo "LOADPUBLIC: $LOADPUBLIC"

if [ ! -d "$TESTDIR/db" ]; then
    # Create temporary directory for tokens
    install -d -m 0755 "$TESTDIR/db"

    # Create SoftHSM configuration file
    cat >"$TESTDIR/softhsm.conf" <<EOF
directories.tokendir = $TESTDIR/db
objectstore.backend = file
log.level = DEBUG
EOF

    cat "$TESTDIR/softhsm.conf"
fi

export SOFTHSM2_CONF=$TESTDIR/softhsm.conf

#init -- each object will have its own token
cmd="softhsm2-util --init-token --label $TOKENLABEL --free --pin 1234 --so-pin 1234"
eval echo "$cmd"
out=$(eval "$cmd")
ret=$?
if [ $ret -ne 0 ]; then
    echo "Init token failed"
    echo "$out"
    exit 1
fi

#load private key
cmd="p11tool --provider $LIBSOFTHSM_PATH --write --load-privkey $PRIVKEY --label $OBJNAME --login --set-pin=1234 \"pkcs11:token=$TOKENLABEL\""
eval echo "$cmd"
out=$(eval "$cmd")
ret=$?
if [ $ret -ne 0 ]; then
   echo "Loading privkey failed"
   echo "$out"
   exit 1
fi

cat "$PUBKEY"

ls -l "$TESTDIR"

if [ "$LOADPUBLIC" -ne 0 ]; then
#load public key
    cmd="p11tool --provider $LIBSOFTHSM_PATH --write --load-pubkey $PUBKEY --label $OBJNAME --login --set-pin=1234 \"pkcs11:token=$TOKENLABEL\""
    eval echo "$cmd"
    out=$(eval "$cmd")
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "Loading pubkey failed"
        echo "$out"
        exit 1
    fi
fi

cmd="p11tool --list-all --login \"pkcs11:token=$TOKENLABEL\" --set-pin=1234"
eval echo "$cmd"
out=$(eval "$cmd")
ret=$?
if [ $ret -ne 0 ]; then
    echo "Logging in failed"
    echo "$out"
    exit 1
fi
echo "$out"

# Skip the p11-kit if not needed
if [ -z "$P11_KIT_CLIENT" ]; then
    exit 0
fi

# when creating more keys, we need to restart the p11-kit
# so it can pick up the new keys
if [ -h "$TESTDIR/p11-kit-server.socket" ]; then
    kill -9 "$(cat "$TESTDIR/p11-kit-server.pid")"
    rm "$TESTDIR/p11-kit-server.socket"
fi

# p11-kit complains if there is no runtime directory
if [ -z "$XDG_RUNTIME_DIR" ]; then
    export XDG_RUNTIME_DIR=$PWD
fi

# Start the p11-kit server
cmd="p11-kit server --provider $LIBSOFTHSM_PATH pkcs11:"
echo "$cmd"
out=$(eval "$cmd")
ret=$?
if [ $ret -ne 0 ]; then
    echo "Starting p11-kit server failed"
    echo "$out"
    exit 1
fi
eval "$out"

# Symlink the p11-kit-server socket to "known place"
P11_KIT_SERVER_ADDRESS_PATH=${P11_KIT_SERVER_ADDRESS:10}
cmd="ln -s $P11_KIT_SERVER_ADDRESS_PATH $TESTDIR/p11-kit-server.socket"
echo "$cmd"
out=$(eval "$cmd")

# Save the PID for the C code to clean up
cmd="echo $P11_KIT_SERVER_PID > $TESTDIR/p11-kit-server.pid"
echo "$cmd"
out=$(eval "$cmd")

cmd="pkcs11-tool -O --login --pin=1234 --module=$P11_KIT_CLIENT --token-label=$TOKENLABEL"
echo "$cmd"
out=$(eval "$cmd")
ret=$?
echo "$out"
if [ $ret -ne 0 ]; then
    echo "Failed to list keys through p11-kit remoting"
    echo "$out"
    exit 1
fi

exit 0
