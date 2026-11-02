// GENERATED from profiles.json by tools/chaos/gen_header.py. Do not edit.
#pragma once
#include "impair.h"

namespace coopchaos {

inline const std::vector<std::pair<std::string, Params>>& allProfiles() {
    static const std::vector<std::pair<std::string, Params>> v = {
        {"lan", Params::make(1001ULL, 0.5, 0.2, 0.0, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0, 200.0)},
        {"good_wifi", Params::make(1002ULL, 12.0, 4.0, 0.005, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0, 200.0)},
        {"bad_wifi", Params::make(1003ULL, 35.0, 25.0, 0.02, 0.01, 0.25, 0.6, 0.02, 0.005, 0.0, 0, 200.0)},
        {"mobile_tether", Params::make(1004ULL, 70.0, 40.0, 0.01, 0.01, 0.2, 0.5, 0.01, 0.0, 0.0, 2000000, 300.0)},
        {"transatlantic", Params::make(1005ULL, 90.0, 8.0, 0.003, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0, 200.0)},
        {"hell", Params::make(1006ULL, 200.0, 150.0, 0.08, 0.03, 0.1, 0.9, 0.1, 0.05, 0.0, 512000, 500.0)},
    };
    return v;
}

}  // namespace coopchaos
