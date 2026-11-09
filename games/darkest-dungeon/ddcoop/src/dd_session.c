#include "dd_session.h"

#include <string.h>

/* ---- portable helpers declared in dd_crypto.h ---- */

int dd_ct_equal(const uint8_t *a, const uint8_t *b, size_t len) {
    uint8_t d = 0;
    for (size_t i = 0; i < len; i++) d |= (uint8_t)(a[i] ^ b[i]);
    return d == 0;
}

void dd_wipe(void *p, size_t len) {
    volatile uint8_t *v = p;
    while (len--) *v++ = 0;
}

int dd_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len, const uint8_t *info,
                   size_t info_len, uint8_t *out, size_t out_len) {
    uint8_t prk[32], t[32], buf[32 + 256 + 1];
    if (out_len > 255 * 32 || info_len > 256) return 0;
    if (!dd_hmac_sha256(salt, salt_len, ikm, ikm_len, prk)) return 0; /* extract */
    size_t done = 0, t_len = 0;
    for (uint8_t i = 1; done < out_len; i++) { /* expand: T(i) = HMAC(PRK, T(i-1) || info || i) */
        memcpy(buf, t, t_len);
        memcpy(buf + t_len, info, info_len);
        buf[t_len + info_len] = i;
        if (!dd_hmac_sha256(prk, sizeof prk, buf, t_len + info_len + 1, t)) return 0;
        t_len = 32;
        size_t n = out_len - done < 32 ? out_len - done : 32;
        memcpy(out + done, t, n);
        done += n;
    }
    dd_wipe(prk, sizeof prk);
    dd_wipe(t, sizeof t);
    return 1;
}

/* ---- little endian ---- */

static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t get32(const uint8_t *p) { uint32_t v = 0; for (int i = 3; i >= 0; i--) v = v << 8 | p[i]; return v; }
static uint64_t get64(const uint8_t *p) { uint64_t v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | p[i]; return v; }

/* ---- handshake ---- */

static const char k_guest_label[] = "DDCoop guest proof";
static const char k_host_label[] = "DDCoop host proof";
static const char k_keys_label[] = "DDCoop session keys v1";

/* proof = HMAC(K, label || salt || host nonce || guest nonce) */
static int proof(const dd_session *s, const char *label, uint8_t out[32]) {
    uint8_t msg[32 + DD_SALT_LEN + 2 * DD_NONCE_LEN];
    size_t ll = strlen(label);
    memcpy(msg, label, ll);
    memcpy(msg + ll, s->salt, DD_SALT_LEN);
    memcpy(msg + ll + DD_SALT_LEN, s->host_nonce, DD_NONCE_LEN);
    memcpy(msg + ll + DD_SALT_LEN + DD_NONCE_LEN, s->guest_nonce, DD_NONCE_LEN);
    return dd_hmac_sha256(s->k, DD_KEY_LEN, msg, ll + DD_SALT_LEN + 2 * DD_NONCE_LEN, out);
}

static int derive_keys(dd_session *s) {
    uint8_t transcript[DD_SALT_LEN + 2 * DD_NONCE_LEN], keys[2 * DD_KEY_LEN];
    memcpy(transcript, s->salt, DD_SALT_LEN);
    memcpy(transcript + DD_SALT_LEN, s->host_nonce, DD_NONCE_LEN);
    memcpy(transcript + DD_SALT_LEN + DD_NONCE_LEN, s->guest_nonce, DD_NONCE_LEN);
    if (!dd_hkdf_sha256(transcript, sizeof transcript, s->k, DD_KEY_LEN, (const uint8_t *)k_keys_label,
                        strlen(k_keys_label), keys, sizeof keys))
        return 0;
    /* keys[0..31] guest->host, keys[32..63] host->guest */
    memcpy(s->key_tx, keys + (s->is_host ? DD_KEY_LEN : 0), DD_KEY_LEN);
    memcpy(s->key_rx, keys + (s->is_host ? 0 : DD_KEY_LEN), DD_KEY_LEN);
    dd_wipe(keys, sizeof keys);
    dd_wipe(s->k, DD_KEY_LEN);
    s->counter_tx = s->counter_rx = 0;
    s->state = DD_SESS_OPEN;
    return 1;
}

int dd_sess_host_start(dd_session *s, const char *password, uint8_t out[DD_HS1_LEN]) {
    memset(s, 0, sizeof *s);
    s->is_host = 1;
    s->iterations = DD_PBKDF2_ITERATIONS;
    if (!password || !password[0] || !dd_random(s->salt, DD_SALT_LEN) || !dd_random(s->host_nonce, DD_NONCE_LEN) ||
        !dd_pbkdf2_sha256(password, strlen(password), s->salt, DD_SALT_LEN, s->iterations, s->k)) {
        s->state = DD_SESS_FAILED;
        return 0;
    }
    memcpy(out, "DDH1", 4);
    out[4] = DD_SESSION_VERSION; out[5] = 0; out[6] = 0; out[7] = 0;
    memcpy(out + 8, s->salt, DD_SALT_LEN);
    memcpy(out + 24, s->host_nonce, DD_NONCE_LEN);
    put32(out + 56, s->iterations);
    s->state = DD_SESS_WAIT_HS2;
    return 1;
}

