#include "bot.h"
#include <cmath>

namespace {
constexpr std::uint32_t ST_ALIVE = 1u << 0, ST_RECALL = 1u << 1, ST_SUBMIT = 1u << 2, ST_MOVE = 1u << 3,
                        ST_ATTACK = 1u << 4, ST_CAST = 1u << 11, ST_TELE = 1u << 12;

struct TypeInfo {
    bool melee;
    bool reset;
    float spell_mod;
};
constexpr TypeInfo TYPES[12] = {
    {true, true, 1.0f},    // ryth
    {false, true, 0.f},    // vesper
    {true, true, 0.f},     // vendetta
    {false, false, 2.0f},  // cassi
    {true, true, 0.f},     // issac
    {false, false, 2.0f},  // stellar
    {false, false, 1.5f},  // prism
    {true, true, 2.0f},    // marcella
    {false, true, 0.f},    // willow
    {true, false, 1.25f},  // ren
    {true, true, 0.f},     // varg
    {false, true, 2.2f},   // tavi
};

bool raw_info(std::uint32_t h, bc::detail::champion_info& ci) {
    return bc::detail::sdk_champion(h, &ci) == 0 && ci.x >= 0 && ci.y >= 0;
}
int raw_move(vec2 d) {
    std::int32_t ok = 0;
    if (bc::detail::sdk_move_to(d.x, d.y, &ok) != 0) return -1;
    return ok ? 1 : 0;
}
bool raw_attack(bool champ, std::uint32_t h) { return bc::detail::sdk_attack(champ ? 1u : 2u, h) == 0; }
int raw_cast(int slot, std::uint32_t kind, std::uint32_t h, vec2 d) {
    std::int32_t ok = 0;
    if (bc::detail::sdk_cast(static_cast<std::uint32_t>(slot), kind, h, d.x, d.y, &ok) != 0) return -1;
    return ok ? 1 : 0;
}
int cooldown(int slot) {
    if (slot < 0) return 99;
    std::int32_t cd = 99;
    if (bc::detail::sdk_champion_cooldown(world.self_handle, static_cast<std::uint32_t>(slot), &cd) != 0) return 99;
    return cd;
}
bool in_bounds(vec2 p) { return p.x >= 0 && p.y >= 0 && p.x < world.W && p.y < world.H; }

// Shard build orders (item_type enum values). Cycle the tail once exhausted.
constexpr std::uint32_t P_STR = 6, P_FOC = 8, P_HP = 10, D_HP = 16, J_HP = 28;
constexpr std::uint32_t BUILD_STR[] = {P_STR, P_STR, P_STR, P_STR, P_STR, J_HP, J_HP, J_HP, J_HP, J_HP,
                                       P_STR, P_STR, P_STR, P_STR, P_STR, D_HP, D_HP, D_HP, D_HP, D_HP,
                                       P_STR, P_STR, P_STR, P_STR, P_STR, P_HP, P_HP, P_HP, P_HP, P_HP};
constexpr std::uint32_t BUILD_TANK[] = {P_HP,  P_HP,  P_HP,  P_HP,  P_HP,  P_STR, P_STR, P_STR, P_STR, P_STR,
                                        J_HP,  J_HP,  J_HP,  J_HP,  J_HP,  P_STR, P_STR, P_STR, P_STR, P_STR,
                                        D_HP,  D_HP,  D_HP,  D_HP,  D_HP,  P_HP,  P_HP,  P_HP,  P_HP,  P_HP};
}  // namespace

// ---------------------------------------------------------------- init / snapshot

