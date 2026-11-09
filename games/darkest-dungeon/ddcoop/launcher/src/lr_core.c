#include "lr_core.h"
#include "lr_snapshot.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void logf_(lr_core *c, const char *fmt, ...) {
    if (!c->cfg.log) return;
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    c->cfg.log(c->cfg.ctx, line);
}

static void set_status(lr_core *c, const char *text) {
    snprintf(c->status, sizeof c->status, "%s", text);
    logf_(c, "status: %s", text);
}

static void fail(lr_core *c, const char *text) {
    snprintf(c->error, sizeof c->error, "%s", text);
    c->state = LR_ST_FAILED;
    logf_(c, "ERROR: %s", text);
}

const char *lr_state_name(lr_state s) {
    static const char *names[] = {"idle",         "host_waiting",   "host_sending", "host_playing", "guest_hello",
                                  "guest_receiving", "guest_launch", "guest_playing", "failed"};
    return (unsigned)s < sizeof names / sizeof *names ? names[s] : "?";
}

enum { SP_HELLO, SP_SESSION, SP_BEGIN, SP_FILES, SP_CHUNKS, SP_END, SP_DONE }; /* host send phases */

static void files_free(lr_core *c) {
    for (uint32_t i = 0; i < LR_MAX_FILES; i++) {
        free(c->files[i].data);
        memset(&c->files[i], 0, sizeof c->files[i]);
    }
    c->file_count = c->total = c->sent_file = c->sent_offset = c->sent_bytes = c->recv_bytes = 0;
    c->send_phase = 0;
}

void lr_core_init(lr_core *c, const lr_config *cfg) {
    memset(c, 0, sizeof *c);
    c->cfg = *cfg;
    if (c->cfg.coop_slot < 1 || c->cfg.coop_slot > 9) c->cfg.coop_slot = 3;
    set_status(c, "ready");
}

void lr_core_free(lr_core *c) {
    if (c->relay_up) lr_relay_free(&c->relay);
    c->relay_up = 0;
    files_free(c);
}

/* ---- sending ---- */

static int send_msg(lr_core *c, const lr_msg *m) {
    uint8_t buf[LR_MAX_MSG];
    size_t n = lr_encode(m, buf);
    if (n == 0 || c->peer == 0) return -1;
    return c->cfg.send(c->cfg.ctx, c->peer, lr_channel_of(m->type), buf, n);
}

/* One-off control messages (BYE, SAVE_RESULT). If Steam's buffer is full the message waits in a resend slot
 * that the tick flushes, so a result is never lost. */
static void send_simple_to(lr_core *c, uint64_t peer, lr_type t, uint32_t a) {
    lr_msg m = {0};
    m.type = t;
    m.a = a;
    size_t n = lr_encode(&m, c->resend);
    if (n == 0) return;
    if (c->cfg.send(c->cfg.ctx, peer, LR_CH_CONTROL, c->resend, n) == 0) {
        c->resend_len = n;
        c->resend_peer = peer;
    }
}

static void flush_resend(lr_core *c) {
    if (c->resend_len && c->cfg.send(c->cfg.ctx, c->resend_peer, LR_CH_CONTROL, c->resend, c->resend_len) != 0)
        c->resend_len = 0;
}

static void send_hello(lr_core *c, uint64_t now_ms) {
    lr_msg m = {0};
    m.type = LR_HELLO;
    m.a = LR_ROLE_GUEST;
    m.b = c->cfg.build;
    m.id = c->cfg.kit;
    send_msg(c, &m);
    c->hello_at = now_ms;
}

static int relay_send(void *ctx, uint64_t peer, int channel, const uint8_t *buf, size_t len) {
    lr_core *c = ctx;
    return c->cfg.send(c->cfg.ctx, peer, channel, buf, len);
}

/* ---- hero ownership ---- */

int lr_core_player(const lr_core *c) { return c->role == LR_ROLE_GUEST ? LR_PLAYER_GUEST : LR_PLAYER_HOST; }

static void owners_save(lr_core *c) {
    static char text[16384];
    if (!c->cfg.heroes_path[0]) return;
    size_t n = lr_owners_ini(&c->owners, lr_core_player(c), text, sizeof text);
    if (n == 0 || !lr_write_file(c->cfg.heroes_path, (const uint8_t *)text, n))
        logf_(c, "could not write %s", c->cfg.heroes_path);
}

/* One log line: revision and "id:h/g" per hero (no names; logs get shared). */
static void owners_log(lr_core *c, const char *why) {
    char list[400];
    size_t k = 0;
    list[0] = 0;
    for (int i = 0; i < c->owners.count && k + 16 < sizeof list; i++)
        k += (size_t)snprintf(list + k, sizeof list - k, " %u:%c", (unsigned)c->owners.heroes[i].id,
                              c->owners.owner[i] == LR_PLAYER_HOST ? 'h' : 'g');
    logf_(c, "heroes (%s): revision %u, %d heroes:%s", why, (unsigned)c->owners.revision, c->owners.count, list);
}

/* Host: a roster file from the campaign. restore = also take over the choices of our last session as host. */
static void host_roster(lr_core *c, const uint8_t *buf, size_t len, int restore) {
    static lr_hero heroes[LR_MAX_HEROES];
    int n = lr_roster_parse(buf, len, heroes, LR_MAX_HEROES);
    if (n < 0) {
        logf_(c, "roster unreadable (%zu bytes); hero ownership unchanged", len);
        return;
    }
    int changed = lr_owners_set_roster(&c->owners, heroes, n);
    uint8_t *old;
    size_t old_len;
    if (restore && c->cfg.heroes_path[0] && lr_read_file(c->cfg.heroes_path, 1 << 20, &old, &old_len)) {
        if (lr_owners_ini_from_host((const char *)old, old_len))
            changed |= lr_owners_apply_ini(&c->owners, (const char *)old, old_len);
        free(old);
    }
    if (changed) {
        owners_save(c);
        owners_log(c, restore ? "campaign" : "roster changed");
    }
}

