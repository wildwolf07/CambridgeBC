#pragma once
#include "common.h"

struct Camp {
    std::uint32_t handle = 0;
    bc::camp_type type = bc::camp_type::wolf;
    vec2 pos{};
    float max_hp = 0, str = 0;
    float hp = 0;
    bool dead = false;
};

enum class GState : std::uint8_t { unknown, alive, reward, respawning };

struct Group {
    std::uint32_t handle = 0;
    int gems = 0, gold = 0, respawn = 0;
    std::vector<Camp> camps;
    vec2 center{};
    float str_total = 0, hp_total = 0;
    bool monolith = false;
    GState state = GState::unknown;
    int respawn_round = 0;
    int seen_round = -1;
    int enemy_round = -1000;
    std::unique_ptr<bc::distance_field> field;
    std::vector<std::pair<int, float>> nbrs;  // nearby gem groups and their distance weight

    float hp_left() const {
        float s = 0;
        for (auto& c : camps)
            if (!c.dead) s += c.hp;
        return s;
    }
    float str_left() const {
        float s = 0;
        for (auto& c : camps)
            if (!c.dead) s += c.str;
        return s;
    }
    bool is_tile(vec2 p) const {
        for (auto& c : camps)
            if (c.pos == p) return true;
        return false;
    }
};

struct UnitView {
    std::uint32_t h = 0;
    int type = 0;
    vec2 pos{};
    float hp = 0, max_hp = 0, str = 0, focus = 0;
    int range = 1;
    std::uint32_t state = 0;
    int gems = 0;
    bool stunned = false;
};

struct EnemyMem {
    std::uint32_t h = 0;
    vec2 pos{};
    int round = -1000;
    float hp = 0, max_hp = 1;
    int type = 0;
    int gems = 0;
    float str = 50, focus = 100;
};

struct World {
    int W = 0, H = 0;
    bitboard static_block;
    bitboard altar_bb;
    bitboard ally_spawn, enemy_spawn;
    std::vector<vec2> altars, ally_spawn_tiles;
    vec2 spawn_center{}, enemy_center{};
    std::vector<Group> groups;
    std::unique_ptr<bc::distance_field> altar_field, spawn_field;
    std::vector<std::uint32_t> ally_handles, enemy_handles;
    std::vector<EnemyMem> enemy_mem;
    std::uint32_t self_handle = 0;
    int my_slot = 0;
    int team_size = 0;

    void init();
    const bc::distance_field& group_field(int gi);
    const bc::distance_field& altars_f();
    const bc::distance_field& spawn_f();
    EnemyMem* mem_of(std::uint32_t h);
};

extern World world;