void Bot::init() {
    inited = true;
    world.init();
    bc::detail::champion_info ci{};
    bc::detail::sdk_champion(world.self_handle, &ci);
    type = ci.type;
    kit.melee = ci.attack_range <= 1;
    const auto abs = bc::self().abilities();
    for (const auto& a : abs) {
        const auto& inf = a.info();
        if (!inf) continue;
        const int slot = static_cast<int>(a.slot());
        std::visit(
            [&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bc::ability_attack_reset>) kit.reset = slot;
                else if constexpr (std::is_same_v<T, bc::ability_dash>) kit.dash = slot;
                else if constexpr (std::is_same_v<T, bc::ability_shield>) kit.shield = slot;
                else if constexpr (std::is_same_v<T, bc::ability_heal>) kit.heal = slot;
                else if constexpr (std::is_same_v<T, bc::ability_damage>) {
                    kit.single = {slot, bc::ability_kind::damage, v.range, v.modifier, 0, 0, v.exclusive_with_movement};
                } else if constexpr (std::is_same_v<T, bc::ability_cc>) {
                    kit.single = {slot, bc::ability_kind::cc, v.range, v.modifier, 0, v.duration, v.exclusive_with_movement};
                } else if constexpr (std::is_same_v<T, bc::ability_aoe_damage>) {
                    kit.aoe = {slot, bc::ability_kind::aoe_damage, v.range, v.modifier, v.radius, v.duration,
                               v.exclusive_with_movement};
                }
            },
            *inf);
    }
    world.spawn_f();
    world.altars_f();
}

void Bot::snapshot() {
    round = static_cast<int>(bc::round_number());
    bc::detail::champion_info ci{};
    bc::detail::sdk_champion(world.self_handle, &ci);
    st = static_cast<std::uint32_t>(ci.state);
    pos = {ci.x, ci.y};
    hp = ci.health;
    max_hp = ci.max_health > 1 ? ci.max_health : 1;
    str = ci.strength;
    focus = ci.focus;
    range = ci.attack_range;
    std::uint32_t v = 0;
    bc::detail::sdk_champion_gems(world.self_handle, &v);
    gems = static_cast<int>(v);
    gold = static_cast<int>(bc::self().gold_count());
    bc::detail::sdk_read_shared_memory(0, shm.data(), 8);
    moved = false;
    cast_excl = false;
    if (!alive_state()) return;
    n_alive = 0;
    is_leader = false;
    for (int i = 0; i < world.team_size; ++i) {
        std::int32_t a = 0;
        if (bc::detail::sdk_champion_is_alive(world.ally_handles[i], &a) == 0 && a) {
            if (n_alive == 0 && i == world.my_slot) is_leader = true;
            ++n_alive;
        }
    }

    vis = bc::vision::visible_tiles();
    occ = bc::vision::champion_tiles() | bc::vision::camp_tiles() | bc::vision::teleport_tiles();
    occ.set(pos, false);
    blocked = world.static_block | occ;

    enemies.clear();
    allies.clear();
    for (auto h : world.enemy_handles) {
        if (!raw_info(h, ci) || !(static_cast<std::uint32_t>(ci.state) & ST_ALIVE)) continue;
        UnitView u;
        u.h = h;
        u.type = ci.type;
        u.pos = {ci.x, ci.y};
        u.hp = ci.health;
        u.max_hp = ci.max_health;
        u.str = ci.strength;
        u.focus = ci.focus;
        u.range = ci.attack_range;
        u.state = static_cast<std::uint32_t>(ci.state);
        v = 0;
        bc::detail::sdk_champion_gems(h, &v);
        u.gems = static_cast<int>(v);
        enemies.push_back(u);
        if (auto* m = world.mem_of(h)) {
            m->pos = u.pos;
            m->round = round;
            m->hp = u.hp;
            m->max_hp = u.max_hp;
            m->type = u.type;
            m->gems = u.gems;
        }
    }
    for (auto h : world.ally_handles) {
        if (h == world.self_handle) continue;
        if (!raw_info(h, ci) || !(static_cast<std::uint32_t>(ci.state) & ST_ALIVE)) continue;
        UnitView u;
        u.h = h;
        u.type = ci.type;
        u.pos = {ci.x, ci.y};
        u.hp = ci.health;
        u.max_hp = ci.max_health;
        u.str = ci.strength;
        u.focus = ci.focus;
        u.range = ci.attack_range;
        u.state = static_cast<std::uint32_t>(ci.state);
        allies.push_back(u);
    }
}