void lr_core_host_roster(lr_core *c, const uint8_t *buf, size_t len) {
    if (c->role != LR_ROLE_HOST || c->state == LR_ST_IDLE || c->state == LR_ST_FAILED) return;
    host_roster(c, buf, len, 0);
}

/* Guest: sends queued ticks in order, as far as Steam takes them now; the tick retries the rest. */
static void guest_send_claims(lr_core *c) {
    int sent = 0;
    while (sent < c->claim_count) {
        lr_msg m = {0};
        m.type = LR_CLAIM;
        m.id = c->claims[sent].id;
        m.a = c->claims[sent].claim;
        int r = send_msg(c, &m);
        if (r == 0) break; /* full: next tick */
        sent++;            /* sent, or the host is gone (then the tick ends the session anyway) */
    }
    memmove(c->claims, c->claims + sent, sizeof c->claims[0] * (size_t)(c->claim_count - sent));
    c->claim_count -= sent;
}

int lr_core_claim(lr_core *c, uint32_t hero_id, int claim) {
    if (c->state == LR_ST_IDLE || c->state == LR_ST_FAILED) return 0;
    if (c->role == LR_ROLE_HOST) {
        if (!lr_owners_claim(&c->owners, hero_id, LR_PLAYER_HOST, claim)) return 0;
        owners_save(c);
        owners_log(c, "host ticked");
        return 1;
    }
    if (c->state != LR_ST_GUEST_LAUNCH && c->state != LR_ST_GUEST_PLAYING) return 0;
    if (c->claim_count >= LR_MAX_CLAIMS) return 0;
    c->claims[c->claim_count].id = hero_id;
    c->claims[c->claim_count].claim = claim ? 1 : 0;
    c->claim_count++;
    guest_send_claims(c);
    return 1;
}

/* Host: the guest gets every new revision of the table (and the whole table again after a refused tick). */
static void host_send_owners(lr_core *c) {
    static uint8_t body[LR_CHUNK];
    if (c->state != LR_ST_HOST_PLAYING || !c->peer || !c->owners.revision || c->owners_sent == c->owners.revision)
        return;
    size_t n = lr_owners_encode(&c->owners, body, sizeof body);
    lr_msg m = {0};
    m.type = LR_ROSTER;
    m.id = c->owners.revision;
    m.data = body;
    m.len = n;
    if (n && send_msg(c, &m) > 0) c->owners_sent = c->owners.revision;
}

/* ---- ini and save folders ---- */

int lr_write_ini(const char *path, const char *role, int port, const char *password, int coop_slot) {
    char text[1024];
    int n = snprintf(text, sizeof text,
                     "; Written by DDCoop.exe. Do not edit: DDCoop rewrites this file for every session.\r\n"
                     "; role=off means a normal game start (from Steam) stays single player.\r\n"
                     "[net]\r\nrole=%s\r\nport=%d\r\nbind_any=0\r\nhost=127.0.0.1\r\npassword=%s\r\n\r\n"
                     "[coop]\r\ncoop_slots=%d\r\n\r\n[log]\r\nverbose=0\r\n",
                     role, port, password, coop_slot);
    return n > 0 && (size_t)n < sizeof text && lr_write_file(path, (const uint8_t *)text, (size_t)n);
}

int lr_find_save_folder(const char *remote_dir, int coop_slot, char *name, size_t cap) {
    const char *forms[] = {"profile_%d", "steam_profile_%d"};
    for (int i = 0; i < 2; i++) {
        char folder[LR_NAME_MAX + 1], path[LR_PATH_MAX];
        snprintf(folder, sizeof folder, forms[i], coop_slot - 1);
        lr_join(path, sizeof path, remote_dir, folder);
        if (lr_dir_exists(path)) {
            snprintf(name, cap, "%s", folder);
            return 1;
        }
    }
    return 0;
}

int lr_list_campaigns(const char *remote_dir, lr_campaign_entry out[9]) {
    int n = 0;
    for (int slot = 1; slot <= 9; slot++) {
        lr_campaign_entry *e = &out[slot - 1];
        char folder[LR_NAME_MAX + 1], dir[LR_PATH_MAX], path[LR_PATH_MAX];
        lr_game_info game;
        memset(e, 0, sizeof *e);
        e->slot = slot;
        e->empty = 1;
        if (!lr_find_save_folder(remote_dir, slot, folder, sizeof folder)) continue;
        lr_join(dir, sizeof dir, remote_dir, folder);
        lr_join(path, sizeof path, dir, "persist.game.json");
        if (!lr_file_exists(path)) continue;
        e->empty = 0;
        if (!lr_read_game_info(remote_dir, folder, &game)) {
            e->unreadable = 1;
            continue;
        }
        e->tutorial = game.text || game.tutorial;
        snprintf(e->estate, sizeof e->estate, "%s", game.estate);
        n++;
    }
    return n;
}