int dd_sess_guest_answer(dd_session *s, const char *password, const uint8_t in[DD_HS1_LEN], uint8_t out[DD_HS2_LEN]) {
    memset(s, 0, sizeof *s);
    s->state = DD_SESS_FAILED;
    if (!password || !password[0] || memcmp(in, "DDH1", 4) != 0 || in[4] != DD_SESSION_VERSION) return 0;
    s->iterations = get32(in + 56);
    if (s->iterations < DD_PBKDF2_MIN || s->iterations > DD_PBKDF2_MAX) return 0;
    memcpy(s->salt, in + 8, DD_SALT_LEN);
    memcpy(s->host_nonce, in + 24, DD_NONCE_LEN);
    if (!dd_random(s->guest_nonce, DD_NONCE_LEN) ||
        !dd_pbkdf2_sha256(password, strlen(password), s->salt, DD_SALT_LEN, s->iterations, s->k))
        return 0;
    memcpy(out, "DDH2", 4);
    memset(out + 4, 0, 4);
    memcpy(out + 8, s->guest_nonce, DD_NONCE_LEN);
    if (!proof(s, k_guest_label, out + 40)) return 0;
    s->state = DD_SESS_WAIT_HS3;
    return 1;
}

int dd_sess_host_finish(dd_session *s, const uint8_t in[DD_HS2_LEN], uint8_t out[DD_HS3_LEN]) {
    uint8_t expect[32];
    memcpy(out, "DDH3", 4);
    memset(out + 4, 0, DD_HS3_LEN - 4); /* result 0, no proof */
    if (s->state != DD_SESS_WAIT_HS2 || memcmp(in, "DDH2", 4) != 0) goto fail;
    memcpy(s->guest_nonce, in + 8, DD_NONCE_LEN);
    if (!proof(s, k_guest_label, expect) || !dd_ct_equal(expect, in + 40, 32)) goto fail;
    if (!proof(s, k_host_label, out + 8)) goto fail; /* our proof only goes to a guest that proved itself */
    put32(out + 4, 1);
    return derive_keys(s);
fail:
    dd_wipe(s->k, DD_KEY_LEN);
    s->state = DD_SESS_FAILED;
    return 0;
}

int dd_sess_guest_finish(dd_session *s, const uint8_t in[DD_HS3_LEN]) {
    uint8_t expect[32];
    if (s->state != DD_SESS_WAIT_HS3 || memcmp(in, "DDH3", 4) != 0 || get32(in + 4) != 1) goto fail;
    if (!proof(s, k_host_label, expect) || !dd_ct_equal(expect, in + 8, 32)) goto fail; /* a fake host fails here */
    return derive_keys(s);
fail:
    dd_wipe(s->k, DD_KEY_LEN);
    s->state = DD_SESS_FAILED;
    return 0;
}

/* ---- frames ---- */

static void frame_nonce(uint64_t counter, uint8_t nonce[DD_GCM_NONCE_LEN]) {
    memset(nonce, 0, 4);
    put64(nonce + 4, counter);
}

size_t dd_sess_seal(dd_session *s, const uint8_t *plain, size_t len, uint8_t *out, size_t cap) {
    if (s->state != DD_SESS_OPEN || len > DD_FRAME_MAX_PLAIN || cap < len + DD_FRAME_OVERHEAD) return 0;
    if (s->counter_tx == UINT64_MAX) return 0; /* never reuse a nonce */
    uint8_t nonce[DD_GCM_NONCE_LEN];
    put32(out, (uint32_t)(8 + len + DD_TAG_LEN));
    put64(out + 4, s->counter_tx);
    frame_nonce(s->counter_tx, nonce);
    if (!dd_aead_seal(s->key_tx, nonce, out, DD_FRAME_HEADER, plain, len, out + DD_FRAME_HEADER,
                      out + DD_FRAME_HEADER + len))
        return 0;
    s->counter_tx++;
    return len + DD_FRAME_OVERHEAD;
}

dd_open_result dd_sess_open(dd_session *s, const uint8_t *in, size_t in_len, uint8_t *out, size_t cap,
                            size_t *plain_len, size_t *used) {
    if (s->state != DD_SESS_OPEN) return DD_OPEN_BAD;
    if (in_len < 4) return DD_OPEN_SHORT;
    uint32_t rest = get32(in);
    if (rest < 8 + DD_TAG_LEN) return DD_OPEN_BAD;
    if (rest - 8 - DD_TAG_LEN > DD_FRAME_MAX_PLAIN) return DD_OPEN_TOO_BIG;
    if (in_len < 4 + (size_t)rest) return DD_OPEN_SHORT;
    size_t len = rest - 8 - DD_TAG_LEN;
    if (cap < len) return DD_OPEN_TOO_BIG;
    uint64_t counter = get64(in + 4);
    if (counter != s->counter_rx) return DD_OPEN_BAD; /* replayed, dropped or reordered */
    uint8_t nonce[DD_GCM_NONCE_LEN];
    frame_nonce(counter, nonce);
    if (!dd_aead_open(s->key_rx, nonce, in, DD_FRAME_HEADER, in + DD_FRAME_HEADER, len, in + DD_FRAME_HEADER + len, out))
        return DD_OPEN_BAD;
    s->counter_rx++;
    *plain_len = len;
    *used = 4 + rest;
    return DD_OPEN_OK;
}

void dd_sess_close(dd_session *s) { dd_wipe(s, sizeof *s); }
