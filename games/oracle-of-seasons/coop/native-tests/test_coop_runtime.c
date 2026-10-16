// Isolation gate only: four original single-Link simulations, not multiplayer.
#include "unit.h"
#include "core/gb.h"
#include "core/hash.h"
#include "platform/setup.h"
#include "platform/tas.h"
#include "assets/assets.h"
#include "game/game.h"
#include "rt/fibers.h"
#include <inttypes.h>

// Matching console-verified boot + movie: after 9500 complete steps, frame 9688
// is controllable overworld room 00/c4, Link (72,79), after the opening scenes.
#define BOOTSTRAP_FRAMES 9500
#define BRANCH_FRAMES 30
#define TOTAL_FRAMES (BOOTSTRAP_FRAMES + BRANCH_FRAMES)

typedef struct { const Tas *movie; uint64_t branch; uint8_t buttons; } Input;
typedef struct { uint64_t state, pixels, render; } Frame;

static uint8_t input_at(void *ctx, uint64_t frame) {
  const Input *in = ctx;
  return frame < in->branch ? tas_input_at(in->movie, frame) : in->buttons;
}

// Compare all sampled gameplay RAM (upstream hash deliberately excludes audio scratch),
// the actual picture, and the per-GB renderer history used by widescreen consumers.
static Frame signature(GB *gb) {
  const GBSample *s = gb->sample;
  uint64_t h = FNV1A64_INIT;
#define ADD(field) h = fnv1a64_update(h, (const uint8_t *)s->field, sizeof s->field)
  ADD(wram); ADD(hram); ADD(line_scx); ADD(line_scy); ADD(line_lcdc);
  ADD(line_wx); ADD(line_wy); ADD(drawn_vram); ADD(drawn_bg_pal);
  ADD(drawn_oam); ADD(line_drawn); ADD(drawn_offscreen);
#undef ADD
  return (Frame){gb_state_hash(gb), gb_frame_hash(gb), h};
}

static void compare(GB *gb, Frame want, unsigned stream, unsigned frame) {
  Frame got = signature(gb);
  if (got.state != want.state || got.pixels != want.pixels || got.render != want.render) {
    fprintf(stderr, "isolation FAIL stream=%u frame=%u state=%016" PRIx64 "/%016" PRIx64
            " pixels=%016" PRIx64 "/%016" PRIx64 " sampled=%016" PRIx64 "/%016" PRIx64 "\n",
            stream, frame, got.state, want.state, got.pixels, want.pixels, got.render, want.render);
    exit(1);
  }
}