int lr_read_game_info(const char *remote_dir, const char *folder, lr_game_info *out) {
    char dir[LR_PATH_MAX], path[LR_PATH_MAX];
    uint8_t *data;
    size_t len;
    lr_join(dir, sizeof dir, remote_dir, folder);
    lr_join(path, sizeof path, dir, "persist.game.json");
    if (!lr_read_file(path, LR_MAX_FILE_SIZE, &data, &len)) return 0;
    int ok = lr_game_parse(data, len, out);
    free(data);
    return ok;
}

/* The campaign's persist.game.json from c->files; 0 if it is not there or not readable. */
static int files_game_info(lr_core *c, lr_game_info *out) {
    for (uint32_t i = 0; i < c->file_count; i++)
        if (strcmp(c->files[i].name, "persist.game.json") == 0) return lr_game_parse(c->files[i].data, c->files[i].size, out);
    return 0;
}

static void log_dlc(lr_core *c, const lr_game_info *g) {
    char list[256];
    size_t k = 0;
    list[0] = 0;
    for (int i = 0; i < g->dlc_count && k + LR_DLC_NAME_MAX + 2 < sizeof list; i++)
        k += (size_t)snprintf(list + k, sizeof list - k, " %s", g->dlc[i]);
    logf_(c, "campaign: %s%s, DLC:%s", g->text ? "text template" : "saved by the game",
          g->tutorial ? ", in the tutorial" : "", g->dlc_count ? list : " none");
}

static void make_password(lr_core *c) {
    static const char alphabet[] = "abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    uint8_t raw[LR_PASSWORD_LEN];
    c->cfg.random(c->cfg.ctx, raw, sizeof raw);
    for (int i = 0; i < LR_PASSWORD_LEN; i++) c->password[i] = alphabet[raw[i] % (sizeof alphabet - 1)];
    c->password[LR_PASSWORD_LEN] = 0;
}

typedef struct {
    lr_core *c;
    const char *dir;
    int ok;
} load_ctx;

static int load_one(void *p, const char *name) {
    load_ctx *l = p;
    lr_core *c = l->c;
    if (strstr(name, ".ddcoop-tmp")) return 1;
    if (!lr_file_name_ok(name) || c->file_count >= LR_MAX_FILES) {
        logf_(c, "save file skipped or too many: %s", name);
        l->ok = 0;
        return 0;
    }
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, l->dir, name);
    lr_file *f = &c->files[c->file_count];
    size_t len;
    if (!lr_read_file(path, LR_MAX_FILE_SIZE, &f->data, &len) || c->total + len > LR_MAX_SAVE_TOTAL) {
        logf_(c, "save file unreadable or too large: %s", name);
        l->ok = 0;
        return 0;
    }
    snprintf(f->name, sizeof f->name, "%s", name);
    f->size = (uint32_t)len;
    f->crc = lr_crc32(0, f->data, len);
    c->total += f->size;
    c->file_count++;
    logf_(c, "campaign file %s: %u bytes crc=%08x", f->name, f->size, f->crc);
    return 1;
}

static int load_campaign(lr_core *c) {
    files_free(c);
    char dir[LR_PATH_MAX];
    lr_join(dir, sizeof dir, c->cfg.remote_dir, c->save_folder);
    load_ctx l = {c, dir, 1};
    if (lr_list_files(dir, load_one, &l) < 0 || !l.ok || c->file_count == 0) {
        files_free(c);
        return 0;
    }
    logf_(c, "campaign %s: %u files, %u bytes", c->save_folder, c->file_count, c->total);
    return 1;
}

typedef struct {
    lr_core *c;
    char names[LR_MAX_FILES][LR_NAME_MAX + 1];
    int n;
} extra_files;

/* Names in the campaign folder that are not in c->files. */
static int collect_extra(void *p, const char *name) {
    extra_files *e = p;
    for (uint32_t i = 0; i < e->c->file_count; i++)
        if (strcmp(e->c->files[i].name, name) == 0) return 1;
    if (e->n < LR_MAX_FILES && strlen(name) <= LR_NAME_MAX) snprintf(e->names[e->n++], LR_NAME_MAX + 1, "%s", name);
    return 1;
}

typedef struct {
    lr_core *c;
    const char *source_dir;
    const char *backup_dir;
    int ok;
    int count;
} backup_check;

/* A guest campaign must be preserved byte-for-byte before it can be replaced. */
static int backup_check_one(void *p, const char *name) {
    backup_check *b = p;
    char source_path[LR_PATH_MAX], backup_path[LR_PATH_MAX];
    uint8_t *source = NULL, *backup = NULL;
    size_t source_len = 0, backup_len = 0;
    if (!lr_file_name_ok(name)) {
        b->ok = 0;
        return 0;
    }
    lr_join(source_path, sizeof source_path, b->source_dir, name);
    lr_join(backup_path, sizeof backup_path, b->backup_dir, name);
    if (!lr_read_file(source_path, LR_MAX_FILE_SIZE, &source, &source_len) ||
        !lr_read_file(backup_path, LR_MAX_FILE_SIZE, &backup, &backup_len) ||
        source_len != backup_len || memcmp(source, backup, source_len) != 0) {
        logf_(b->c, "backup verification failed for %s", name);
        b->ok = 0;
    } else {
        logf_(b->c, "backup verified: %s: %u bytes crc=%08x", name,
              (unsigned)source_len, lr_crc32(0, source, source_len));
        b->count++;
    }
    free(source);
    free(backup);
    return b->ok;
}

static int count_backup_file(void *p, const char *name) {
    int *count = p;
    (void)name;
    (*count)++;
    return 1;
}

