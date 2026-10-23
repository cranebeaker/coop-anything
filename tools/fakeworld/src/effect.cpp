#include "fakeworld/effect.h"

namespace coop::fakeworld {

void io(Archive& a, Ref& v) {
    a.str(v.pack);
    a.varu32(v.id);
}

namespace {
void io(Archive& a, SetStage& v) { io(a, v.quest); a.varu32(v.stage); }
void io(Archive& a, StartQuest& v) { io(a, v.quest); }
void io(Archive& a, StopQuest& v) { io(a, v.quest); }
void io(Archive& a, KillActor& v) { io(a, v.actor); }
void io(Archive& a, ResurrectActor& v) { io(a, v.actor); }
void io(Archive& a, MoveRef& v) { io(a, v.ref); io(a, v.cell); a.f32(v.x); a.f32(v.y); a.f32(v.z); }
void io(Archive& a, EnableRef& v) { io(a, v.ref); }
void io(Archive& a, DisableRef& v) { io(a, v.ref); }
void io(Archive& a, ObjectState& v) {
    io(a, v.ref); a.u8(v.mask); a.boolean(v.open); a.boolean(v.locked); a.vari32(v.level);
    a.boolean(v.destroyed); a.boolean(v.activatedOnce);
}
void io(Archive& a, SetLock& v) { io(a, v.ref); a.boolean(v.locked); a.vari32(v.level); }
void io(Archive& a, SpawnRef& v) { a.varu32(v.id); io(a, v.base); io(a, v.cell); a.f32(v.x); a.f32(v.y); a.f32(v.z); }
void io(Archive& a, DespawnRef& v) { a.varu32(v.id); }
void io(Archive& a, CompanionOwner& v) { io(a, v.npc); a.u8(v.owner); a.boolean(v.active); }

template <size_t I = 0>
bool readAlt(Archive& a, size_t idx, Effect& out) {
    if constexpr (I == std::variant_size_v<Effect>) {
        (void)a; (void)idx; (void)out;
        return false;
    } else {
        if (idx == I) {
            std::variant_alternative_t<I, Effect> v{};
            io(a, v);
            if (!a.ok()) return false;
            out = std::move(v);
            return true;
        }
        return readAlt<I + 1>(a, idx, out);
    }
}
}  // namespace

void io(Archive& a, Effect& v) {
    uint8_t tag = static_cast<uint8_t>(v.index());
    a.u8(tag);
    if (a.reading()) {
        if (!a.ok() || tag >= std::variant_size_v<Effect> || !readAlt(a, tag, v)) a.fail();
    } else {
        std::visit([&](auto& e) { io(a, e); }, v);
    }
}

}  // namespace coop::fakeworld
