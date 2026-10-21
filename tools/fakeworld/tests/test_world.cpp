// World model, digests, codecs.
#include "fakeworld/messages.h"
#include "fakeworld/world.h"
#include "fakeworld/wire.h"
#include "minitest.h"

using namespace coop;
using namespace coop::fakeworld;

namespace {
WorldSpec spec(uint64_t seed) { WorldSpec s; s.seed = seed; s.gridW = 3; s.gridH = 3; return s; }
Ref Q(uint32_t i) { return Ref("world.pack", i); }
}  // namespace

TEST(world, build_is_deterministic_per_seed) {
    World a = World::build(spec(1)), b = World::build(spec(1)), c = World::build(spec(2));
    for (int k = 0; k < 4; ++k) {
        CHECK(a.digest(static_cast<Category>(k)) == b.digest(static_cast<Category>(k)));
    }
    CHECK(a.digest(Category::Actors) != c.digest(Category::Actors));
    CHECK(a.actors().size() > 10);
    CHECK(a.cells().size() == 9 + 2);
}

TEST(world, cells_adjacency_and_proximity_loading) {
    World w = World::build(spec(1));
    CHECK(w.adjacent(w.cellAt(0, 0), w.cellAt(1, 1)));
    CHECK(!w.adjacent(w.cellAt(0, 0), w.cellAt(2, 0)));
    CHECK(w.loadedAround(w.cellAt(1, 1)).size() == 9);
    CHECK(w.loadedAround(w.cellAt(0, 0)).size() == 4);
    CHECK(w.loadedAround(1000).size() == 1);   // interiors load alone
    CHECK(!w.adjacent(1000, w.cellAt(0, 0)));
    CHECK(w.cellAt(3, 0) == 0);
}

TEST(world, save_load_roundtrip_keeps_every_digest) {
    World a = World::build(spec(3));
    a.applyEffect(SetStage{Q(1), 40});
    a.player(1).present = true; a.player(1).cell = 2; a.player(1).inventory[Q(5)] = 3;
    Bytes b = a.save();
    World c = World::build(spec(99));
    REQUIRE(c.load(b));
    for (int k = 0; k < 4; ++k) CHECK(a.digest(static_cast<Category>(k)) == c.digest(static_cast<Category>(k)));
    CHECK(c.player(1).inventory.at(Q(5)) == 3);
    CHECK(c.quests().at(Q(1)) == 40);
    Bytes trunc(b.begin(), b.begin() + static_cast<std::ptrdiff_t>(b.size() / 2));
    World d = World::build(spec(1));
    CHECK(!d.load(trunc));
    CHECK(d.digest(Category::Actors) == World::build(spec(1)).digest(Category::Actors));   // a failed load changes nothing
}

TEST(world, export_import_key_repairs_one_entity) {
    World a = World::build(spec(1)), b = World::build(spec(1));
    NetId id = a.actors().begin()->first;
    a.actors().at(id).health = 12; a.actors().at(id).pos.x += 999;
    CHECK(a.digest(Category::Actors) != b.digest(Category::Actors));
    Archive w = Archive::writer();
    REQUIRE(a.exportKey(Category::Actors, id, w));
    Bytes bytes = w.finish();
    Archive r = Archive::reader(bytes.data(), bytes.size());
    CHECK(b.importKey(Category::Actors, id, r));
    CHECK(a.digest(Category::Actors) == b.digest(Category::Actors));
    // wrong key is refused
    Archive r2 = Archive::reader(bytes.data(), bytes.size());
    CHECK(!b.importKey(Category::Actors, id + 1, r2));
    b.removeKey(Category::Actors, id);
    CHECK(b.actors().count(id) == 0);
}

TEST(world, effects_stage_is_max_kill_makes_corpse_spawn_is_idempotent) {
    World w = World::build(spec(1));
    w.applyEffect(SetStage{Q(7), 30});
    w.applyEffect(SetStage{Q(7), 10});
    CHECK(w.quests().at(Q(7)) == 30);
    const ActorState& a0 = w.actors().begin()->second;
    CHECK(w.applyEffect(KillActor{a0.ref}));
    CHECK(w.actors().begin()->second.dead);
    CHECK(w.containers().count(a0.id) == 1);
    SpawnRef s; s.id = kCreatedIdBase | 5; s.base = Q(0x100001); s.cell = w.cellRef(1); s.x = 10;
    CHECK(w.applyEffect(s));
    const size_t n = w.actors().size();
    CHECK(w.applyEffect(s));
    CHECK(w.actors().size() == n);
    DespawnRef d; d.id = s.id;
    CHECK(w.applyEffect(d));
    CHECK(w.actors().size() == n - 1);
    CHECK(!w.applyEffect(KillActor{Q(0xDEAD)}));   // unknown target
    CHECK(w.effectStats().unknownTarget == 1);
}