static int backup_matches(lr_core *c, const char *source_dir, const char *backup_dir) {
    backup_check b = {c, source_dir, backup_dir, 1, 0};
    int source_count = lr_list_files(source_dir, backup_check_one, &b);
    int backup_count = 0;
    if (lr_list_files(backup_dir, count_backup_file, &backup_count) < 0) backup_count = -1;
    if (source_count < 0 || !b.ok || backup_count < 0 || source_count != b.count || source_count != backup_count) {
        logf_(c, "backup verification failed: source=%d verified=%d backup=%d",
              source_count, b.count, backup_count);
        return 0;
    }
    logf_(c, "backup verified: %d files", b.count);
    return 1;
}

/* Guest: back up our copy of the co-op campaign, then replace it with the host's. */
static int store_campaign(lr_core *c) {
    char dir[LR_PATH_MAX];
    if (c->cfg.restore_path[0] && lr_file_exists(c->cfg.restore_path)) {
        logf_(c, "guest restore is still pending; refusing to overwrite its original backup");
        return 0;
    }
    c->backup_path[0] = 0;
    lr_join(dir, sizeof dir, c->cfg.remote_dir, c->save_folder);
    if (lr_dir_exists(dir)) {
        char stamp[64], parent[LR_PATH_MAX];
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        strftime(stamp, sizeof stamp, "%Y-%m-%d_%H%M%S", tm);
        uint8_t unique[8];
        c->cfg.random(c->cfg.ctx, unique, sizeof unique);
        size_t stamp_len = strlen(stamp);
        for (size_t j = 0; j < sizeof unique; j++)
            snprintf(stamp + stamp_len + j * 2, sizeof stamp - stamp_len - j * 2, "%02x", unique[j]);
        lr_join(parent, sizeof parent, c->cfg.backup_root, stamp);
        lr_join(c->backup_path, sizeof c->backup_path, parent, c->save_folder);
        if (!lr_copy_dir_files(dir, c->backup_path) || !backup_matches(c, dir, c->backup_path)) {
            logf_(c, "backup failed verification; refusing to replace the guest save: %s", c->backup_path);
            return 0;
        }
        if (c->cfg.restore_path[0]) {
            char sub[LR_PATH_MAX], copy[LR_PATH_MAX];
            lr_join(sub, sizeof sub, dir, "backup");
            lr_join(copy, sizeof copy, c->backup_path, "backup");
            if (lr_dir_exists(sub) && (!lr_copy_dir_files(sub, copy) || !backup_matches(c, sub, copy))) return 0;
        }
        logf_(c, "backup of own %s: %s", c->save_folder, c->backup_path);
    } else if (!lr_make_dirs(dir)) {
        return 0;
    }
    /* Persist recovery before the first replacement. A launcher crash, game crash or failed load
     * must not turn the received campaign into the guest's permanent original. */
    if (c->cfg.restore_path[0]) {
        lr_snapshot *original = calloc(1, sizeof *original);
        if (!original) return 0;
        int ok = lr_snapshot_from_dir(original, c->cfg.steam_user, c->backup_path[0] ? c->backup_path : NULL);
        snprintf(original->folder, sizeof original->folder, "%s", c->save_folder);
        ok = ok && lr_snapshot_write(c->cfg.restore_path, original);
        lr_snapshot_free(original);
        free(original);
        if (!ok) {
            lr_delete_file(c->cfg.restore_path); /* no guest file has been replaced yet */
            logf_(c, "could not persist verified guest recovery; refusing to replace the guest save");
            return 0;
        }
        logf_(c, "guest recovery saved and verified before campaign replacement");
        logf_(c, "guest original left intact; campaign will be imported through Steam before game launch");
        return 1;
    }
    for (uint32_t i = 0; i < c->file_count; i++) {
        char path[LR_PATH_MAX];
        lr_join(path, sizeof path, dir, c->files[i].name);
        if (!lr_write_file(path, c->files[i].data, c->files[i].size)) {
            logf_(c, "write failed: %s", path);
            return 0;
        }
        uint8_t *written = NULL;
        size_t written_len = 0;
        int verified = lr_read_file(path, LR_MAX_FILE_SIZE, &written, &written_len) &&
                       written_len == c->files[i].size &&
                       lr_crc32(0, written, written_len) == c->files[i].crc;
        free(written);
        if (!verified) {
            logf_(c, "campaign file read-back failed: %s", c->files[i].name);
            return 0;
        }
        logf_(c, "campaign file persisted: %s: %u bytes crc=%08x", c->files[i].name,
              c->files[i].size, c->files[i].crc);
    }
    /* Files the host does not have (they are in the backup) would mix two campaigns: remove them. */
    extra_files *extra = calloc(1, sizeof *extra);
    if (!extra) return 0;
    extra->c = c;
    lr_list_files(dir, collect_extra, extra);
    for (int i = 0; i < extra->n; i++) {
        char path[LR_PATH_MAX];
        lr_join(path, sizeof path, dir, extra->names[i]);
        lr_delete_file(path);
        logf_(c, "removed own file not in the host's campaign: %s", extra->names[i]);
    }
    free(extra);
    return 1;
}

/* ---- host ---- */

#define LR_HOST_RESTART_TEXT "Your friend left. Close the game and click Host again to start a new session."
void lr_core_host_spent(lr_core *c) {
    if (c->role != LR_ROLE_HOST || c->host_spent) return;
    c->host_spent = 1;
    logf_(c, "host: the game's session ended; no new guest until the game restarts");
    if (c->state == LR_ST_HOST_WAITING) set_status(c, LR_HOST_RESTART_TEXT);
}

