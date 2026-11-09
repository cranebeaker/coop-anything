/* Tests for src/dd_session.c and the crypto backend: published test vectors first, then the handshake
 * and frame rules. Linux (OpenSSL backend):
 *   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc \
 *       -o build/test_session tests/test_session.c src/dd_session.c src/dd_crypto_openssl.c src/dd_proto.c -lcrypto
 *   setarch x86_64 -R build/test_session */
#include "dd_proto.h"
#include "dd_session.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

/* 1 if needle occurs in hay (portable stand-in for memmem). */
static int contains(const uint8_t *hay, size_t hay_len, const uint8_t *needle, size_t needle_len) {
    for (size_t i = 0; needle_len && i + needle_len <= hay_len; i++)
        if (memcmp(hay + i, needle, needle_len) == 0) return 1;
    return 0;
}

static void unhex(const char *h, uint8_t *out) {
    for (size_t i = 0; h[2 * i]; i++) {
        unsigned v;
        sscanf(h + 2 * i, "%2x", &v);
        out[i] = (uint8_t)v;
    }
}
static int eqhex(const uint8_t *got, const char *hex) {
    uint8_t want[128];
    size_t n = strlen(hex) / 2;
    unhex(hex, want);
    return memcmp(got, want, n) == 0;
}

static void test_vectors(void) {
    uint8_t out[64], key[64];

    /* HMAC-SHA256, RFC 4231 test cases 1 and 2 */
    memset(key, 0x0b, 20);
    dd_hmac_sha256(key, 20, (const uint8_t *)"Hi There", 8, out);
    CHECK(eqhex(out, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"), "HMAC RFC 4231 #1");
    dd_hmac_sha256((const uint8_t *)"Jefe", 4, (const uint8_t *)"what do ya want for nothing?", 28, out);
    CHECK(eqhex(out, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"), "HMAC RFC 4231 #2");

    /* PBKDF2-HMAC-SHA256 ("password", "salt") */
    dd_pbkdf2_sha256("password", 8, (const uint8_t *)"salt", 4, 1, out);
    CHECK(eqhex(out, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"), "PBKDF2 c=1");
    dd_pbkdf2_sha256("password", 8, (const uint8_t *)"salt", 4, 4096, out);
    CHECK(eqhex(out, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"), "PBKDF2 c=4096");

    /* HKDF-SHA256, RFC 5869 test case 1 */
    uint8_t ikm[22], salt[13], info[10];
    memset(ikm, 0x0b, sizeof ikm);
    for (int i = 0; i < 13; i++) salt[i] = (uint8_t)i;
    for (int i = 0; i < 10; i++) info[i] = (uint8_t)(0xf0 + i);
    dd_hkdf_sha256(salt, 13, ikm, 22, info, 10, out, 42);
    CHECK(eqhex(out, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"), "HKDF RFC 5869 #1");

    /* AES-256-GCM, McGrew/Viega test cases 13 (empty) and 14 (16 zero bytes), zero key and IV */
    uint8_t k0[32] = {0}, iv[12] = {0}, zero[16] = {0}, ct[16], tag[16], back[16];
    CHECK(dd_aead_seal(k0, iv, NULL, 0, NULL, 0, ct, tag) && eqhex(tag, "530f8afbc74536b9a963b4f1c4cb738b"), "GCM #13 tag");
    CHECK(dd_aead_seal(k0, iv, NULL, 0, zero, 16, ct, tag), "GCM #14 seal");
    CHECK(eqhex(ct, "cea7403d4d606b6e074ec5d3baf39d18") && eqhex(tag, "d0d1c8a799996bf0265b98b5d48ab919"), "GCM #14 ct/tag");
    CHECK(dd_aead_open(k0, iv, NULL, 0, ct, 16, tag, back) && memcmp(back, zero, 16) == 0, "GCM #14 open");
    tag[0] ^= 1;
    CHECK(!dd_aead_open(k0, iv, NULL, 0, ct, 16, tag, back), "GCM bad tag must fail");
}

/* Runs a full handshake; returns 1 if both sides ended OPEN. */
static int handshake(dd_session *host, dd_session *guest, const char *host_pw, const char *guest_pw) {
    uint8_t hs1[DD_HS1_LEN], hs2[DD_HS2_LEN], hs3[DD_HS3_LEN];
    if (!dd_sess_host_start(host, host_pw, hs1)) return 0;
    if (!dd_sess_guest_answer(guest, guest_pw, hs1, hs2)) return 0;
    int host_ok = dd_sess_host_finish(host, hs2, hs3);
    int guest_ok = dd_sess_guest_finish(guest, hs3);
    return host_ok && guest_ok && host->state == DD_SESS_OPEN && guest->state == DD_SESS_OPEN;
}

static void test_handshake(void) {
    dd_session h, g;
    CHECK(handshake(&h, &g, "correct horse battery", "correct horse battery"), "handshake with the right password");

    /* a co-op message both ways */
    dd_msg m;
    memset(&m, 0, sizeof m);
    m.type = DD_MSG_TOWN_REQUEST;
    m.u.town.request_id = 5;
    m.u.town.action.kind = DD_ACT_RECRUIT;
    m.u.town.action.u.hero.hero_id = 30;
    uint8_t plain[300], frame[400], back[400];
    size_t n = dd_encode(&m, plain, sizeof plain), plen, used;
    size_t f = dd_sess_seal(&g, plain, n, frame, sizeof frame);
    CHECK(f == n + DD_FRAME_OVERHEAD, "seal size");
    CHECK(!contains(frame, f, plain + DD_PROTO_HEADER, n - DD_PROTO_HEADER), "the message must not appear in clear");
    CHECK(dd_sess_open(&h, frame, f, back, sizeof back, &plen, &used) == DD_OPEN_OK && plen == n && used == f &&
              memcmp(back, plain, n) == 0, "guest -> host");
    f = dd_sess_seal(&h, plain, n, frame, sizeof frame);
    CHECK(dd_sess_open(&g, frame, f, back, sizeof back, &plen, &used) == DD_OPEN_OK && memcmp(back, plain, n) == 0,
          "host -> guest");
    CHECK(dd_sess_open(&h, frame, f, back, sizeof back, &plen, &used) == DD_OPEN_BAD,
          "a host frame must not open on the host (separate keys per direction)");

    /* different sessions, different keys */
    dd_session h2, g2;
    handshake(&h2, &g2, "correct horse battery", "correct horse battery");
    CHECK(memcmp(h.key_tx, h2.key_tx, DD_KEY_LEN) != 0, "fresh keys per session");

    /* wrong password: the host refuses, the guest is told so */
    dd_session h3, g3;
    CHECK(!handshake(&h3, &g3, "correct horse battery", "wrong"), "wrong password must fail");
    CHECK(h3.state == DD_SESS_FAILED && g3.state == DD_SESS_FAILED, "both sides failed");

    /* fake host: does not know the password but claims success with a made-up proof */
    dd_session gx, hx;
    uint8_t hs1[DD_HS1_LEN], hs2[DD_HS2_LEN], hs3[DD_HS3_LEN];
    dd_sess_host_start(&hx, "attacker guess", hs1);
    CHECK(dd_sess_guest_answer(&gx, "correct horse battery", hs1, hs2), "guest answers any well-formed HS1");
    memcpy(hs3, "DDH3", 4);
    hs3[4] = 1; hs3[5] = hs3[6] = hs3[7] = 0;
    dd_random(hs3 + 8, 32);
    CHECK(!dd_sess_guest_finish(&gx, hs3), "a guest must reject a host that cannot prove the password");

    /* downgrade / denial of service through the iteration count */
    dd_session gd, hd;
    dd_sess_host_start(&hd, "pw", hs1);
    hs1[56] = 0xe8; hs1[57] = 0x03; hs1[58] = 0; hs1[59] = 0; /* 1000 iterations */
    CHECK(!dd_sess_guest_answer(&gd, "pw", hs1, hs2), "too few iterations refused");
    hs1[56] = 0; hs1[57] = 0; hs1[58] = 0; hs1[59] = 0x10; /* 268 million */
    CHECK(!dd_sess_guest_answer(&gd, "pw", hs1, hs2), "too many iterations refused");
    CHECK(!dd_sess_host_start(&hd, "", hs1), "an empty password is refused");
}

static void test_frames(void) {
    dd_session h, g;
    handshake(&h, &g, "frames", "frames");
    uint8_t plain[64], frame[128], back[128], f2[128];
    size_t plen, used;
    for (int i = 0; i < 64; i++) plain[i] = (uint8_t)i;
    size_t f = dd_sess_seal(&g, plain, sizeof plain, frame, sizeof frame);

    /* every single-byte change must be detected */
    int undetected = 0;
    for (size_t i = 0; i < f; i++) {
        uint8_t c[128];
        memcpy(c, frame, f);
        c[i] ^= 0x01;
        dd_session hc = h;
        dd_open_result r = dd_sess_open(&hc, c, f, back, sizeof back, &plen, &used);
        if (r == DD_OPEN_OK) undetected++;
    }
    CHECK(undetected == 0, "%d tampered frames were accepted", undetected);

    CHECK(dd_sess_open(&h, frame, f, back, sizeof back, &plen, &used) == DD_OPEN_OK, "original opens");
    CHECK(dd_sess_open(&h, frame, f, back, sizeof back, &plen, &used) == DD_OPEN_BAD, "replay refused");

    size_t fa = dd_sess_seal(&g, plain, 10, frame, sizeof frame);
    size_t fb = dd_sess_seal(&g, plain, 20, f2, sizeof f2);
    CHECK(dd_sess_open(&h, f2, fb, back, sizeof back, &plen, &used) == DD_OPEN_BAD, "reordered frame refused");
    CHECK(dd_sess_open(&h, frame, fa, back, sizeof back, &plen, &used) == DD_OPEN_OK, "in-order frame still opens");

    for (size_t cut = 0; cut < fb; cut++)
        CHECK(dd_sess_open(&h, f2, cut, back, sizeof back, &plen, &used) == DD_OPEN_SHORT, "partial frame %zu", cut);
    CHECK(dd_sess_open(&h, f2, fb, back, sizeof back, &plen, &used) == DD_OPEN_OK, "complete frame opens");

    uint8_t huge[8] = {0xff, 0xff, 0xff, 0x7f, 0, 0, 0, 0};
    CHECK(dd_sess_open(&h, huge, sizeof huge, back, sizeof back, &plen, &used) == DD_OPEN_TOO_BIG, "oversized length");

    dd_session closed;
    memset(&closed, 0, sizeof closed);
    CHECK(dd_sess_seal(&closed, plain, 4, frame, sizeof frame) == 0, "no sealing before the handshake");
    dd_sess_close(&h);
    CHECK(h.state == 0 && h.key_rx[0] == 0 && h.key_rx[31] == 0, "close wipes the keys");
}

int main(void) {
    test_vectors();
    test_handshake();
    test_frames();
    printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
