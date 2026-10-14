#pragma once
#include "types.h"

// Shard build orders (item_type ids: family*6 + {0 str, 2 focus, 4 health}; families M P D T J H B).
// Each entry is bought as five matching shards (one emblem); after the list, entries from loop_from repeat.
namespace it {
constexpr std::uint32_t M_STR = 0, M_FOC = 2, M_HP = 4, P_STR = 6, P_FOC = 8, P_HP = 10, D_STR = 12, D_FOC = 14, D_HP = 16,
                        T_STR = 18, T_FOC = 20, T_HP = 22, J_STR = 24, J_FOC = 26, J_HP = 28, B_STR = 36, B_FOC = 38, B_HP = 40;
}
struct Build {
    std::uint32_t items[8];
    int n;
    int loop_from;
};

inline const Build& build_for(int t) {
    using namespace it;
    static constexpr Build MELEE{{P_STR, J_STR, P_STR, D_STR, P_STR, B_STR}, 6, 2};
    static constexpr Build TANK{{M_STR, J_HP, P_STR, D_HP, P_HP, P_STR}, 6, 2};
    static constexpr Build CASTER{{P_FOC, B_FOC, P_FOC, D_HP, P_FOC, J_FOC}, 6, 2};
    static constexpr Build CASSI{{P_FOC, D_HP, P_FOC, B_FOC, P_FOC, P_FOC}, 6, 2};
    static constexpr Build TAVI{{P_FOC, P_STR, B_FOC, D_HP, P_FOC, P_STR}, 6, 2};
    static constexpr Build WILLOW{{P_FOC, D_HP, P_FOC, B_FOC, P_FOC, P_FOC}, 6, 2};
    switch (t) {
    case ct::RYTH: return TANK;
    case ct::CASSI: return CASSI;
    case ct::STELLAR:
    case ct::PRISM: return CASTER;
    case ct::TAVI: return TAVI;
    case ct::WILLOW: return WILLOW;
    default: return MELEE;
    }
}

// Multiplier on the gold weight in camp choice (the capturer gets the whole pot): route gold to champions that
// turn it into damage fastest.
inline float gold_weight(int t) {
    switch (t) {
    case ct::TAVI: return 2.0f;  // shops anywhere
    case ct::STELLAR: return 1.6f;
    case ct::VENDETTA:
    case ct::VARG: return 1.3f;
    case ct::CASSI:
    case ct::WILLOW: return 0.5f;
    default: return 1.0f;
    }
}