int lr_core_host_start(lr_core *c, uint64_t now_ms) {
    lr_core_free(c);
    c->host_spent = 0;
    c->role = LR_ROLE_HOST;
    c->peer = 0;
    c->error[0] = 0;
    c->password[0] = 0;
    c->resend_len = 0;
    if (!lr_find_save_folder(c->cfg.remote_dir, c->cfg.coop_slot, c->save_folder, sizeof c->save_folder)) {
        char msg[256];
        snprintf(msg, sizeof msg,
                 "No campaign found in slot %d. Start Darkest Dungeon through Steam, create or load a "
                 "campaign in slot %d, close the game and try again.",
                 c->cfg.coop_slot, c->cfg.coop_slot);
        fail(c, msg);
        return 0;
    }
    /* Freeze the host's campaign before its game starts. Never read a moving Steam save during a join. */
    if (!load_campaign(c)) {
        fail(c, "Could not safely read the campaign; nothing was sent (see DDCoop-launcher.log).");
        return 0;
    }
    lr_game_info game;
    if (!files_game_info(c, &game)) {
        logf_(c, "campaign: persist.game.json missing or unreadable; tutorial and DLC not checked");
    } else {
        log_dlc(c, &game);
        if (game.text || game.tutorial) {
            char msg[sizeof c->error];
            snprintf(msg, sizeof msg, LR_TUTORIAL_TEXT, c->cfg.coop_slot);
            files_free(c);
            fail(c, msg);
            return 0;
        }
    }
    lr_owners_init(&c->owners);
    c->owners_sent = 0;
    for (uint32_t i = 0; i < c->file_count; i++)
        if (strcmp(c->files[i].name, "persist.roster.json") == 0) host_roster(c, c->files[i].data, c->files[i].size, 1);
    c->game_port = LR_DEFAULT_GAME_PORT;
    while (!lr_port_free(c->game_port) && c->game_port < LR_DEFAULT_GAME_PORT + 50) c->game_port++;
    make_password(c);
    if (!lr_write_ini(c->cfg.ini_path, "host", c->game_port, c->password, c->cfg.coop_slot)) {
        fail(c, "Could not write ddcoop.ini. Check folder permissions or run Repair DDCoop.bat.");
        return 0;
    }
    if (!lr_relay_init(&c->relay, LR_ROLE_HOST, c->game_port, relay_send, c)) {
        fail(c, "Could not start the relay.");
        return 0;
    }
    c->relay_up = 1;
    c->state = LR_ST_HOST_WAITING;
    c->last_rx = c->last_tx = now_ms;
    logf_(c, "host: port %d, campaign folder %s", c->game_port, c->save_folder);
    set_status(c, "Game starting; waiting for a friend to join.");
    return 1;
}

static void host_accept(lr_core *c, uint64_t peer, const lr_msg *hello, uint64_t now_ms) {
    if (c->host_spent && c->peer != peer) {
        logf_(c, "friend refused: the game must restart first");
        send_simple_to(c, peer, LR_BYE, LR_BYE_RESTART);
        return;
    }
    if (hello->b != c->cfg.build) {
        send_simple_to(c, peer, LR_BYE, LR_BYE_BUILD);
        return;
    }
    if (hello->id != c->cfg.kit) {
        logf_(c, "friend runs another DDCoop kit (%08x, ours %08x): refused", hello->id, c->cfg.kit);
        send_simple_to(c, peer, LR_BYE, LR_BYE_VERSION);
        return;
    }
    if (c->peer != 0 && c->peer != peer) {
        send_simple_to(c, peer, LR_BYE, LR_BYE_FULL);
        return;
    }
    if (c->peer == peer && c->state == LR_ST_HOST_SENDING) return; /* a repeated HELLO; already answering */
    if (c->peer == peer && c->state == LR_ST_HOST_PLAYING) {
        logf_(c, "friend said HELLO again; resending the session");
        lr_relay_drop_peer(&c->relay, peer);
    }
    c->peer = peer;
    c->last_rx = now_ms;
    c->owners_sent = 0;
    if (c->file_count == 0) {
        send_simple_to(c, peer, LR_BYE, LR_BYE_SAVE);
        c->peer = 0;
        set_status(c, "The campaign snapshot is missing; friend refused (see DDCoop-launcher.log).");
        return;
    }
    c->send_phase = SP_HELLO;
    c->state = LR_ST_HOST_SENDING;
    set_status(c, "Friend found; sending campaign.");
}

