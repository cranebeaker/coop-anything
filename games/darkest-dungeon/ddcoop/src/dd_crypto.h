/* The cryptographic building blocks the session layer needs, from the operating system's library:
 * Windows CNG (dd_crypto_win.c, in the mod) and OpenSSL (dd_crypto_openssl.c, for the Linux tests).
 * Both are checked against the same published test vectors (tests/test_session.c). Nothing here is
 * home-made cryptography; only HKDF is composed from HMAC, exactly as RFC 5869 defines it. */
#ifndef DD_CRYPTO_H
#define DD_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#define DD_KEY_LEN 32
#define DD_TAG_LEN 16
#define DD_GCM_NONCE_LEN 12

/* All return 1 on success, 0 on failure. */
int dd_random(void *buf, size_t len);
int dd_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len, uint8_t out[32]);
int dd_pbkdf2_sha256(const char *password, size_t password_len, const uint8_t *salt, size_t salt_len,
                     uint32_t iterations, uint8_t out[DD_KEY_LEN]);
/* AES-256-GCM. dd_aead_open returns 1 only if the tag is valid; on 0, out must not be used. */
int dd_aead_seal(const uint8_t key[DD_KEY_LEN], const uint8_t nonce[DD_GCM_NONCE_LEN], const uint8_t *aad, size_t aad_len,
                 const uint8_t *plain, size_t len, uint8_t *out, uint8_t tag[DD_TAG_LEN]);
int dd_aead_open(const uint8_t key[DD_KEY_LEN], const uint8_t nonce[DD_GCM_NONCE_LEN], const uint8_t *aad, size_t aad_len,
                 const uint8_t *cipher, size_t len, const uint8_t tag[DD_TAG_LEN], uint8_t *out);

/* Portable helpers (dd_session.c). */
int dd_ct_equal(const uint8_t *a, const uint8_t *b, size_t len); /* constant time */
void dd_wipe(void *p, size_t len);
/* HKDF-SHA256 (RFC 5869): extract with salt, then expand info into out (out_len <= 255*32). */
int dd_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len, const uint8_t *info,
                   size_t info_len, uint8_t *out, size_t out_len);

#endif
