/* dd_crypto.h on OpenSSL 3 (Linux tests). */
#include "dd_crypto.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

int dd_random(void *buf, size_t len) { return RAND_bytes(buf, (int)len) == 1; }

int dd_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len, uint8_t out[32]) {
    unsigned int n = 0;
    return HMAC(EVP_sha256(), key, (int)key_len, msg, msg_len, out, &n) != NULL && n == 32;
}

int dd_pbkdf2_sha256(const char *password, size_t password_len, const uint8_t *salt, size_t salt_len,
                     uint32_t iterations, uint8_t out[DD_KEY_LEN]) {
    return PKCS5_PBKDF2_HMAC(password, (int)password_len, salt, (int)salt_len, (int)iterations, EVP_sha256(),
                             DD_KEY_LEN, out) == 1;
}

static int gcm(int enc, const uint8_t *key, const uint8_t *nonce, const uint8_t *aad, size_t aad_len,
               const uint8_t *in, size_t len, uint8_t *out, uint8_t *tag) {
    EVP_CIPHER_CTX *c = EVP_CIPHER_CTX_new();
    int ok = c != NULL, n = 0;
    ok = ok && EVP_CipherInit_ex(c, EVP_aes_256_gcm(), NULL, NULL, NULL, enc) == 1;
    ok = ok && EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_SET_IVLEN, DD_GCM_NONCE_LEN, NULL) == 1;
    ok = ok && EVP_CipherInit_ex(c, NULL, NULL, key, nonce, enc) == 1;
    if (ok && aad_len) ok = EVP_CipherUpdate(c, NULL, &n, aad, (int)aad_len) == 1;
    if (ok && len) ok = EVP_CipherUpdate(c, out, &n, in, (int)len) == 1;
    if (ok && !enc) ok = EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_SET_TAG, DD_TAG_LEN, tag) == 1;
    ok = ok && EVP_CipherFinal_ex(c, out + (len ? (size_t)n : 0), &n) == 1; /* fails on a bad tag */
    if (ok && enc) ok = EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_GCM_GET_TAG, DD_TAG_LEN, tag) == 1;
    EVP_CIPHER_CTX_free(c);
    return ok;
}

int dd_aead_seal(const uint8_t key[DD_KEY_LEN], const uint8_t nonce[DD_GCM_NONCE_LEN], const uint8_t *aad, size_t aad_len,
                 const uint8_t *plain, size_t len, uint8_t *out, uint8_t tag[DD_TAG_LEN]) {
    return gcm(1, key, nonce, aad, aad_len, plain, len, out, tag);
}

int dd_aead_open(const uint8_t key[DD_KEY_LEN], const uint8_t nonce[DD_GCM_NONCE_LEN], const uint8_t *aad, size_t aad_len,
                 const uint8_t *cipher, size_t len, const uint8_t tag[DD_TAG_LEN], uint8_t *out) {
    uint8_t t[DD_TAG_LEN];
    for (int i = 0; i < DD_TAG_LEN; i++) t[i] = tag[i];
    return gcm(0, key, nonce, aad, aad_len, cipher, len, out, t);
}