/* Sends HELLO, SESSION and the campaign, as much as the transport takes right now. */
static void host_send_campaign(lr_core *c) {
    for (int budget = 0; budget < 64 && c->send_phase < SP_DONE; budget++) {
        lr_msg m = {0};
        if (c->send_phase == SP_HELLO) {
            m.type = LR_HELLO;
            m.a = LR_ROLE_HOST;
            m.b = c->cfg.build;
            m.id = c->cfg.kit;
        } else if (c->send_phase == SP_SESSION) {
            m.type = LR_SESSION;
            m.a = (uint32_t)c->cfg.coop_slot;
            snprintf(m.text, sizeof m.text, "%s", c->password);
        } else if (c->send_phase == SP_BEGIN) {
            m.type = LR_SAVE_BEGIN;
            m.a = c->file_count;
            m.b = c->total;
            snprintf(m.text, sizeof m.text, "%s", c->save_folder);
        } else if (c->send_phase == SP_FILES) {
            lr_file *f = &c->files[c->sent_file];
            m.type = LR_SAVE_FILE;
            m.id = c->sent_file;
            m.a = f->size;
            m.b = f->crc;
            snprintf(m.text, sizeof m.text, "%s", f->name);
        } else if (c->send_phase == SP_CHUNKS) {
            while (c->sent_file < c->file_count && c->sent_offset >= c->files[c->sent_file].size) {
                c->sent_file++;
                c->sent_offset = 0;
            }
            if (c->sent_file >= c->file_count) {
                c->send_phase = SP_END;
                continue;
            }
            lr_file *f = &c->files[c->sent_file];
            m.type = LR_SAVE_CHUNK;
            m.id = c->sent_file;
            m.a = c->sent_offset;
            m.data = f->data + c->sent_offset;
            m.len = f->size - c->sent_offset < LR_CHUNK ? f->size - c->sent_offset : LR_CHUNK;
        } else {
            m.type = LR_SAVE_END;
        }
        int r = send_msg(c, &m);
        if (r == 0) return; /* full: continue next tick */
        if (r < 0) {
            set_status(c, "Friend became unreachable during transfer.");
            return;
        }
        if (c->send_phase <= SP_BEGIN) {
            c->send_phase++;
            c->sent_file = 0;
        } else if (c->send_phase == SP_FILES) {
            if (++c->sent_file >= c->file_count) {
                c->send_phase = SP_CHUNKS;
                c->sent_file = 0;
                c->sent_offset = 0;
            }
        } else if (c->send_phase == SP_CHUNKS) {
            c->sent_offset += (uint32_t)m.len;
            c->sent_bytes += (uint32_t)m.len;
        } else {
            c->send_phase = SP_DONE;
        }
    }
}

/* ---- guest ---- */

int lr_core_guest_join(lr_core *c, uint64_t host_peer, uint64_t now_ms) {
    lr_core_free(c);
    c->role = LR_ROLE_GUEST;
    c->error[0] = 0;
    c->password[0] = 0; /* nothing from an earlier session may carry over */
    c->save_folder[0] = 0;
    c->backup_path[0] = 0;
    c->resend_len = 0;
    c->peer = host_peer;
    lr_owners_init(&c->owners);
    c->claim_count = 0;
    if (c->cfg.game_running && c->cfg.game_running(c->cfg.ctx)) {
        fail(c, lr_bye_text(LR_BYE_GAME_RUNNING));
        return 0;
    }
    if (!lr_relay_init(&c->relay, LR_ROLE_GUEST, 0, relay_send, c)) {
        fail(c, "Could not open a local game port.");
        return 0;
    }
    c->relay_up = 1;
    c->relay.host_peer = host_peer;
    c->game_port = c->relay.port;
    send_hello(c, now_ms);
    c->state = LR_ST_GUEST_HELLO;
    c->last_rx = c->last_tx = now_ms;
    logf_(c, "guest: relay port %d, HELLO sent", c->game_port);
    set_status(c, "Connecting to host...");
    return 1;
}

/* Guest: the game calls a campaign with DLC we lack "corrupted" and may start a new one in its place, so
 * check before our own save is touched. Fails the session (with the missing DLC by name) when one is missing. */
static int guest_dlc_ok(lr_core *c) {
    lr_game_info game;
    if (!files_game_info(c, &game)) {
        logf_(c, "campaign: persist.game.json missing or unreadable; DLC not checked");
        return 1;
    }
    log_dlc(c, &game);
    if (!c->cfg.has_dlc) return 1;
    char missing[100];
    size_t k = 0;
    missing[0] = 0;
    for (int i = 0; i < game.dlc_count; i++) {
        int have = c->cfg.has_dlc(c->cfg.ctx, game.dlc[i]);
        logf_(c, "DLC %s: %s", game.dlc[i], have ? "installed" : "MISSING");
        const char *title = lr_dlc_title(game.dlc[i]);
        if (have || strstr(missing, title)) continue; /* flagellant and districts are both The Crimson Court */
        if (k + strlen(title) + 12 < sizeof missing)
            k += (size_t)snprintf(missing + k, sizeof missing - k, "%s%s%s", k ? ", " : "", title,
                                  strcmp(game.dlc[i], "musketeer") == 0 ? " (gratis)" : "");
        else if (k + 6 < sizeof missing && !strstr(missing, "..."))
            k += (size_t)snprintf(missing + k, sizeof missing - k, ", ...");
    }
    if (!k) return 1;
    char msg[sizeof c->error];
    snprintf(msg, sizeof msg,
             "The host campaign requires DLC you do not have: %s. Install it through Steam and join again; "
             "your own save has not been changed.",
             missing);
    fail(c, msg);
    return 0;
}

