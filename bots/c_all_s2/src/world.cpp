#include "world.h"

World world;

static vec2 centroid(const std::vector<vec2>& ps) {
    if (ps.empty()) return {0, 0};
    long sx = 0, sy = 0;
    for (auto p : ps) {
        sx += p.x;
        sy += p.y;
    }
    return {static_cast<int>(sx / static_cast<long>(ps.size())), static_cast<int>(sy / static_cast<long>(ps.size()))};
}

void World::init() {
    W = bc::map::width();
    H = bc::map::height();
    altar_bb = bc::map::altars();
    ally_spawn = bc::map::spawn_region(team::ally);
    enemy_spawn = bc::map::spawn_region(team::enemy);
    static_block = bc::map::walls() | bc::map::holes() | altar_bb | enemy_spawn;
    altars = altar_bb.positions();
    ally_spawn_tiles = ally_spawn.positions();
    spawn_center = centroid(ally_spawn_tiles);
    enemy_center = centroid(enemy_spawn.positions());

    for (const auto& g : bc::map::camp_groups()) {
        Group G;
        G.handle = static_cast<std::uint32_t>(static_cast<bc::handle_camp_group_t>(g));
        G.gems = static_cast<int>(g.gem_value());
        G.gold = static_cast<int>(g.gold_value());
        G.respawn = g.max_respawn_timer();
        std::vector<vec2> ps;
        for (const auto& c : g) {
            Camp C;
            C.handle = static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(c));
            C.type = c.kind();
            C.pos = c.spawn_position();
            auto b = camp_base(C.type);
            C.max_hp = C.hp = b.hp;
            C.str = b.str;
            G.str_total += b.str;
            G.hp_total += b.hp;
            if (C.type == bc::camp_type::monolith) G.monolith = true;
            ps.push_back(C.pos);
            G.camps.push_back(C);
        }
        G.center = centroid(ps);
        groups.push_back(std::move(G));
    }

    self_handle = static_cast<std::uint32_t>(bc::detail::sdk_my_champion());
    for (const auto& c : bc::champions(team::ally))
        ally_handles.push_back(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(c)));
    for (const auto& c : bc::champions(team::enemy))
        enemy_handles.push_back(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(c)));
    std::sort(ally_handles.begin(), ally_handles.end());
    team_size = static_cast<int>(ally_handles.size());
    for (int i = 0; i < team_size; ++i)
        if (ally_handles[i] == self_handle) my_slot = i;
    for (auto h : enemy_handles) {
        EnemyMem m;
        m.h = h;
        enemy_mem.push_back(m);
    }
}

const bc::distance_field& World::group_field(int gi) {
    auto& g = groups[gi];
    if (!g.field) {
        bitboard roots;
        for (auto& c : g.camps) roots.set(c.pos);
        bitboard visited;
        g.field = std::make_unique<bc::distance_field>();
        g.field->compute(static_block, roots, visited);
    }
    return *g.field;
}

const bc::distance_field& World::altars_f() {
    if (!altar_field) {
        bitboard walls = static_block ^ altar_bb;
        bitboard visited;
        altar_field = std::make_unique<bc::distance_field>();
        altar_field->compute(walls, altar_bb, visited);
    }
    return *altar_field;
}

const bc::distance_field& World::spawn_f() {
    if (!spawn_field) {
        bitboard visited;
        spawn_field = std::make_unique<bc::distance_field>();
        spawn_field->compute(static_block, ally_spawn, visited);
    }
    return *spawn_field;
}

EnemyMem* World::mem_of(std::uint32_t h) {
    for (auto& m : enemy_mem)
        if (m.h == h) return &m;
    return nullptr;
}