TEST(world, object_state_sticky_flags_never_unset) {
    World w = World::build(spec(1));
    const NetId door = w.objects().begin()->first;
    ObjectState o; o.ref = placedRef(door); o.mask = kObjDestroyed | kObjActivated | kObjOpen; o.destroyed = true; o.activatedOnce = true; o.open = true;
    CHECK(w.applyEffect(o));
    o.destroyed = false; o.activatedOnce = false; o.open = false;
    CHECK(w.applyEffect(o));
    CHECK(w.objects().at(door).destroyed);
    CHECK(w.objects().at(door).activatedOnce);
    CHECK(!w.objects().at(door).open);
}

TEST(world, duplicate_detection_finds_two_live_actors_per_site_and_repeated_drop_origins) {
    World w = World::build(spec(1));
    CHECK(w.duplicates().empty());
    for (int i = 0; i < 2; ++i) {
        ActorState a; a.id = kCreatedIdBase | (10 + i); a.ref = createdRef(a.id); a.created = true; a.site = 0x5000; a.cell = 1;
        w.putActor(a);
    }
    CHECK(w.duplicates().size() == 1);
    ItemState a, b; a.id = kCreatedIdBase | 40; b.id = kCreatedIdBase | 41; a.origin = b.origin = 77;
    w.items()[a.id] = a; w.items()[b.id] = b;
    CHECK(w.duplicates().size() == 2);
}

TEST(world, effects_roundtrip_through_the_wire_and_apply_the_same) {
    std::vector<Effect> fx;
    fx.push_back(SetStage{Q(3), 5});
    fx.push_back(StartQuest{Q(4)});
    fx.push_back(StopQuest{Q(4)});
    fx.push_back(KillActor{Q(5)});
    fx.push_back(ResurrectActor{Q(5)});
    fx.push_back(MoveRef{Q(6), Q(7), 1, 2, 3});
    fx.push_back(EnableRef{Q(8)});
    fx.push_back(DisableRef{Q(8)});
    ObjectState os; os.ref = Q(9); os.mask = kObjOpen | kObjLock; os.open = true; os.locked = true; os.level = 40; fx.push_back(os);
    fx.push_back(SetLock{Q(9), true, 12});
    SpawnRef sp; sp.id = kCreatedIdBase | 7; sp.base = Q(10); sp.cell = Q(11); sp.x = 4; fx.push_back(sp);
    DespawnRef dp; dp.id = sp.id; fx.push_back(dp);
    fx.push_back(CompanionOwner{Q(12), 2, true});
    CHECK(fx.size() == std::variant_size_v<Effect>);
    for (Effect& e : fx) {
        Archive w = Archive::writer();
        io(w, e);
        Bytes b = w.finish();
        Archive r = Archive::reader(b.data(), b.size());
        Effect back;
        io(r, back);
        CHECK(r.ok() && r.atEnd() && back.index() == e.index());
        Archive w2 = Archive::writer();
        io(w2, back);
        CHECK(w2.finish() == b);   // re-encoding the decoded effect is byte identical
        for (size_t n = 0; n < b.size(); ++n) {   // a truncated effect never decodes
            Archive t = Archive::reader(b.data(), n);
            Effect x;
            io(t, x);
            CHECK(!(t.ok() && t.atEnd()));
        }
    }
    World a = World::build(spec(1)), c = World::build(spec(1));
    a.applyEffect(fx[0]);
    Archive w = Archive::writer();
    io(w, fx[0]);
    Bytes b = w.finish();
    Archive r = Archive::reader(b.data(), b.size());
    Effect back;
    io(r, back);
    c.applyEffect(back);
    CHECK(a.digest(Category::Story) == c.digest(Category::Story));
    CHECK(c.quests().at(Q(3)) == 5);
}