static void guest_save_message(lr_core *c, const lr_msg *m) {
    if (m->type == LR_SAVE_BEGIN) {
        files_free(c);
        /* This is what gets replaced on this PC, so it is checked here: only the slot the host announced, and never the game's restore area. */
        int slot = 0;
        if (!lr_slot_folder_ok(m->text, &slot) || slot != c->cfg.coop_slot) {
            send_simple_to(c, c->peer, LR_SAVE_RESULT, LR_BYE_PROTOCOL);
            fail(c, "The host sent an unexpected save folder.");
            return;
        }
        snprintf(c->save_folder, sizeof c->save_folder, "%s", m->text);
        c->file_count = m->a;
        c->total = m->b;
        c->state = LR_ST_GUEST_RECEIVING;
        set_status(c, "Receiving host campaign...");
        return;
    }
    if (c->state != LR_ST_GUEST_RECEIVING) return;
    if (m->type == LR_SAVE_FILE) {
        if (m->id >= c->file_count) return;
        lr_file *f = &c->files[m->id];
        free(f->data);
        f->data = malloc(m->a ? m->a : 1);
        if (!f->data) {
            fail(c, "Not enough memory for the campaign.");
            return;
        }
        snprintf(f->name, sizeof f->name, "%s", m->text);
        f->size = m->a;
        f->crc = m->b;
        f->received = 0;
        return;
    }
    if (m->type == LR_SAVE_CHUNK) {
        if (m->id >= c->file_count) return;
        lr_file *f = &c->files[m->id];
        if (!f->data || m->a != f->received || m->a + m->len > f->size) {
            send_simple_to(c, c->peer, LR_SAVE_RESULT, LR_BYE_SAVE);
            fail(c, "The campaign arrived corrupted; try joining again.");
            return;
        }
        memcpy(f->data + m->a, m->data, m->len);
        f->received += (uint32_t)m->len;
        c->recv_bytes += (uint32_t)m->len;
        return;
    }
    if (m->type == LR_SAVE_END) {
        uint32_t sum = 0;
        for (uint32_t i = 0; i < c->file_count; i++) {
            lr_file *f = &c->files[i];
            if (!f->name[0] || f->received != f->size || lr_crc32(0, f->data, f->size) != f->crc) {
                send_simple_to(c, c->peer, LR_SAVE_RESULT, LR_BYE_SAVE);
                fail(c, "The campaign transfer was incomplete; try joining again.");
                return;
            }
            logf_(c, "campaign file received: %s: %u bytes crc=%08x (verified)", f->name, f->size, f->crc);
            sum += f->size;
        }
        if (sum != c->total) {
            send_simple_to(c, c->peer, LR_SAVE_RESULT, LR_BYE_SAVE);
            fail(c, "The campaign transfer was incomplete; try joining again.");
            return;
        }
        if (c->cfg.game_running && c->cfg.game_running(c->cfg.ctx)) {
            send_simple_to(c, c->peer, LR_SAVE_RESULT, LR_BYE_GAME_RUNNING);
            fail(c, lr_bye_text(LR_BYE_GAME_RUNNING));
            return;
        }
        if (!guest_dlc_ok(c)) {
            send_simple_to(c, c->peer, LR_SAVE_RESULT, LR_BYE_DLC);
            return;
        }
        if (!store_campaign(c)) {
            send_simple_to(c, c->peer, LR_SAVE_RESULT, LR_BYE_SAVE);
            fail(c, "Could not save the campaign (see DDCoop-launcher.log); your own save has not been changed. "
                    "of staat in DDCoop-backups");
            return;
        }
        if (!lr_write_ini(c->cfg.ini_path, "guest", c->game_port, c->password, c->cfg.coop_slot)) {
            send_simple_to(c, c->peer, LR_SAVE_RESULT, LR_BYE_SAVE);
            fail(c, "Could not write ddcoop.ini (run Repair DDCoop.bat).");
            return;
        }
        send_simple_to(c, c->peer, LR_SAVE_RESULT, 0);
        c->state = LR_ST_GUEST_LAUNCH;
        {
            char msg[160];
            snprintf(msg, sizeof msg, "Campaign ready; the game will open slot %d automatically. Your own slot %d will be restored afterwards.", c->cfg.coop_slot, c->cfg.coop_slot);
            set_status(c, msg);
        }
    }
}

int lr_core_take_launch(lr_core *c) {
    if (c->state != LR_ST_GUEST_LAUNCH) return 0;
    c->state = LR_ST_GUEST_PLAYING;
    return 1;
}

/* ---- both ---- */