void Bot::observe_groups() {
    for (auto& g : world.groups) {
        if (cheb(pos, g.center) > 11) continue;
        bool seen = false;
        for (auto& c : g.camps)
            if (vis.is_set(c.pos)) {
                seen = true;
                break;
            }
        if (!seen) continue;
        std::uint32_t s = 0;
        if (bc::detail::sdk_camp_group_state(g.handle, &s) != 0 || s == 0) continue;
        g.seen_round = round;
        if (s == 1) {
            g.state = GState::alive;
            for (auto& c : g.camps) {
                if (!vis.is_set(c.pos)) continue;
                // Camps get fresh handles when a group respawns; resolve by tile.
                std::uint32_t h = 0;
                bc::detail::camp_state cs{};
                if (bc::detail::sdk_camp_at(c.pos.x, c.pos.y, &h) == 0 && bc::detail::sdk_camp(h, &cs) == 0) {
                    c.handle = h;
                    c.hp = cs.health;
                    c.dead = false;
                } else {
                    c.dead = true;
                }
            }
        } else if (s == 2) {
            g.state = GState::reward;
            for (auto& c : g.camps) c.dead = true;
        } else if (s == 3) {
            g.state = GState::respawning;
            std::int32_t t = 0;
            bc::detail::sdk_camp_group_cooldown(g.handle, &t);
            g.respawn_round = round + t;
            for (auto& c : g.camps) c.dead = true;
        }
        for (auto& e : enemies)
            if (cheb(e.pos, g.center) <= 5) g.enemy_round = round;
    }
}

void Bot::predict_groups() {
    for (auto& g : world.groups) {
        const bool respawned = g.state == GState::respawning && round >= g.respawn_round;
        const bool stale_reward = g.state == GState::reward && round - g.seen_round > 45;
        if (respawned || stale_reward) {
            g.state = GState::unknown;
            for (auto& c : g.camps) {
                c.dead = false;
                c.hp = c.max_hp;
            }
        }
    }
}

float Bot::dps_of(int t, float s, float f) const {
    if (t < 0 || t > 11) return s;
    const auto& ti = TYPES[t];
    return s * (ti.reset ? 1.125f : 1.f) + ti.spell_mod * f / 8.f;
}

float Bot::my_camp_dps() const {
    float d = dps_of(type, str, focus);
    if (type == static_cast<int>(bc::champion_type::varg)) d *= 1.3f;
    return d;
}

// ---------------------------------------------------------------- shared memory claims

void Bot::claim(int gi) {
    if (world.my_slot >= 6) return;
    const std::uint8_t val = gi < 0 ? 0 : static_cast<std::uint8_t>(gi + 1);
    if (shm[world.my_slot] == val) return;
    shm[world.my_slot] = val;
    bc::detail::sdk_write_shared_memory(world.my_slot, &val, 1);
}

int Bot::claims_on(int gi) const {
    int n = 0;
    const int lim = std::min(world.team_size, 6);
    for (int i = 0; i < lim; ++i)
        if (i != world.my_slot && shm[i] == gi + 1) ++n;
    return n;
}

// ---------------------------------------------------------------- movement

bool Bot::can_walk() const { return (st & ST_MOVE) && !cast_excl; }

bool Bot::step_field(const bc::distance_field& f, bool onto_root) {
    if (!can_walk()) return false;
    const int cur = f.get(pos);
    vec2 cp[8];
    int cv[8];
    int n = 0;
    for (auto d : DIRS) {
        const vec2 q = pos + d;
        if (!in_bounds(q) || blocked.is_set(q)) continue;
        const int v = f.get(q);
        if (v == UNREACH || (v == 0 && !onto_root)) continue;
        if (v > cur) continue;
        int k = n++;
        while (k > 0 && (cv[k - 1] > v || (cv[k - 1] == v && cp[k - 1] == last_pos))) {
            cp[k] = cp[k - 1];
            cv[k] = cv[k - 1];
            --k;
        }
        cp[k] = q;
        cv[k] = v;
    }
    for (int i = 0; i < n; ++i) {
        if (cv[i] == cur && cp[i] == last_pos) continue;
        const int r = raw_move(cp[i]);
        if (r == 1) {
            last_pos = pos;
            pos = cp[i];
            moved = true;
            st &= ~ST_MOVE;
            return true;
        }
        blocked.set(cp[i]);
        if (r < 0) break;
    }
    return false;
}

