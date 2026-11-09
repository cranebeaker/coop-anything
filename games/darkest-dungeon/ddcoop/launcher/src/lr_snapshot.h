/* Immutable snapshots of one campaign slot and transactional preparation through a storage adapter.
 * This does not validate the game's save format. It verifies the exact bytes exposed by storage. */
#ifndef LR_SNAPSHOT_H
#define LR_SNAPSHOT_H
#include "lr_files.h"
#include "lr_wire.h"
#define LR_SNAPSHOT_NAME_MAX (LR_NAME_MAX + 7) /* optional backup/ prefix, no other nesting */

typedef struct {
    char name[LR_SNAPSHOT_NAME_MAX + 1];
    uint32_t size, crc;
    uint8_t *data;
} lr_snapshot_file;
typedef struct {
    uint64_t user;
    uint32_t count, total;
    char folder[LR_NAME_MAX + 1]; /* The one campaign folder this belongs to; lr_slot_folder_ok. */
    lr_snapshot_file files[LR_MAX_FILES];
} lr_snapshot;

/* profile_0..8 or steam_profile_0..8: menu slots 1..9. Never profile_9, the game's restore area. Fills *slot if given. */
int lr_slot_folder_ok(const char *folder, int *slot);

void lr_snapshot_free(lr_snapshot *s);
int lr_snapshot_name_ok(const char *name); /* root basename or backup/basename, within its one slot */
int lr_snapshot_add(lr_snapshot *s, const char *name, const uint8_t *data, size_t len);
int lr_snapshot_from_dir(lr_snapshot *s, uint64_t user, const char *dir); /* NULL = empty original slot */
int lr_snapshot_write(const char *path, const lr_snapshot *s);
int lr_snapshot_read(const char *path, lr_snapshot *s);

typedef struct {
    void *ctx;
    int (*list)(void *ctx, int (*visit)(void *, const char *), void *visit_ctx);
    int (*read)(void *ctx, const char *name, uint8_t **out, size_t *len);
    int (*write)(void *ctx, const char *name, const uint8_t *data, size_t len);
    int (*remove)(void *ctx, const char *name);
    int (*backup)(void *ctx, const lr_snapshot *original);
    void (*log)(void *ctx, const char *line);
} lr_snapshot_io;

/* Read all target files first. Save and verify a recovery copy before any mutation. Import,
 * remove stale root files and compare the entire result byte for byte. On error, roll back
 * and verify the original. 1=verified desired bytes, 0=failed (details in log). */
int lr_snapshot_apply(const lr_snapshot *want, const lr_snapshot_io *io);
/* The same; *unread (may be NULL) says the slot could not be read at the start, so nothing was written. Right after the
 * game wrote a save Steam may not serve it yet (owner's run of 7 Nov 17:56: a new campaign, readable a minute later). */
int lr_snapshot_apply_ex(const lr_snapshot *want, const lr_snapshot_io *io, int *unread);
#endif
