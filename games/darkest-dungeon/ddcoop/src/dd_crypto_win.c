/* dd_crypto.h on Windows CNG (bcrypt.dll), used in the mod. */
#define WIN32_LEAN_AND_MEAN
#include "dd_crypto.h"

#include <windows.h>
#include <bcrypt.h>
#include <string.h>

#define OK(s) ((s) >= 0) /* NTSTATUS success */

int dd_random(void *buf, size_t len) {
    return OK(BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

int dd_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len, uint8_t out[32]) {
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE h = NULL;
    int ok = OK(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG));
    ok = ok && OK(BCryptCreateHash(alg, &h, NULL, 0, (PUCHAR)key, (ULONG)key_len, 0));
    ok = ok && OK(BCryptHashData(h, (PUCHAR)msg, (ULONG)msg_len, 0));
    ok = ok && OK(BCryptFinishHash(h, out, 32, 0));
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

int dd_pbkdf2_sha256(const char *password, size_t password_len, const uint8_t *salt, size_t salt_len,
                     uint32_t iterations, uint8_t out[DD_KEY_LEN]) {
    BCRYPT_ALG_HANDLE alg = NULL;
    int ok = OK(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG));
    ok = ok && OK(BCryptDeriveKeyPBKDF2(alg, (PUCHAR)password, (ULONG)password_len, (PUCHAR)salt, (ULONG)salt_len,
                                        iterations, out, DD_KEY_LEN, 0));
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

static int gcm(int enc, const uint8_t *key, const uint8_t *nonce, const uint8_t *aad, size_t aad_len,
               const uint8_t *in, size_t len, uint8_t *out, uint8_t *tag) {
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE k = NULL;
    int ok = OK(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0));
    ok = ok && OK(BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                                    sizeof(BCRYPT_CHAIN_MODE_GCM), 0));
    ok = ok && OK(BCryptGenerateSymmetricKey(alg, &k, NULL, 0, (PUCHAR)key, DD_KEY_LEN, 0));
    if (ok) {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce = (PUCHAR)nonce;
        info.cbNonce = DD_GCM_NONCE_LEN;
        info.pbAuthData = (PUCHAR)aad;
        info.cbAuthData = (ULONG)aad_len;
        info.pbTag = tag;
        info.cbTag = DD_TAG_LEN;
        ULONG done = 0;
        /* BCrypt rejects a NULL buffer even for zero length; any valid pointer will do then. */
        uint8_t empty = 0;
        PUCHAR src = len ? (PUCHAR)in : &empty, dst = len ? out : &empty;
        ok = enc ? OK(BCryptEncrypt(k, src, (ULONG)len, &info, NULL, 0, dst, (ULONG)len, &done, 0))
                 : OK(BCryptDecrypt(k, src, (ULONG)len, &info, NULL, 0, dst, (ULONG)len, &done, 0)); /* bad tag fails */
    }
    if (k) BCryptDestroyKey(k);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

int dd_aead_seal(const uint8_t key[DD_KEY_LEN], const uint8_t nonce[DD_GCM_NONCE_LEN], const uint8_t *aad, size_t aad_len,
                 const uint8_t *plain, size_t len, uint8_t *out, uint8_t tag[DD_TAG_LEN]) {
    return gcm(1, key, nonce, aad, aad_len, plain, len, out, tag);
}

int dd_aead_open(const uint8_t key[DD_KEY_LEN], const uint8_t nonce[DD_GCM_NONCE_LEN], const uint8_t *aad, size_t aad_len,
                 const uint8_t *cipher, size_t len, const uint8_t tag[DD_TAG_LEN], uint8_t *out) {
    uint8_t t[DD_TAG_LEN];
    memcpy(t, tag, DD_TAG_LEN);
    return gcm(0, key, nonce, aad, aad_len, cipher, len, out, t);
}
