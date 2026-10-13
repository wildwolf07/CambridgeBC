#pragma once
#include "world.h"
#include "shared.h"

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
    Spell dmg;     // single-target damage
    Spell cc;      // single-target stun (may also damage)
    Spell aoe;     // ground area damage
};

class Bot {
public:
    void turn();

    enum Act { A_OTHER, A_DEAD, A_CHAN_REC, A_CHAN_SUB, A_COMBAT, A_HEAL, A_BANK, A_F_TRAV, A_F_ATK, A_F_CAPT, A_F_WAIT, A_F_NONE, A_N };

private:
    void turn_inner();
    int act = 0;
    unsigned long long max_used = 0;
    std::array<int, A_N> acts{};
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
    Shared sh;
    bool moved = false, cast_excl = false;
    int n_alive = 1;
    bool is_leader = false;
    std::vector<float> gscore;
    std::vector<std::uint8_t> claimed_other;
    bool target_valid(int gi) const;
    bool ally_capturing(const Group& g) const;

    void init();
    void snapshot();
    void observe_groups();
    void track_deaths();
    std::vector<std::uint8_t> ally_was_alive, enemy_was_alive;
    int ally_deaths = 0, enemy_deaths = 0;
    void predict_groups();

    float dps_of(int t, float s, float f) const;
    float my_camp_dps() const;
    bool alive_state() const { return st & 1u; }

    // actions
    void shop();
    bool combat();
    bool engage(const UnitView& e);
    void engage_inner(const UnitView& e);
    void kite(const UnitView& e);
    float melee_threat(vec2 q) const;
    void retreat();
    float sim_fight() const;
    // Engagement cluster (CFG_TEAMFIGHT): indices into allies / enemies sharing one connected fight.
    std::vector<int> cl_a, cl_e;
    void build_cluster();
    float flee_cost() const;
    bool helper_ok(const UnitView& a) const;
    bool assist();
    bool answer_rally();
    bool shop_trip();
    bool follow_partner();
    std::uint32_t partner_h = 0;
    int partner_slot = -1;
    bool follower = false;
    std::vector<int> squad;
    bool squad_mode = false;
    int squad_alive = 1;
    std::array<int, 8> slot_type{};
    bool in_squad(int slot) const {
        for (int x : squad)
            if (x == slot) return true;
        return false;
    }
    int squad_leader() const;
    bool follow_squad();
    bool combat_old();
    void engage_old(const UnitView& e);
    void retreat_old();
    float danger_at(vec2 t) const;
    float enemy_danger_at(vec2 t) const;
    bool need_heal() const;
    int threat_level() const;
    float camp_threat_at(vec2 q) const;
    void assess_camps();
    float camp_in = 0;
    bool was_alive = false;
    float prev_hp = 0, prev_camp_in = 0;
    int prev_round = -10, hidden_round = -100;
    vec2 hidden_pos{};
    char last_note[160] = "";
    bool camp_near = false;
    std::vector<const Camp*> near_camps;
    void go_heal();
    bool should_bank();
    void go_bank();
    void place_locks(bool at_spawn);
    void farm();
    int choose_group();
    void act_group(int gi);
    bool opportunistic_attack(int skip_gi);
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
