#pragma once
// Static champion kit table (research/data/game_constants_and_stats.txt). Used to model enemies (whose ability
// state we cannot read) and allies; our own kit is also read from the SDK at init.
#include "common.h"

namespace ct {
enum : int { RYTH = 0, VESPER, VENDETTA, CASSI, ISSAC, STELLAR, PRISM, MARCELLA, WILLOW, REN, VARG, TAVI };
}

struct TypeKit {
    bool melee;      // basic range 1
    int range;       // basic attack range
    bool reset;      // attack reset (one extra basic attack, cd 8)
    bool dash;       // dash 2 (does not block the move)
    float dmg;       // single-target damage spell, x focus, range 4
    float cc;        // single-target stun (+damage x focus), range 4
    float aoe;       // ground AoE x focus, range 4, radius 1
    bool aoe_stun;   // Ryth AoE stuns
    bool spell_excl; // damage/cc/aoe spells block movement (true for every caster here except Cassi's stun)
    float heal;      // heal ally x focus, range 4
    float shield;    // self shield x focus
    float hp0, hp_g, str0, str_g, foc0, foc_g;
};

inline constexpr TypeKit KITS[12] = {
    // melee rng reset dash  dmg   cc    aoe  aoeS excl  heal shield   hp0  hpg   str0 strg  foc0 focg
    {true, 1, true, false, 0.f, 0.f, 1.0f, true, true, 0.f, 0.f, 1200, 100, 50, 5.0f, 100, 10},     // ryth
    {false, 3, true, true, 0.f, 0.f, 0.f, false, true, 0.f, 0.f, 900, 100, 45, 5.0f, 100, 10},     // vesper
    {true, 1, true, true, 0.f, 0.f, 0.f, false, true, 0.f, 0.f, 800, 100, 70, 5.0f, 100, 10},      // vendetta
    {false, 3, false, false, 2.0f, 0.001f, 0.f, false, true, 0.f, 0.f, 700, 71.2f, 30, 3.5f, 70, 15},  // cassi (stun no dmg)
    {true, 1, true, true, 0.f, 0.f, 0.f, false, true, 0.f, 0.f, 900, 95, 40, 4.75f, 100, 10},      // issac
    {false, 3, false, false, 2.0f, 0.f, 1.4f, false, true, 0.f, 0.f, 750, 75, 35, 3.5f, 100, 10},  // stellar
    {false, 3, false, false, 0.f, 1.5f, 1.0f, false, true, 0.f, 0.f, 700, 75, 35, 3.5f, 80, 10},   // prism
    {true, 1, true, false, 2.0f, 0.f, 0.f, false, true, 0.f, 0.f, 1200, 100, 45, 5.0f, 50, 10},    // marcella
    {false, 3, true, false, 0.f, 0.f, 0.f, false, false, 3.0f, 0.f, 700, 50, 45, 3.0f, 105, 5},    // willow
    {true, 1, false, true, 0.f, 1.25f, 0.f, false, true, 0.f, 0.f, 900, 100, 40, 5.0f, 100, 10},   // ren
    {true, 1, true, false, 0.f, 0.f, 0.f, false, true, 0.f, 2.5f, 1000, 100, 50, 5.0f, 100, 10},   // varg
    {false, 3, true, false, 0.f, 2.2f, 0.f, false, true, 0.f, 0.f, 750, 75, 35, 5.0f, 100, 10},    // tavi
};

inline const TypeKit& kit_of(int t) { return KITS[(t < 0 || t > 11) ? 0 : t]; }

// Expected damage per round over a cooldown cycle (basic + reset + spells).
inline float sustained_dps(int t, float str, float foc) {
    const auto& k = kit_of(t);
    return str * (k.reset ? 1.125f : 1.f) + (k.dmg + k.cc + k.aoe) * foc / 8.f;
}
// Damage this unit could put on one target in a single turn if everything is ready.
inline float burst(int t, float str, float foc) {
    const auto& k = kit_of(t);
    return str * (k.reset ? 2.f : 1.f) + (k.dmg + k.cc) * foc + (k.aoe > 0 ? k.aoe * foc : 0.f);
}
inline bool is_ci(int t) { return t == ct::CASSI || t == ct::ISSAC; }
inline bool is_caster(int t) { return t == ct::STELLAR || t == ct::PRISM || t == ct::CASSI || t == ct::TAVI || t == ct::WILLOW; }