static GB *new_runtime(Input *input) {
  size_t n;
  uint8_t *rom = oracles_read_file(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Seasons (USA, Australia).gbc", &n);
  if (!rom) SKIP("Seasons ROM not present");
  GB *gb = calloc(1, sizeof *gb);
  ASSERT(gb);
  gb_init(gb);
  ASSERT(gb_load_rom(gb, rom, n));
  gb->cyctab = cyctab_alloc(rom, n);
  gb->code_bits = assets_code_bits(rom, n);
  assets_zero_code(rom, n);
  gb_reset(gb);
  ASSERT(oracles_load_boot_state(gb, TAS_DIR "/seasons-consoleverified-boot.state"));
  gb->input_at = input_at;
  gb->input_ctx = input;
  return gb;
}

static void step(GB *gb) {
  gb_run_frame(gb);
  if (gb->hung) { fprintf(stderr, "native stopped: %s\n", gb_stop_reason); exit(1); }
}

static void describe(const char *name, GB *gb) {
  const GBSample *s = gb->sample;
  fprintf(stdout, "%s frame=%" PRIu64 " group=%02x room=%02x modifier=%02x Link=(%u,%u) enabled=%02x state=%02x split=%02x fade=%02x text=%02x parked=%d hash=%016" PRIx64 "\n",
          name, s->frame, s->wram[0][0xc49], s->wram[0][0xc4c], s->wram[0][0xc4e],
          s->wram[1][0x00d], s->wram[1][0x00b], s->wram[1][0], s->wram[1][4],
          s->hram[0x19], s->wram[0][0x4ab], s->wram[0][0xba0], threads_parked(gb), gb_state_hash(gb));
  fflush(stdout);
}

static void assert_gameplay(GB *gb) {
  const GBSample *s = gb->sample;
  ASSERT((s->wram[0][0xd00] & 0x0f) == 1); // main gameplay, as in test_room
  ASSERT(s->hram[0x19] == 2 || s->hram[0x19] == 3);
  ASSERT_EQ(s->wram[0][0x4ab], 0); // fade ended
  ASSERT_EQ(s->wram[0][0xba0], 0); // no dialogue
  ASSERT(s->wram[1][0] != 0); // actual Link object enabled
  ASSERT_EQ(s->wram[1][4], 1); // normal Link state
}

int main(int argc, char **argv) {
  bool disable_second = argc == 2 && strcmp(argv[1], "--disable-second-input") == 0;
  ASSERT(argc == 1 || disable_second);
  Tas movie;
  ASSERT(tas_load(&movie, TAS_DIR "/seasons-consoleverified.inputs"));
  Input inputs[4] = {{&movie, UINT64_MAX, JOY_RIGHT}, {&movie, UINT64_MAX, JOY_LEFT},
                     {&movie, UINT64_MAX, JOY_RIGHT}, {&movie, UINT64_MAX, JOY_LEFT}};
  GB *g[4];
  for (unsigned i = 0; i < 4; i++) g[i] = new_runtime(&inputs[i]);
  Frame *expected[2] = {calloc(TOTAL_FRAMES, sizeof(Frame)), calloc(TOTAL_FRAMES, sizeof(Frame))};
  ASSERT(expected[0] && expected[1]);
  unsigned initial_x = 0, initial_y = 0;
  for (unsigned stream = 0; stream < 2; stream++) {
    GB *gb = g[stream];
    for (unsigned f = 0; f < TOTAL_FRAMES; f++) {
      if (f == BOOTSTRAP_FRAMES) {
        describe("fixture", gb);
        assert_gameplay(gb);
        ASSERT_EQ(gb->sample->frame, 9688);
        ASSERT_EQ(gb->sample->wram[0][0xc49], 0);
        ASSERT_EQ(gb->sample->wram[0][0xc4c], 0xc4);
        ASSERT_EQ(gb->sample->wram[1][0xd], 72);
        ASSERT_EQ(gb->sample->wram[1][0xb], 79);
        ASSERT(threads_parked(gb));
        const char *fixture_out = getenv("COOP_FIXTURE_OUT");
        if (!stream && fixture_out) ASSERT(oracles_save_boot_state(gb, fixture_out));
        inputs[stream].branch = GRID_FRAME(gb->cycles);
        if (!stream) { initial_x = gb->sample->wram[1][0xd]; initial_y = gb->sample->wram[1][0xb]; }
      }
      step(gb);
      expected[stream][f] = signature(gb);
    }
    describe(stream ? "isolated-left" : "isolated-right", gb);
  }
  // The candidate instances take alternating COMPLETE frames. References above each ran alone.
  // A test-only missing P2 stream must disagree with its isolated left-input reference.
  for (unsigned f = 0; f < TOTAL_FRAMES; f++) {
    for (unsigned stream = 0; stream < 2; stream++) {
      GB *gb = g[stream + 2];
      if (f == BOOTSTRAP_FRAMES) {
        assert_gameplay(gb);
        inputs[stream + 2].branch = GRID_FRAME(gb->cycles);
        if (stream && disable_second) inputs[3].buttons = 0;
      }
      step(gb);
      compare(gb, expected[stream][f], stream, f);
    }
  }
  assert_gameplay(g[2]); assert_gameplay(g[3]);
  const GBSample *right = g[2]->sample, *left = g[3]->sample;
  ASSERT(right->wram[1][0xd] != left->wram[1][0xd] || right->wram[1][0xb] != left->wram[1][0xb]);
  ASSERT(right->wram[1][0xd] != initial_x || right->wram[1][0xb] != initial_y);
  ASSERT(left->wram[1][0xd] != initial_x || left->wram[1][0xb] != initial_y);
  describe("alternated-right", g[2]); describe("alternated-left", g[3]);
  printf("PASS native runtime isolation: 2 streams, %u frames each, sampled RAM/render/pixels equal isolated references; gameplay outcomes differ\n", TOTAL_FRAMES);
  // The upstream API has no fiber destructor. Keep all four allocations alive until exit:
  // render_line_regs also keys a bounded side table by GB address. No address reuse is claimed.
  return 0;
}
