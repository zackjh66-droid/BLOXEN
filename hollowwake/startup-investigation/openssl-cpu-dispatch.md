# Legacy OpenSSL CPU dispatch investigation

The exact 6.21 client contains the `OpenSSL 1.0.2g 1 Mar 2016` version string and `OPENSSL_ia32cap` configuration name. Windows run 39888034954 observed a first- and second-chance access violation at main RVA `0x155a3`, in the SHA-extension SHA-1 block routine's `movaps xmm6, [rax-0x48]` epilogue. The unchanged original executable also exited with `0xC0000005` during the early PAK-loading stage on that runner; its exact fault address was not captured in that run. That does not establish a regression caused by the separate experimental header-state change.

The published OpenSSL 1.0.2g Win64 SHA-extension generator uses RAX for the next input-block pointer in its loop, then uses RAX again to restore saved XMM registers and RSP in the epilogue. The inspected game instructions match this sequence. This is a concrete reason to test the library's existing CPU-dispatch fallback, rather than alter hashing results or PAK signature checks.

`OPENSSL_ia32cap=:~0x20000000` is interpreted by the original library's configuration parser. In this old parser, specifying the extended capability word also clears other leaf-7 dispatch flags. The policy selects existing library implementations; no algorithm, signature result, CPUID response, code branch or integrity check is replaced. Hollowwake supplies this default only through the game's CRT environment adapter and honors an existing nonempty setting. Other environment queries delegate to the real CRT. Original-source comparisons and initialization captures set the same process-local configuration explicitly.

Sources inspected from the upstream release tag, without executing or copying their implementation into Hollowwake:

- [Configuration documentation](https://github.com/openssl/openssl/blob/OpenSSL_1_0_2g/doc/crypto/OPENSSL_ia32cap.pod), SHA-256 `3e929adeeab25ad2c44ae52b0c0be9d78ec91853997222cf2fe5a0347702d60e`.
- [Configuration parser](https://github.com/openssl/openssl/blob/OpenSSL_1_0_2g/crypto/cryptlib.c), SHA-256 `8ad687d2273455d6c6adafd519d542cbce8e7399f79e76ee12a5266a15a0cf50`.
- [SHA-1 assembly generator](https://github.com/openssl/openssl/blob/OpenSSL_1_0_2g/crypto/sha/asm/sha1-x86_64.pl), SHA-256 `8b1234d8fb04d2e170de01c097cc94a9e10c4870f73d0b8391fd47357bcd3a4b`.

The runtime-header replay experiment is removed from the active launch path while its timing remains unproven. The confirmed 819 copied-import restoration remains. Actual full-client execution, original UI, account login and Xbox behavior require separate results; an environment-adapter fixture does not establish them.