TEST(world, envelope_codec_roundtrips_every_message_type) {
    std::vector<Body> bodies;
    MFullState fs; fs.sess = 3; fs.hostNowMs = 99; fs.fxSeq = 4; fs.world = {1, 2, 3}; OwnMsg om; om.netId = 5; om.epoch = 2; om.owner = 1; fs.own = {om};
    bodies.push_back(fs);
    MPose po; po.slot = 2; po.cell = 4; po.pos = {1, 2, 3}; po.seq = 8; bodies.push_back(po);
    MVitals vi; vi.slot = 1; vi.health = 50; vi.seq = 2; bodies.push_back(vi);
    MActorSnap sn; sn.id = 9; sn.epoch = 2; sn.seq = 5; sn.pos = {4, 5, 6}; sn.settle = true; bodies.push_back(sn);
    MActorEvent ev; ev.id = 9; ev.eventId = 123456789012ull; ev.kind = ActorEventKind::Death; ev.dead = true; ev.loot = {{Q(1), 2}}; bodies.push_back(ev);
    MHit hi; hi.hitId = 77; hi.victim = 3; hi.damage = 25; hi.limb = Limb::Head; bodies.push_back(hi);
    bodies.push_back(MOwn{om});
    MEffects ef; ef.seq = 3; ef.fx = {Effect{SetStage{Q(1), 4}}, Effect{KillActor{Q(2)}}}; bodies.push_back(ef);
    MDropReq dr; dr.reqId = 1; dr.base = Q(3); bodies.push_back(dr);
    MItemCreated ic; ic.id = kCreatedIdBase | 1; ic.base = Q(3); ic.origin = 99; bodies.push_back(ic);
    bodies.push_back(MPickup{5, 6});
    bodies.push_back(MItemTaken{6, 1, 5});
    MContainerReq cr; cr.opId = 4; cr.cont = 3; cr.op = ContainerOpKind::Take; cr.base = Q(1); cr.count = 2; cr.closeHash = 0xABCDEF; bodies.push_back(cr);
    MContainerRes cs; cs.opId = 4; cs.ok = true; cs.op = ContainerOpKind::Put; bodies.push_back(cs);
    bodies.push_back(MContainerSync{3, Q(1), 4});
    bodies.push_back(MSpawnRoll{0x5000});
    MSpawnActor sa; sa.id = kCreatedIdBase | 2; sa.site = 0x5000; sa.base = Q(1); sa.pick = Q(2); bodies.push_back(sa);
    bodies.push_back(MSiteState{0x5000, 0, true, 123, 2});
    bodies.push_back(MActivate{0x4000});
    MDigest dg; dg.cat = 1; dg.total = 5; dg.keys = {{1, 2}, {3, 4}}; bodies.push_back(dg);
    MRepair rp; rp.cat = 2; rp.states = {{7, {1, 2, 3}}}; rp.removes = {9}; bodies.push_back(rp);
    CHECK(bodies.size() == std::variant_size_v<Body>);
    for (auto& b : bodies) {
        Envelope e; e.sess = 4; e.src = 2; e.dest = kNoPlayer; e.body = b;
        Bytes enc = encode(e);
        Envelope back;
        REQUIRE(decode(enc, back));
        CHECK(back.sess == 4 && back.src == 2 && back.dest == kNoPlayer && back.body.index() == b.index());
        CHECK(encode(back) == enc);   // re-encoding the decoded message is byte-identical
        CHECK(std::string(bodyName(b)) != "?");
    }
}

TEST(world, decode_never_crashes_on_garbage_truncation_or_bitflips) {
    Rng r(5);
    MActorEvent ev; ev.id = 9; ev.loot = {{Q(1), 2}, {Q(2), 1}};
    Envelope e; e.sess = 1; e.src = 1; e.dest = kNoPlayer; e.body = ev;
    Bytes good = encode(e);
    int decoded = 0;
    for (size_t n = 0; n <= good.size(); ++n) { Envelope x; decoded += decode(Bytes(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(n)), x) ? 1 : 0; }
    CHECK(decoded == 1);   // only the full message decodes
    for (int i = 0; i < 2000; ++i) {
        Bytes b = good;
        b[r.range(0, static_cast<uint32_t>(b.size() - 1))] ^= static_cast<uint8_t>(1u << r.range(0, 7));
        Envelope x; decode(b, x);
        Bytes junk(r.range(0, 80));
        for (auto& v : junk) v = static_cast<uint8_t>(r.u32());
        decode(junk, x);
    }
    CHECK(true);
}

TEST(world, intent_roundtrip) {
    Action i; i.kind = ActionKind::Attack; i.a = 77; i.n = 31; i.pos = {1, 2, 3}; i.f = 1500; i.base = Q(4); i.limb = Limb::RightLeg; i.flag = true;
    Archive w = Archive::writer(); io(w, i);
    Bytes b = w.finish();
    Archive r = Archive::reader(b.data(), b.size());
    Action back; io(r, back);
    CHECK(r.ok());
    CHECK(back.kind == i.kind && back.a == 77 && back.n == 31 && back.pos == i.pos && back.base == i.base && back.limb == i.limb && back.flag);
    CHECK(std::string(toString(back.kind)) == "Attack");
}