void lr_core_on_message(lr_core *c, uint64_t peer, int channel, const uint8_t *buf, size_t len, uint64_t now_ms) {
    lr_msg m;
    if (!lr_decode(buf, len, channel, &m)) {
        logf_(c, "refused a malformed message (%zu bytes, channel %d)", len, channel);
        return;
    }
    if (c->role == LR_ROLE_HOST) {
        if (c->state == LR_ST_IDLE || c->state == LR_ST_FAILED) return;
        if (c->cfg.allowed && !c->cfg.allowed(c->cfg.ctx, peer)) {
            logf_(c, "message from someone outside the lobby dropped");
            return;
        }
        if (m.type == LR_HELLO) {
            if (m.a == LR_ROLE_GUEST) host_accept(c, peer, &m, now_ms);
            return;
        }
        if (peer != c->peer) {
            if (m.type != LR_BYE && m.type != LR_KEEPALIVE) send_simple_to(c, peer, LR_BYE, LR_BYE_FULL);
            return;
        }
        c->last_rx = now_ms;
        if (channel == LR_CH_STREAM) {
            if (c->state == LR_ST_HOST_PLAYING || c->state == LR_ST_HOST_SENDING) lr_relay_on_message(&c->relay, peer, &m, now_ms);
            return;
        }
        if (m.type == LR_SAVE_RESULT) {
            if (m.a == 0) {
                c->state = LR_ST_HOST_PLAYING;
                char msg[120];
                snprintf(msg, sizeof msg, "Your friend has the campaign. Both games will open slot %d automatically.", c->cfg.coop_slot);
                set_status(c, msg);
            } else {
                set_status(c, lr_bye_text(m.a));
                c->peer = 0;
                c->state = LR_ST_HOST_WAITING;
            }
        } else if (m.type == LR_CLAIM) {
            if (c->state == LR_ST_HOST_PLAYING && lr_owners_claim(&c->owners, m.id, LR_PLAYER_GUEST, (int)m.a)) {
                owners_save(c);
                owners_log(c, "friend ticked");
            } else {
                c->owners_sent = 0; /* refused: the friend's list goes back to the table */
            }
        } else if (m.type == LR_BYE) {
            char msg[256];
            snprintf(msg, sizeof msg, c->host_spent ? "Friend left: %s. Close the game and click Host again." :
                     "Friend left: %s; waiting for another friend.", lr_bye_text(m.a));
            lr_relay_drop_peer(&c->relay, peer);
            c->peer = 0;
            c->state = LR_ST_HOST_WAITING;
            set_status(c, msg);
        }
        return;
    }
    if (c->role == LR_ROLE_GUEST) {
        if (peer != c->peer || c->state == LR_ST_IDLE || c->state == LR_ST_FAILED) return;
        c->last_rx = now_ms;
        if (channel == LR_CH_STREAM) {
            lr_relay_on_message(&c->relay, peer, &m, now_ms);
            return;
        }
        switch (m.type) {
        case LR_HELLO:
            if (m.a != LR_ROLE_HOST) return;
            if (m.b != c->cfg.build) fail(c, lr_bye_text(LR_BYE_BUILD));
            else if (m.id != c->cfg.kit) fail(c, lr_bye_text(LR_BYE_VERSION));
            break;
        case LR_SESSION:
            memcpy(c->password, m.text, LR_PASSWORD_LEN); /* the decoder checked: exactly 32 letters/digits */
            c->password[LR_PASSWORD_LEN] = 0;
            c->cfg.coop_slot = (int)m.a; /* The decoder checked: 1..9. */
            break;
        case LR_BYE: {
            char msg[256];
            snprintf(msg, sizeof msg, "The host closed the connection: %s", lr_bye_text(m.a));
            fail(c, msg);
            break;
        }
        case LR_ROSTER:
            if (!c->password[0]) return;
            if (lr_owners_decode(m.data, m.len, m.id, &c->owners)) {
                owners_save(c);
                owners_log(c, "from host");
            } else {
                logf_(c, "refused a malformed hero table (%zu bytes)", m.len);
            }
            break;
        case LR_SAVE_BEGIN:
        case LR_SAVE_FILE:
        case LR_SAVE_CHUNK:
        case LR_SAVE_END:
            if (!c->password[0]) {
                fail(c, "The host sent a campaign without a session.");
                return;
            }
            if (c->state == LR_ST_GUEST_HELLO || c->state == LR_ST_GUEST_RECEIVING) guest_save_message(c, &m);
            break;
        default:
            break;
        }
    }
}

void lr_core_peer_left(lr_core *c, uint64_t peer) {
    if (peer == 0 || peer != c->peer) return;
    if (c->relay_up) lr_relay_drop_peer(&c->relay, peer);
    if (c->role == LR_ROLE_HOST) {
        c->peer = 0;
        c->state = LR_ST_HOST_WAITING;
        set_status(c, c->host_spent ? LR_HOST_RESTART_TEXT : "Friend left the session; waiting for another friend.");
    } else if (c->state != LR_ST_FAILED) {
        fail(c, "The host left the session.");
    }
}

void lr_core_tick(lr_core *c, uint64_t now_ms) {
    flush_resend(c);
    if (c->state == LR_ST_IDLE || c->state == LR_ST_FAILED) return;
    if (c->state == LR_ST_GUEST_HELLO && !c->password[0] && now_ms >= c->hello_at &&
        now_ms - c->hello_at >= LR_HELLO_REPEAT_MS)
        send_hello(c, now_ms);
    if (c->peer && now_ms >= c->last_tx && now_ms - c->last_tx >= LR_KEEPALIVE_MS) {
        lr_msg m = {0};
        m.type = LR_KEEPALIVE;
        send_msg(c, &m);
        c->last_tx = now_ms;
    }
    if (c->peer && now_ms >= c->last_rx && now_ms - c->last_rx > LR_PEER_TIMEOUT_MS) {
        logf_(c, "peer silent for %d s", LR_PEER_TIMEOUT_MS / 1000);
        lr_core_peer_left(c, c->peer);
        if (c->state == LR_ST_FAILED) snprintf(c->error, sizeof c->error, "Lost contact with the host (20 s).");
        return;
    }
    if (c->state == LR_ST_HOST_SENDING) host_send_campaign(c);
    if (c->role == LR_ROLE_HOST) host_send_owners(c);
    if (c->role == LR_ROLE_GUEST && c->claim_count) guest_send_claims(c);
    if (c->relay_up) lr_relay_tick(&c->relay, now_ms);
}

void lr_core_stop(lr_core *c) {
    if (c->peer && c->state != LR_ST_IDLE) send_simple_to(c, c->peer, LR_BYE, LR_BYE_QUIT);
    if (c->cfg.ini_path[0]) lr_write_ini(c->cfg.ini_path, "off", LR_DEFAULT_GAME_PORT, "unused-until-ddcoop-starts", c->cfg.coop_slot);
    lr_core_free(c);
    c->peer = 0;
    c->state = LR_ST_IDLE;
    c->password[0] = 0;
    lr_owners_init(&c->owners); /* ddcoop-heroes.ini stays: the host restores its choices next session */
    c->owners_sent = 0;
    c->claim_count = 0;
    set_status(c, "stopped");
}
