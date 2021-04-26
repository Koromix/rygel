// Heavily inspired from https://github.com/whs/tweetnacl-sealed-box
// But without the crazy "max three lines of code per file" code style.

nacl.sealedBox = new function(m, pk) {
    this.seal = function(m, pk) {
        let ek = nacl.box.keyPair();
        let nonce = makeNonce(ek.publicKey, pk);
        let boxed = nacl.box(m, nonce, pk, ek.secretKey);

        let overhead = nacl.box.publicKeyLength + nacl.box.overheadLength;
        let c = new Uint8Array(overhead + m.length);
        c.set(ek.publicKey);
        c.set(boxed, ek.publicKey.length);

        for(var i = 0; i < ek.secretKey.length; i++)
            ek.secretKey[i] = 0;

        return c;
    };

    this.open = function(c, pk, sk) {
        let epk = c.subarray(0, nacl.box.publicKeyLength);
        let boxed = c.subarray(nacl.box.publicKeyLength);
        let nonce = makeNonce(epk, pk);

        let m = nacl.box.open(boxed, nonce, epk, sk);

        return m;
    };

    function makeNonce(pk1, pk2) {
        let state = blake2b.init(nacl.box.nonceLength, null);

        blake2b.update(state, pk1);
        blake2b.update(state, pk2);

        return blake2b.finalize(state);
    }
};
