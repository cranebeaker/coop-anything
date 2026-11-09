/* Authenticated, encrypted session between host and guest, independent of the transport.
 *
 * Handshake (three fixed-size plaintext messages; the password itself never crosses the network):
 *   HS1 host->guest : "DDH1", version, salt[16], host nonce[32], PBKDF2 iterations
 *   HS2 guest->host : "DDH2", guest nonce[32], guest proof = HMAC(K, "DDCoop guest proof" || T)
 *   HS3 host->guest : "DDH3", result, host proof  = HMAC(K, "DDCoop host proof"  || T)
 *   K = PBKDF2-HMAC-SHA256(password, salt, iterations), T = salt || host nonce || guest nonce.
 *   The host answers with its proof only after checking the guest's, and the guest checks the host's:
 *   both sides prove knowledge of the password (mutual). Session keys = HKDF-SHA256(salt = T, ikm = K,
 *   info = "DDCoop session keys v1"), one AES-256-GCM key per direction.
 * Known limit: someone who records a handshake can try passwords offline against the proofs, so the
 * password must be long and random (the host generates one).
 *
 * Frames after the handshake: u32 length of the rest | u64 counter | ciphertext | 16-byte tag.
 * nonce = 4 zero bytes || counter (little endian); the header is authenticated too. Each side's counter
 * starts at 0 and must arrive exactly in order (TCP), so a replayed, dropped or reordered frame fails. */
#ifndef DD_SESSION_H
#define DD_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "dd_crypto.h"

#define DD_SESSION_VERSION 1
#define DD_SALT_LEN 16
#define DD_NONCE_LEN 32
#define DD_HS1_LEN 60
#define DD_HS2_LEN 72
#define DD_HS3_LEN 40
#define DD_PBKDF2_ITERATIONS 200000
#define DD_PBKDF2_MIN 100000   /* a guest refuses fewer (downgrade) ... */
#define DD_PBKDF2_MAX 2000000  /* ... or more (a hostile host making the guest spin) */
#define DD_FRAME_HEADER 12
#define DD_FRAME_OVERHEAD (DD_FRAME_HEADER + DD_TAG_LEN)
#define DD_FRAME_MAX_PLAIN (1u << 20)

typedef enum { DD_SESS_NEW = 0, DD_SESS_WAIT_HS2, DD_SESS_WAIT_HS3, DD_SESS_OPEN, DD_SESS_FAILED } dd_sess_state;

typedef struct {
    int is_host;
    dd_sess_state state;
    uint8_t salt[DD_SALT_LEN], host_nonce[DD_NONCE_LEN], guest_nonce[DD_NONCE_LEN];
    uint32_t iterations;
    uint8_t k[DD_KEY_LEN];                     /* PBKDF2 output, wiped once the session is open */
    uint8_t key_tx[DD_KEY_LEN], key_rx[DD_KEY_LEN];
    uint64_t counter_tx, counter_rx;
} dd_session;

typedef enum {
    DD_OPEN_OK = 0,
    DD_OPEN_SHORT,     /* need more bytes */
    DD_OPEN_TOO_BIG,   /* length beyond DD_FRAME_MAX_PLAIN: drop the connection */
    DD_OPEN_BAD,       /* authentication failed, wrong counter, or the session is not open */
} dd_open_result;

/* Host: begins a handshake. Writes HS1 (DD_HS1_LEN bytes) to out. */
int dd_sess_host_start(dd_session *s, const char *password, uint8_t out[DD_HS1_LEN]);
/* Host: checks HS2. Writes HS3 (DD_HS3_LEN) to out in both cases; returns 1 if the guest knew the password. */
int dd_sess_host_finish(dd_session *s, const uint8_t in[DD_HS2_LEN], uint8_t out[DD_HS3_LEN]);
/* Guest: answers HS1 with HS2 (DD_HS2_LEN). Returns 0 for a malformed or unacceptable HS1. */
int dd_sess_guest_answer(dd_session *s, const char *password, const uint8_t in[DD_HS1_LEN], uint8_t out[DD_HS2_LEN]);
/* Guest: checks HS3. Returns 1 if the host accepted us AND proved it knows the password. */
int dd_sess_guest_finish(dd_session *s, const uint8_t in[DD_HS3_LEN]);

/* Seals plain into out (needs len + DD_FRAME_OVERHEAD bytes). Returns the frame size, 0 on error. */
size_t dd_sess_seal(dd_session *s, const uint8_t *plain, size_t len, uint8_t *out, size_t cap);
/* Opens the frame at the start of in. On OK: *plain_len bytes in out, *used = frame size. */
dd_open_result dd_sess_open(dd_session *s, const uint8_t *in, size_t in_len, uint8_t *out, size_t cap,
                            size_t *plain_len, size_t *used);

void dd_sess_close(dd_session *s); /* wipes all key material */

#endif
