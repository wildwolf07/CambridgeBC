#pragma once
#include "world.h"

struct Spell {
    int slot = -1;
    bc::ability_kind kind{};
    float range = 0, mod = 0, radius = 0;
    int dur = 0;
    bool excl = false;
};

struct Kit {
    bool melee = true;
    int reset = -1, dash = -1, shield = -1, heal = -1;
    Spell single;  // damage or cc on a unit
    Spell aoe;     // ground area damage
};

class Bot {
public:
    void turn();

private:
    // persistent
    bool inited = false;
    int type = 0;
    Kit kit;
    int target = -1;
    bool healing = false;
    vec2 last_pos{-1, -1};
    int build_step = 0;
    int stuck = 0;

    // snapshot
    int round = 0;
    vec2 pos{};
    float hp = 0, max_hp = 1, str = 0, focus = 0;
    int range = 1;
    std::uint32_t st = 0;
    int gems = 0, gold = 0;
    bitboard vis, occ, blocked;
    std::vector<UnitView> enemies, allies;
    std::array<std::uint8_t, 8> shm{};
    bool moved = false, cast_excl = false;
    int n_alive = 1;
    bool is_leader = false;
    std::vector<float> gscore;
    bool target_valid(int gi) const;

    void init();
    void snapshot();
    void observe_groups();
    void predict_groups();

    float dps_of(int t, float s, float f) const;
    float my_camp_dps() const;
    bool alive_state() const { return st & 1u; }

    // actions
    void shop();
    bool combat();
    void engage(const UnitView& e);
    void retreat();
    bool need_heal() const;
    void go_heal();
    bool should_bank();
    void go_bank();
    void farm();
    int choose_group();
    void act_group(int gi);
    bool attack_camp(Group& g);
    bool attack_unit_handle(std::uint32_t h, bool champ);
    bool try_reset_attack(std::uint32_t h, bool champ);
    bool cast_on_camp(const Camp& c);
    void claim(int gi);
    int claims_on(int gi) const;

    // movement
    bool step_field(const bc::distance_field& f, bool onto_root);
    bool step_towards(vec2 t);
    bool try_dash_field(const bc::distance_field& f);
    bool can_walk() const;
};