bool Bot::step_towards(vec2 t) {
    if (!can_walk()) return false;
    const int cur = cheb(pos, t);
    vec2 best = pos;
    long best_score = static_cast<long>(cur) * 1000 + (static_cast<long>(pos.x - t.x) * (pos.x - t.x) + static_cast<long>(pos.y - t.y) * (pos.y - t.y));
    for (auto d : DIRS) {
        const vec2 q = pos + d;
        if (!in_bounds(q) || blocked.is_set(q)) continue;
        const long sc = static_cast<long>(cheb(q, t)) * 1000 +
                        (static_cast<long>(q.x - t.x) * (q.x - t.x) + static_cast<long>(q.y - t.y) * (q.y - t.y));
        if (sc < best_score) {
            best_score = sc;
            best = q;
        }
    }
    if (best == pos) return false;
    if (raw_move(best) == 1) {
        last_pos = pos;
        pos = best;
        moved = true;
        st &= ~ST_MOVE;
        return true;
    }
    blocked.set(best);
    return false;
}

bool Bot::try_dash_field(const bc::distance_field& f) {
    if (kit.dash < 0 || !(st & ST_CAST) || cooldown(kit.dash) != 0) return false;
    const int cur = f.get(pos);
    if (cur == UNREACH || cur < 4) return false;
    vec2 best = pos;
    int bv = cur - 1;
    for (int dx = -2; dx <= 2; ++dx)
        for (int dy = -2; dy <= 2; ++dy) {
            if (iabs(dx) < 2 && iabs(dy) < 2) continue;
            const vec2 q{pos.x + dx, pos.y + dy};
            if (!in_bounds(q) || blocked.is_set(q)) continue;
            const int v = f.get(q);
            if (v < bv) {
                bv = v;
                best = q;
            }
        }
    if (best == pos) return false;
    if (raw_cast(kit.dash, 0, 0, best) == 1) {
        last_pos = pos;
        pos = best;
        st &= ~ST_CAST;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- shopping

void Bot::shop() {
    if (gold < 30) return;
    const bool tank = type == static_cast<int>(bc::champion_type::ryth);
    const std::uint32_t* build = tank ? BUILD_TANK : BUILD_STR;
    constexpr int len = 30;
    int guard = 0;
    while (gold >= 30 && guard++ < 12) {
        int idx = build_step < len ? build_step : 10 + (build_step - len) % (len - 10);
        if (bc::detail::sdk_buy_item(build[idx], 1) != 0) break;
        gold -= 30;
        ++build_step;
    }
}

// ---------------------------------------------------------------- attacks

bool Bot::try_reset_attack(std::uint32_t h, bool champ) {
    if (kit.reset < 0 || !(st & ST_CAST) || cooldown(kit.reset) != 0) return false;
    if (raw_cast(kit.reset, 0, 0, {-1, -1}) != 1) return false;
    st &= ~ST_CAST;
    if (raw_attack(champ, h)) return true;
    return false;
}

bool Bot::attack_unit_handle(std::uint32_t h, bool champ) {
    if (!(st & ST_ATTACK)) return false;
    if (!raw_attack(champ, h)) return false;
    st &= ~ST_ATTACK;
    if (!kit.melee) st &= ~ST_MOVE;
    // Follow up with an attack reset if the target survived.
    bool alive_t = false;
    if (champ) {
        bc::detail::champion_info ci{};
        alive_t = raw_info(h, ci) && (static_cast<std::uint32_t>(ci.state) & ST_ALIVE);
    } else {
        bc::detail::camp_state cs{};
        alive_t = bc::detail::sdk_camp(h, &cs) == 0;
    }
    if (alive_t) try_reset_attack(h, champ);
    return true;
}

bool Bot::cast_on_camp(const Camp& c) {
    if (!(st & ST_CAST)) return false;
    const int d = cheb(pos, c.pos);
    if (kit.single.slot >= 0 && kit.single.mod > 0 && d <= static_cast<int>(kit.single.range) &&
        cooldown(kit.single.slot) == 0) {
        if (raw_cast(kit.single.slot, 2, c.handle, {-1, -1}) == 1) {
            st &= ~ST_CAST;
            if (kit.single.excl) cast_excl = true;
            return true;
        }
    }
    if (kit.aoe.slot >= 0 && d <= static_cast<int>(kit.aoe.range) && cooldown(kit.aoe.slot) == 0) {
        if (raw_cast(kit.aoe.slot, 0, 0, c.pos) == 1) {
            st &= ~ST_CAST;
            if (kit.aoe.excl) cast_excl = true;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- combat vs champions

bool Bot::combat() {
    if (enemies.empty()) return false;
    const UnitView* nearest = nullptr;
    int nd = 99;
    float e_hp = 0, e_dps = 0;
    for (auto& e : enemies) {
        const int d = cheb(pos, e.pos);
        if (d < nd) {
            nd = d;
            nearest = &e;
        }
        if (d <= 7) {
            e_hp += e.hp;
            e_dps += dps_of(e.type, e.str, e.focus);
        }
    }
    if (!nearest || nd > 7) return false;
    float a_hp = hp, a_dps = dps_of(type, str, focus);
    for (auto& a : allies)
        if (cheb(a.pos, pos) <= 6) {
            a_hp += a.hp;
            a_dps += dps_of(a.type, a.str, a.focus);
        }
    const float t_we = e_hp / std::max(1.f, a_dps);
    const float t_they = a_hp / std::max(1.f, e_dps);
    const float adv = t_they / std::max(0.1f, t_we);

    // Best target: cheapest to kill, weighted by loot.
    const UnitView* best = nullptr;
    float best_sc = -1;
    const float my_hit = str;
    for (auto& e : enemies) {
        const int d = cheb(pos, e.pos);
        if (d > 6) continue;
        if (world.enemy_spawn.is_set(e.pos)) continue;
        const float hits = e.hp / std::max(1.f, my_hit);
        const float sc = (60.f + 2.f * e.gems) / (hits + 2.f * std::max(0, d - range));
        if (sc > best_sc) {
            best_sc = sc;
            best = &e;
        }
    }
    bool killable = false;
    if (best) {
        const int d = cheb(pos, best->pos);
        float burst = str * ((kit.reset >= 0 && cooldown(kit.reset) == 0) ? 2.f : 1.f);
        if (kit.single.slot >= 0 && kit.single.mod > 0 && cooldown(kit.single.slot) == 0) burst += kit.single.mod * focus;
        killable = best->hp <= burst && d <= range + (kit.melee ? 1 : 0);
    }
    const bool low = hp < 0.3f * max_hp;
    const bool fight = best && (killable || (adv >= 1.25f && !low) || (adv >= 2.5f));
    if (fight) {
        engage(*best);
        return true;
    }
    // Disadvantaged: back off if they can reach us soon.
    bool threatened = false;
    for (auto& e : enemies)
        if (cheb(pos, e.pos) <= e.range + 2) threatened = true;
    if (threatened) {
        retreat();
        return true;
    }
    return false;
}

void Bot::engage(const UnitView& e) {
    int d = cheb(pos, e.pos);
    // Opening spell (damage/stun) if in range.
    if ((st & ST_CAST) && kit.single.slot >= 0 && d <= static_cast<int>(kit.single.range) &&
        cooldown(kit.single.slot) == 0) {
        const bool need_walk = kit.melee && d > 2;
        if (!(kit.single.excl && need_walk)) {
            if (raw_cast(kit.single.slot, 1, e.h, {-1, -1}) == 1) {
                st &= ~ST_CAST;
                if (kit.single.excl) cast_excl = true;
            }
        }
    }
    if ((st & ST_CAST) && kit.aoe.slot >= 0 && d <= static_cast<int>(kit.aoe.range) && cooldown(kit.aoe.slot) == 0 &&
        !(kit.aoe.excl && kit.melee && d > 2)) {
        if (raw_cast(kit.aoe.slot, 0, 0, e.pos) == 1) {
            st &= ~ST_CAST;
            if (kit.aoe.excl) cast_excl = true;
        }
    }
    if ((st & ST_CAST) && kit.shield >= 0 && cooldown(kit.shield) == 0 && d <= 2) {
        if (raw_cast(kit.shield, 0, 0, {-1, -1}) == 1) st &= ~ST_CAST;
    }
    if (d <= range) {
        attack_unit_handle(e.h, true);
        return;
    }
    // Close the distance.
    if (can_walk()) {
        if (d >= 3 && kit.dash >= 0 && (st & ST_CAST) && cooldown(kit.dash) == 0) {
            vec2 best = pos;
            int bd = d;
            for (int dx = -2; dx <= 2; ++dx)
                for (int dy = -2; dy <= 2; ++dy) {
                    const vec2 q{pos.x + dx, pos.y + dy};
                    if (q == pos || !in_bounds(q) || blocked.is_set(q)) continue;
                    const int dq = cheb(q, e.pos);
                    if (dq < bd) {
                        bd = dq;
                        best = q;
                    }
                }
            if (best != pos && raw_cast(kit.dash, 0, 0, best) == 1) {
                pos = best;
                st &= ~ST_CAST;
            }
        }
        step_towards(e.pos);
        d = cheb(pos, e.pos);
        if (d <= range && kit.melee) attack_unit_handle(e.h, true);
    }
}

void Bot::retreat() {
    const auto& sf = world.spawn_f();
    if (!can_walk()) return;
    vec2 best = pos;
    float best_sc = -1e9f;
    for (int i = -1; i < 8; ++i) {
        const vec2 q = i < 0 ? pos : pos + DIRS[i];
        if (i >= 0 && (!in_bounds(q) || blocked.is_set(q))) continue;
        float sc = 0;
        for (auto& e : enemies) sc += static_cast<float>(std::min(cheb(q, e.pos), 8)) * 3.f;
        const int sd = sf.get(q);
        sc -= static_cast<float>(sd == UNREACH ? 200 : sd);
        if (sc > best_sc) {
            best_sc = sc;
            best = q;
        }
    }
    if (best != pos && raw_move(best) == 1) {
        last_pos = pos;
        pos = best;
        moved = true;
        st &= ~ST_MOVE;
    }
}

// ---------------------------------------------------------------- healing

bool Bot::need_heal() const {
    if (healing) return true;
    float thr = 0.33f;
    for (auto& e : enemies)
        if (cheb(e.pos, pos) <= 6) thr = 0.45f;
    return hp < thr * max_hp;
}

void Bot::go_heal() {
    healing = true;
    if (world.ally_spawn.is_set(pos)) {
        if (hp >= 0.95f * max_hp) healing = false;
        return;
    }
    if (st & ST_RECALL) return;
    const auto& sf = world.spawn_f();
    const int sd = sf.get(pos);
    bool danger = false;
    for (auto& e : enemies)
        if (cheb(e.pos, pos) <= 5) danger = true;
    // Camps in range would interrupt a recall.
    bool camp_threat = false;
    if (!danger && sd > 5) {
        for (auto& g : world.groups) {
            if (cheb(g.center, pos) > 6 || g.str_left() <= 0) continue;
            for (auto& c : g.camps)
                if (!c.dead && c.str > 0 && cheb(c.pos, pos) <= 3) camp_threat = true;
        }
    }
    if (!danger && !camp_threat && sd > 5) {
        if (bc::detail::sdk_recall() == 0) return;
    }
    if (!try_dash_field(sf)) {}
    step_field(sf, true);
}

// ---------------------------------------------------------------- banking

bool Bot::should_bank() {
    if (gems <= 0) return false;
    const int team_total = static_cast<int>(bc::global_submitted_gems(team::ally));
    if (team_total + gems >= 1000) return true;
    const int da = world.altars_f().get(pos);
    if (da == UNREACH) return false;
    if (gems >= 60) return true;
    if (gems >= 30 && da <= 10) return true;
    if (da <= 3) return true;
    bool danger = false;
    for (auto& e : enemies)
        if (cheb(e.pos, pos) <= 8) danger = true;
    if (danger && da <= 12) return true;
    if (round > 1900) return true;
    return false;
}

void Bot::go_bank() {
    const auto& af = world.altars_f();
    const int da = af.get(pos);
    if (da <= 1) {
        for (auto a : world.altars) {
            if (cheb(a, pos) > 1) continue;
            if (bc::detail::sdk_submit_gems(a.x, a.y) == 0) return;
        }
    }
    try_dash_field(af);
    step_field(af, false);
}

// ---------------------------------------------------------------- farming

int Bot::choose_group() {
    bitboard roots;
    roots.set(pos);
    bitboard visited;
    bc::distance_field myf;
    myf.compute(world.static_block, roots, visited);
    const auto& sf = world.spawn_f();
    const float dps = std::max(10.f, my_camp_dps());
    const int pack_cur = shm[6] ? shm[6] - 1 : -1;
    int best = -1, pbest = -1;
    float best_sc = 0, pbest_sc = 0, pcur_sc = 0;
    bool pcur_valid = pack_cur >= 0 && pack_cur < static_cast<int>(world.groups.size());
    for (int gi = 0; gi < static_cast<int>(world.groups.size()); ++gi) {
        auto& g = world.groups[gi];
        if (g.monolith) continue;
        int d = UNREACH;
        for (auto& c : g.camps) {
            const int v = myf.get(c.pos);
            if (v < d) d = v;
            if (g.camps.size() > 3) break;
        }
        if (d == UNREACH) {
            if (gi == pack_cur) pcur_valid = false;
            continue;
        }
        int wait = 0;
        float hpl = g.hp_left(), strl = g.str_left();
        if (g.state == GState::respawning) {
            wait = std::max(0, g.respawn_round - round);
            hpl = g.hp_total;
            strl = g.str_total;
        } else if (g.state == GState::reward) {
            hpl = 0;
            strl = 0;
        }
        const int helpers = claims_on(gi);
        if (gi == pack_cur && ((g.state == GState::respawning && wait > 12) || (g.state == GState::reward && helpers > 0)))
            pcur_valid = false;
        if (g.state == GState::reward && helpers > 0 && gi != target) continue;
        float value = static_cast<float>(g.gems) + 0.25f * static_cast<float>(g.gold);
        if (round - g.enemy_round < 20) value *= 0.7f;
        auto score = [&](int h) {
            const float team_dps = dps * (1.f + 0.85f * static_cast<float>(h));
            const float kill_t = hpl / team_dps;
            const float dmg_me = kill_t * strl * 0.6f / (1.f + static_cast<float>(h));
            float time = static_cast<float>(std::max(d, wait)) + kill_t + 6.f;
            if (dmg_me > hp * 0.8f) {
                const int sd = sf.get(g.center);
                const float trips = std::ceil((dmg_me - hp * 0.8f) / (max_hp * 0.65f));
                time += trips * (2.f * static_cast<float>(sd == UNREACH ? 50 : sd) + 8.f);
            }
            float v = value;
            if (h > 0 && (g.str_total <= 0 || hpl < 1600)) v *= 0.3f;
            return v / time;
        };
        float sc = score(helpers);
        const bool big = g.str_total > 0 && g.hp_total >= 2000;
        if (big && n_alive > 1) {
            const float psc = score(std::max(helpers, n_alive - 1));
            if (gi == pack_cur) {
                pcur_sc = psc;
                sc = std::max(sc, psc);
            }
            if (psc > pbest_sc) {
                pbest_sc = psc;
                pbest = gi;
            }
        }
        gscore[gi] = sc;
        if (sc > best_sc) {
            best_sc = sc;
            best = gi;
        }
    }
    // The pack target only changes once it is finished or unreachable.
    if (pbest >= 0 && !pcur_valid) {
        const std::uint8_t val = static_cast<std::uint8_t>(pbest + 1);
        if (shm[6] != val) {
            shm[6] = val;
            bc::detail::sdk_write_shared_memory(6, &val, 1);
        }
        gscore[pbest] = std::max(gscore[pbest], pbest_sc);
        if (pbest_sc > best_sc) best = pbest;
    }
    (void)pcur_sc;
    return best;
}

bool Bot::target_valid(int gi) const {
    if (gi < 0) return false;
    const auto& g = world.groups[gi];
    if (g.state == GState::respawning) return g.respawn_round - round <= 6;
    if (g.state == GState::reward) return claims_on(gi) == 0 || g.is_tile(pos);
    return true;
}

bool Bot::attack_camp(Group& g) {
    // Prefer dangerous camps, then the weakest.
    Camp* best = nullptr;
    float best_sc = 1e18f;
    for (auto& c : g.camps) {
        if (c.dead || !vis.is_set(c.pos)) continue;
        if (cheb(pos, c.pos) > range) continue;
        const float sc = (c.str > 0 ? 0.f : 1e6f) + c.hp;
        if (sc < best_sc) {
            best_sc = sc;
            best = &c;
        }
    }
    if (!best) return false;
    if (enemies.empty()) cast_on_camp(*best);
    if ((st & ST_CAST) && kit.shield >= 0 && g.str_left() > 0 && hp < 0.9f * max_hp && cooldown(kit.shield) == 0) {
        if (raw_cast(kit.shield, 0, 0, {-1, -1}) == 1) st &= ~ST_CAST;
    }
    return attack_unit_handle(best->handle, false);
}

void Bot::act_group(int gi) {
    auto& g = world.groups[gi];
    const auto& f = world.group_field(gi);
    const bool all_dead = g.state == GState::reward ||
                          (g.state != GState::respawning && g.hp_left() <= 0 && g.seen_round == round);
    if (all_dead) {
        if (g.is_tile(pos)) return;
        try_dash_field(f);
        step_field(f, true);
        return;
    }
    if (g.state == GState::respawning) {
        // Wait near the group but off its spawn tiles.
        if (f.get(pos) > 2) {
            try_dash_field(f);
            step_field(f, false);
        }
        return;
    }
    if (attack_camp(g)) return;
    if (!moved) {
        try_dash_field(f);
        step_field(f, false);
    }
    if (kit.melee) attack_camp(g);
}

void Bot::farm() {
    if (gscore.size() != world.groups.size()) gscore.assign(world.groups.size(), 0.f);
    std::fill(gscore.begin(), gscore.end(), 0.f);
    int cand = choose_group();
    if (target_valid(target) && cand != target) {
        const auto& g = world.groups[target];
        const bool engaged = cheb(pos, g.center) <= 4;
        if (engaged || gscore[target] * 1.6f >= gscore[cand < 0 ? target : cand]) cand = target;
    }
    target = cand;
    claim(target);
    if (world.my_slot == 0 && round % 5 == 0) {
        LOG("R%d s%d t%d pos(%d,%d) hp=%.0f/%.0f gems=%d tgt=%d", round, world.my_slot, type, pos.x, pos.y, hp, max_hp, gems, target);
        if (target >= 0) { auto& g = world.groups[target]; LOG(" st=%d hpl=%.0f d=%d", (int)g.state, g.hp_left(), (int)world.group_field(target).get(pos)); }
        LOG(" fuel=%llu\n", (unsigned long long)bc::fuel_remaining());
    }
    if (target < 0) {
        step_field(world.spawn_f(), true);
        return;
    }
    act_group(target);
}

// ---------------------------------------------------------------- turn

void Bot::turn() {
    if (!inited) init();
    snapshot();
    if (!alive_state()) {
        healing = false;
        shop();
        claim(-1);
        return;
    }
    if (st & (ST_RECALL | ST_SUBMIT | ST_TELE)) {
        if (world.ally_spawn.is_set(pos)) shop();
        return;
    }
    observe_groups();
    predict_groups();
    const bool at_spawn = world.ally_spawn.is_set(pos);
    if (at_spawn) shop();
    if (bc::fuel_remaining() < 20000) return;

    if (combat()) return;
    if (need_heal()) {
        claim(-1);
        go_heal();
        return;
    }
    if (should_bank()) {
        go_bank();
        return;
    }
    if (bc::fuel_remaining() < 25000) return;
    farm();
}
