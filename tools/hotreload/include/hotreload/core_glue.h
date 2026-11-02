// The core side of a reload, in two calls. Link the static library `hotreload_core` into your core DLL.
//
//   int Init() {                       // HrCoreApi::init
//       ... start threads, install hooks, register methods ...
//       return hotreload::AfterLoad(g_api) ? 1 : 0;        // restores saved state (or starts fresh)
//   }
//   int BeforeUnload() { return hotreload::BeforeUnload(g_api) ? 1 : 0; }   // HrCoreApi::before_unload
//
// BeforeUnload does, in this order: preflight and save every participant, drain (wait until no thread but the ones the
// core owns runs core code), shut every participant down, check the leak probes, and hand the state blob to the loader.
// It returns false to refuse. A refusal before the shutdown step leaves the core fully working, so the loader just keeps it.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "hotreload/abi.h"
#include "hotreload/reload_drain.h"
#include "hotreload/reload_state.h"

namespace hotreload {

// For mods that patch engine code: the glue cannot know your hooks. Set these once and the drain takes them into account.
struct DrainHooks {
    // Put the original code back, so no thread can enter a detour any more (the trampolines stay allocated).
    // Return how many hooks could not be taken off (0 = all off). Called right before the drain.
    std::function<int()> suspend;
    // Called when the drain refused: put the hooks back so the core keeps working.
    std::function<void()> resume;
    // Add the address ranges of your trampolines and generated code, so a thread inside them counts as busy.
    std::function<void(std::vector<CodeRange>&)> extraRanges;
};
void SetDrainHooks(DrainHooks hooks);

// Old core: see the sequence above. `api` is the HrLoaderApi the core received.
bool BeforeUnload(const HrLoaderApi* api);
// New core: call at the end of init. On a hot reload it reads the blob the previous core saved and restores every
// participant. It also registers the control method `reload.state`.
bool AfterLoad(const HrLoaderApi* api);

// The JSON that `reload.state` returns: participants and versions, live threads, and what the last save, drain,
// shutdown or restore reported (notes, failures, leaks, restored, fresh, migrated, dropped, drain).
std::string StatusJson();

}  // namespace hotreload
