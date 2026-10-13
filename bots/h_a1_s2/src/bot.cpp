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
constexpr std::uint32_t M_STR = 0, M_FOC = 2, P_STR = 6, P_FOC = 8, P_HP = 10, D_HP = 16, J_HP = 28;
// Emblem families in purchase order; each entry is bought as five matching shards.
#if CFG_BUILD == 0
constexpr std::uint32_t BUILD_STR[] = {P_STR, J_HP, P_STR, D_HP, P_STR, P_HP};
constexpr std::uint32_t BUILD_TANK[] = {P_HP, P_STR, J_HP, P_STR, D_HP, P_HP};
#elif CFG_BUILD == 1
constexpr std::uint32_t BUILD_STR[] = {M_STR, J_HP, D_HP, P_STR, P_STR, P_HP};
constexpr std::uint32_t BUILD_TANK[] = {M_STR, J_HP, D_HP, P_STR, P_HP, P_STR};
#else
constexpr std::uint32_t BUILD_STR[] = {M_STR, J_HP, P_STR, D_HP, P_STR, P_HP};
constexpr std::uint32_t BUILD_TANK[] = {M_STR, J_HP, P_STR, D_HP, P_HP, P_STR};
#endif
constexpr std::uint32_t BUILD_FOC[] = {M_FOC, J_HP, P_FOC, D_HP, P_FOC, P_HP};
constexpr int BUILD_LEN = 6;
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
                    kit.dmg = {slot, bc::ability_kind::damage, v.range, v.modifier, 0, 0, v.exclusive_with_movement};
                } else if constexpr (std::is_same_v<T, bc::ability_cc>) {
                    kit.cc = {slot, bc::ability_kind::cc, v.range, v.modifier, 0, v.duration, v.exclusive_with_movement};
                } else if constexpr (std::is_same_v<T, bc::ability_aoe_damage>) {
                    kit.aoe = {slot, bc::ability_kind::aoe_damage, v.range, v.modifier, v.radius, v.duration,
                               v.exclusive_with_movement};
                }
            },
            *inf);
    }
    world.spawn_f();
    world.altars_f();
    const int want = type == static_cast<int>(bc::champion_type::cassi) ? static_cast<int>(bc::champion_type::issac)
                     : type == static_cast<int>(bc::champion_type::issac) ? static_cast<int>(bc::champion_type::cassi)
                                                                            : -1;
    if (want >= 0) {
        for (int i = 0; i < world.team_size; ++i) {
            bc::detail::champion_info pi{};
            if (bc::detail::sdk_champion(world.ally_handles[i], &pi) == 0 && pi.type == want) {
                partner_h = world.ally_handles[i];
                partner_slot = i;
            }
        }
        follower = type == static_cast<int>(bc::champion_type::cassi);
    } else if (CFG_BUDDY) {
        // Pair the remaining champions in slot order; the later slot follows.
        std::vector<int> rest;
        for (int i = 0; i < world.team_size; ++i) {
            bc::detail::champion_info pi{};
            if (bc::detail::sdk_champion(world.ally_handles[i], &pi) != 0) continue;
            if (pi.type == static_cast<int>(bc::champion_type::cassi) || pi.type == static_cast<int>(bc::champion_type::issac)) continue;
            rest.push_back(i);
        }
        for (std::size_t k = 0; k + 1 < rest.size(); k += 2) {
            if (rest[k] == world.my_slot) {
                partner_slot = rest[k + 1];
                follower = false;
            } else if (rest[k + 1] == world.my_slot) {
                partner_slot = rest[k];
                follower = true;
            }
        }
        if (partner_slot >= 0) partner_h = world.ally_handles[partner_slot];
    }
    if (CFG_SQUAD) {
        // Squad 0: Cassi & Issac (+ spare bodies); squad 1: up to three other champions.
        std::vector<int> ci, rest;
        for (int i = 0; i < world.team_size; ++i) {
            bc::detail::champion_info pi{};
            if (bc::detail::sdk_champion(world.ally_handles[i], &pi) != 0) continue;
            if (pi.type == static_cast<int>(bc::champion_type::cassi) || pi.type == static_cast<int>(bc::champion_type::issac)) ci.push_back(i);
            else rest.push_back(i);
            slot_type[i] = pi.type;
        }
        std::vector<int> s0 = ci, s1;
        if (ci.empty()) {
            for (std::size_t k = 0; k < rest.size(); ++k) (k < rest.size() / 2 ? s0 : s1).push_back(rest[k]);
        } else {
            for (std::size_t k = 0; k < rest.size(); ++k) (k < 3 ? s1 : s0).push_back(rest[k]);
        }
        for (int x : s0)
            if (x == world.my_slot) squad = s0;
        for (int x : s1)
            if (x == world.my_slot) squad = s1;
    }
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
    sh.load();
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
        std::uint8_t r = 0;
        if (bc::detail::sdk_champion_restrictions(h, &r) == 0) u.stunned = (r & 3u) == 3u;
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

void Bot::claim(int gi) { sh.set_claim(world.my_slot, gi); }

int Bot::claims_on(int gi) const {
    int n = 0;
    const int lim = std::min(world.team_size, 6);
    for (int i = 0; i < lim; ++i) {
        if (i == world.my_slot || i == partner_slot || in_squad(i)) continue;
        if (sh.claim(i) == gi) ++n;
    }
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
    const bool guard = CFG_SAFE_STEP && !enemies.empty() && enemy_danger_at(pos) <= 0;
    for (auto d : DIRS) {
        const vec2 q = pos + d;
        if (!in_bounds(q) || blocked.is_set(q)) continue;
        const int v = f.get(q);
        if (v == UNREACH || (v == 0 && !onto_root)) continue;
        if (v > cur) continue;
        if (guard && enemy_danger_at(q) > 0) continue;
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
    const bool caster = CFG_CASSI_FOCUS && type == static_cast<int>(bc::champion_type::cassi);
    const std::uint32_t* build = caster ? BUILD_FOC : (tank ? BUILD_TANK : BUILD_STR);
    int guard = 0;
    while (gold >= 30 && guard++ < 12) {
        const int fam = build_step / 5;
        const int idx = fam < BUILD_LEN ? fam : 2 + (fam - BUILD_LEN) % (BUILD_LEN - 2);
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
    for (const Spell* sp : {&kit.dmg, &kit.cc}) {
        if (sp->slot < 0 || sp->mod <= 0 || d > static_cast<int>(sp->range) || cooldown(sp->slot) != 0) continue;
        if (raw_cast(sp->slot, 2, c.handle, {-1, -1}) == 1) {
            st &= ~ST_CAST;
            if (sp->excl) cast_excl = true;
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

namespace {
bool enemy_can_act(const UnitView& e) { return !e.stunned && !(e.state & (ST_RECALL | ST_SUBMIT | ST_TELE)); }
}  // namespace

float Bot::danger_at(vec2 t) const { return enemy_danger_at(t) + camp_threat_at(t) * 0.5f; }

float Bot::enemy_danger_at(vec2 t) const {
    float d = 0;
    for (auto& e : enemies) {
        if (!enemy_can_act(e)) continue;
        const int dist = cheb(t, e.pos);
        const bool emelee = e.range <= 1;
        const float dps = dps_of(e.type, e.str, e.focus);
        if (emelee ? dist <= 2 : dist <= e.range) d += dps;
        else if (dist <= 4 && TYPES[e.type < 0 || e.type > 11 ? 0 : e.type].spell_mod > 0) d += 0.4f * dps;
    }
    return d;
}

bool Bot::combat() {
    if (enemies.empty()) return false;
    float e_hp = 0, e_dps = 0;
    int near = 0;
    for (auto& e : enemies) {
        const int d = cheb(pos, e.pos);
        if (world.enemy_spawn.is_set(e.pos) && d > e.range + 1) continue;
        if (d > 8) continue;
        ++near;
        e_hp += e.hp;
        if (enemy_can_act(e)) e_dps += dps_of(e.type, e.str, e.focus);
    }
    if (near == 0) return false;
    float a_hp = hp, a_dps = dps_of(type, str, focus);
    for (auto& a : allies)
        if (cheb(a.pos, pos) <= 7) {
            a_hp += a.hp;
            a_dps += dps_of(a.type, a.str, a.focus);
        }
    const float t_we = e_hp / std::max(1.f, a_dps);
    const float t_they = a_hp / std::max(1.f, e_dps);
    const float adv = t_they / std::max(0.1f, t_we);

    // Focus target: cheap to kill, carrying loot, or helpless while channeling.
    const int shared_focus = sh.focus();
    const UnitView* best = nullptr;
    float best_sc = -1;
    float burst = str * ((kit.reset >= 0 && cooldown(kit.reset) == 0) ? 2.f : 1.f);
    if (kit.dmg.slot >= 0 && cooldown(kit.dmg.slot) == 0) burst += kit.dmg.mod * focus;
    else if (kit.cc.slot >= 0 && kit.cc.mod > 0 && cooldown(kit.cc.slot) == 0) burst += kit.cc.mod * focus;
    for (auto& e : enemies) {
        const int d = cheb(pos, e.pos);
        if (d > 7 || world.enemy_spawn.is_set(e.pos)) continue;
        if (CFG_TERRITORY && cheb(e.pos, world.enemy_center) <= P_TERRITORY &&
            !(e.hp <= burst && d <= range + (kit.melee ? 1 : 0)))
            continue;
        const float hits = e.hp / std::max(1.f, str);
        float sc = (60.f + 2.5f * static_cast<float>(e.gems)) / (hits + 1.5f * static_cast<float>(std::max(0, d - range)));
        if (!enemy_can_act(e)) sc *= 1.6f;
        // Cassi and Issac die together: either kill removes two bodies.
        if (CFG_CI_FOCUS && (e.type == static_cast<int>(bc::champion_type::cassi) || e.type == static_cast<int>(bc::champion_type::issac))) sc *= 1.8f;
        if (e.hp <= burst && d <= range + (kit.melee ? 1 : 0)) sc *= 3.f;
        int ei = -1;
        for (int i = 0; i < static_cast<int>(world.enemy_handles.size()); ++i)
            if (world.enemy_handles[i] == e.h) ei = i;
        if (ei == shared_focus) sc *= 1.4f;
        if (sc > best_sc) {
            best_sc = sc;
            best = &e;
        }
    }
    bool killable = false, helpless = false;
    if (best) {
        const int d = cheb(pos, best->pos);
        killable = best->hp <= burst && d <= range + (kit.melee ? 1 : 0);
        helpless = !enemy_can_act(*best) && d <= range + 2;
    }
    const bool linked = CFG_LINKED_CAUTION && (type == static_cast<int>(bc::champion_type::cassi) || type == static_cast<int>(bc::champion_type::issac));
    const bool low = hp < (linked ? 0.4f : 0.3f) * max_hp || (CFG_LOWHP && hp < P_LOWHP * max_hp);
    const bool rich = gems >= 40;
    float need = rich ? 1.5f * P_ENGAGE : (linked ? 1.12f * P_ENGAGE : P_ENGAGE);
    if (low) need = 2.5f;
    bool fight = best && (killable || adv >= need || (helpless && adv >= 0.7f));
    // An ally committed to a fight next to us: join it instead of fleeing piecemeal.
    const bool call = CFG_CALL && sh.has_rally() && sh.rally_age(round) <= 2 && cheb(pos, sh.rally()) <= 6;
    if (call) {
        const int fi = sh.focus();
        if (fi >= 0 && fi < static_cast<int>(world.enemy_handles.size()))
            for (auto& e : enemies)
                if (e.h == world.enemy_handles[fi] && cheb(pos, e.pos) <= 7 && !world.enemy_spawn.is_set(e.pos)) best = &e;
    }
    bool isolated_prey = false;
    if (CFG_HUNT && best) {
        isolated_prey = true;
        for (auto& e : enemies)
            if (e.h != best->h && cheb(e.pos, best->pos) <= 4) isolated_prey = false;
        int mates = 0;
        for (auto& a : allies)
            if (cheb(a.pos, pos) <= 4) ++mates;
        if (mates == 0 && hp < 0.7f * max_hp) isolated_prey = false;
    }
    if (CFG_SIM && best && !killable) {
        const float v = sim_fight();
        float margin = P_SIM_MARGIN + (rich ? 0.5f : 0.f) + (low ? 0.8f : 0.f);
        if (call) margin -= P_CALL_BONUS;
        if (isolated_prey) margin -= 0.6f;
        fight = v > margin || (helpless && v > -0.3f);
    }
    if (fight) {
        int ei = -1;
        for (int i = 0; i < static_cast<int>(world.enemy_handles.size()); ++i)
            if (world.enemy_handles[i] == best->h) ei = i;
        if (ei >= 0) sh.set_focus(ei);
        sh.set_rally(pos, round);
        // A stalled engagement (nothing reachable or allowed) must not freeze the champion.
        if (engage(*best)) return true;
        return false;
    }
    bool rich_threat = false;
    for (auto& e : enemies)
        if (rich && enemy_can_act(e) && cheb(pos, e.pos) <= 5 && !world.enemy_spawn.is_set(e.pos)) rich_threat = true;
    bool early = false;
    if (CFG_SAFETY) {
        int close = 0;
        for (auto& e : enemies)
            if (enemy_can_act(e) && cheb(pos, e.pos) <= P_EARLY_D && !world.enemy_spawn.is_set(e.pos)) ++close;
        int friends = 0;
        for (auto& a : allies)
            if (cheb(a.pos, pos) <= 4) ++friends;
        early = close >= 2 || (close == 1 && friends == 0 && hp < 0.6f * max_hp);
    }
    if (enemy_danger_at(pos) > 0 || rich_threat || early) {
        // A same-speed melee chaser gets free hits on a fleeing unit; hit back unless escape is real.
        const UnitView* pin = nullptr;
        for (auto& e : enemies)
            if (enemy_can_act(e) && e.range <= 1 && cheb(pos, e.pos) <= 2 && !world.enemy_spawn.is_set(e.pos)) {
                if (!pin || e.hp < pin->hp) pin = &e;
            }
        const int sd = world.spawn_f().get(pos);
        const bool can_escape = sd <= 3 || (kit.dash >= 0 && cooldown(kit.dash) == 0 && (st & ST_CAST));
        bool duel_ok = true;
        if (CFG_DUEL && pin) {
            // Hit back only if we out-trade the pinning enemy; otherwise run toward help.
            const float my_t = pin->hp / std::max(1.f, dps_of(type, str, focus));
            const float his_t = hp / std::max(1.f, dps_of(pin->type, pin->str, pin->focus));
            int mates = 0;
            for (auto& a : allies)
                if (cheb(a.pos, pos) <= 4) ++mates;
            duel_ok = mates > 0 || my_t <= his_t * 1.1f;
        }
        if (pin && !can_escape && duel_ok) {
            sh.set_rally(pos, round);
            const UnitView* tgt = (best && cheb(pos, best->pos) <= range + (kit.melee ? 1 : 0)) ? best : pin;
            if (engage(*tgt)) return true;
        }
        retreat();
        return true;
    }
    return false;
}

float Bot::sim_fight() const {
    struct U {
        float hp, dps, val;
        bool me;
    };
    auto is_ci = [](int t) {
        return t == static_cast<int>(bc::champion_type::cassi) || t == static_cast<int>(bc::champion_type::issac);
    };
    U A[8], E[8];
    int na = 0, ne = 0;
    A[na++] = {hp, dps_of(type, str, focus), is_ci(type) ? 2.f : 1.f, true};
    for (auto& a : allies)
        if (na < 8 && cheb(a.pos, pos) <= 7) A[na++] = {a.hp, dps_of(a.type, a.str, a.focus), is_ci(a.type) ? 2.f : 1.f, false};
    for (auto& e : enemies) {
        const int d = cheb(pos, e.pos);
        if (ne >= 8 || d > 8 || (world.enemy_spawn.is_set(e.pos) && d > e.range + 1)) continue;
        E[ne++] = {e.hp, dps_of(e.type, e.str, e.focus), is_ci(e.type) ? 2.f : 1.f, false};
    }
    if (ne == 0) return 1.f;
    float a_start = 0, e_start = 0;
    for (int i = 0; i < na; ++i) a_start += A[i].hp;
    for (int i = 0; i < ne; ++i) e_start += E[i].hp;
    auto hit = [](U* side, int n, float dmg) {
        while (dmg > 0) {
            int t = -1;
            for (int i = 0; i < n; ++i)
                if (side[i].hp > 0 && (t < 0 || side[i].hp < side[t].hp)) t = i;
            if (t < 0) return;
            const float d = std::min(dmg, side[t].hp);
            side[t].hp -= d;
            dmg -= d;
        }
    };
    for (int r = 0; r < 25; ++r) {
        float da = 0, de = 0;
        for (int i = 0; i < na; ++i)
            if (A[i].hp > 0) da += A[i].dps;
        for (int i = 0; i < ne; ++i)
            if (E[i].hp > 0) de += E[i].dps;
        if (da <= 0 || de <= 0) break;
        hit(E, ne, da);
        hit(A, na, de);
    }
    float v = 0, a_end = 0, e_end = 0;
    for (int i = 0; i < ne; ++i) {
        if (E[i].hp <= 0) v += E[i].val;
        e_end += std::max(0.f, E[i].hp);
    }
    for (int i = 0; i < na; ++i) {
        if (A[i].hp <= 0) v -= A[i].val + (A[i].me ? 0.5f : 0.f);
        a_end += std::max(0.f, A[i].hp);
    }
    v += 0.5f * ((e_start - e_end) / std::max(1.f, e_start) - (a_start - a_end) / std::max(1.f, a_start));
    return v;
}

bool Bot::engage(const UnitView& e) {
    const std::uint32_t st0 = st;
    const vec2 p0 = pos;
    engage_inner(e);
    return st != st0 || pos != p0;
}

void Bot::engage_inner(const UnitView& e) {
    int d = cheb(pos, e.pos);
    const bool e_flee_risk = e.hp < 0.35f * e.max_hp;
    // Stun first when it stops damage, a channel or an escape.
    for (const Spell* sp : {&kit.cc, &kit.dmg}) {
        if (!(st & ST_CAST) || sp->slot < 0 || d > static_cast<int>(sp->range) || cooldown(sp->slot) != 0) continue;
        if (sp == &kit.cc && e.stunned) continue;
        const bool need_walk = kit.melee && d > 1;
        if (sp->excl && need_walk) continue;
        if (raw_cast(sp->slot, 1, e.h, {-1, -1}) == 1) {
            st &= ~ST_CAST;
            if (sp->excl) cast_excl = true;
        }
    }
    if ((st & ST_CAST) && kit.aoe.slot >= 0 && d <= static_cast<int>(kit.aoe.range) && cooldown(kit.aoe.slot) == 0 &&
        !(kit.aoe.excl && kit.melee && d > 1)) {
        if (raw_cast(kit.aoe.slot, 0, 0, e.pos) == 1) {
            st &= ~ST_CAST;
            if (kit.aoe.excl) cast_excl = true;
        }
    }
    if ((st & ST_CAST) && kit.shield >= 0 && cooldown(kit.shield) == 0 && danger_at(pos) > 0) {
        if (raw_cast(kit.shield, 0, 0, {-1, -1}) == 1) st &= ~ST_CAST;
    }
    if (d <= range && !(kit.melee && false)) {
        // Melee: reposition to a safer adjacent tile only if it keeps the target in reach.
        attack_unit_handle(e.h, true);
        if (kit.melee && can_walk()) {
            // Stay on target; stepping costs nothing for melee but keep adjacent.
        }
        return;
    }
    // Ranged with another enemy already in range: shoot it rather than walking in.
    if (!kit.melee) {
        for (auto& o : enemies)
            if (o.h != e.h && cheb(pos, o.pos) <= range && !world.enemy_spawn.is_set(o.pos)) {
                attack_unit_handle(o.h, true);
                return;
            }
    }
    if (!can_walk()) return;
    if (d >= 3 && kit.dash >= 0 && (st & ST_CAST) && cooldown(kit.dash) == 0 && (kit.melee || e_flee_risk)) {
        vec2 bestp = pos;
        int bd = d;
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = -2; dy <= 2; ++dy) {
                const vec2 q{pos.x + dx, pos.y + dy};
                if (q == pos || !in_bounds(q) || blocked.is_set(q)) continue;
                const int dq = cheb(q, e.pos);
                if (dq < bd) {
                    bd = dq;
                    bestp = q;
                }
            }
        if (bestp != pos && raw_cast(kit.dash, 0, 0, bestp) == 1) {
            pos = bestp;
            st &= ~ST_CAST;
        }
    }
    // Melee spacing: stepping from 3 to 2 hands an enemy melee the first strike.
    if (CFG_SPACING && kit.melee && cheb(pos, e.pos) == 3 && e.range <= 1 && enemy_can_act(e) && e.hp > 0.35f * e.max_hp) {
        bool ally_engaged = false;
        for (auto& a : allies)
            if (cheb(a.pos, e.pos) <= 1) ally_engaged = true;
        if (!ally_engaged) return;
    }
    // Step to the reachable tile closest to the target, breaking ties by danger.
    vec2 bestq = pos;
    float best_sc = static_cast<float>(cheb(pos, e.pos)) * 100.f + danger_at(pos);
    const bool finish = e.hp < 0.35f * e.max_hp;
    for (auto dd : DIRS) {
        const vec2 q = pos + dd;
        if (!in_bounds(q) || blocked.is_set(q)) continue;
        if (CFG_NO_SOLO && !finish && enemy_danger_at(q) > 0 && enemy_danger_at(pos) <= 0) {
            // Never be the first one to step into their reach.
            bool cover = false;
            for (auto& a : allies)
                if (cheb(a.pos, q) <= 2) cover = true;
            if (!cover) continue;
        }
        const float sc = static_cast<float>(cheb(q, e.pos)) * 100.f + danger_at(q);
        if (sc < best_sc) {
            best_sc = sc;
            bestq = q;
        }
    }
    if (bestq != pos && raw_move(bestq) == 1) {
        last_pos = pos;
        pos = bestq;
        moved = true;
        st &= ~ST_MOVE;
    }
    d = cheb(pos, e.pos);
    if (d <= range && kit.melee) attack_unit_handle(e.h, true);
}

void Bot::retreat() {
    const auto& sf = world.spawn_f();
    // A stun on the closest threat buys a turn.
    if ((st & ST_CAST) && kit.cc.slot >= 0 && !kit.cc.excl) {
        for (auto& e : enemies)
            if (enemy_can_act(e) && cheb(pos, e.pos) <= static_cast<int>(kit.cc.range) && cooldown(kit.cc.slot) == 0) {
                if (raw_cast(kit.cc.slot, 1, e.h, {-1, -1}) == 1) st &= ~ST_CAST;
                break;
            }
    }
    if (!can_walk()) return;
    const UnitView* buddy = nullptr;
    if (CFG_SAFETY) {
        int bd = 99;
        for (auto& a : allies) {
            const int d = cheb(a.pos, pos);
            if (d < bd && d <= 10 && a.hp > 0.4f * a.max_hp) {
                bd = d;
                buddy = &a;
            }
        }
    }
    auto score = [&](vec2 q) {
        const int sd = sf.get(q);
        float sc = danger_at(q) * 10.f + static_cast<float>(sd == UNREACH ? 300 : sd) * 0.5f;
        if (buddy) sc += static_cast<float>(cheb(q, buddy->pos)) * 0.8f;
        return sc;
    };
    if (kit.dash >= 0 && (st & ST_CAST) && cooldown(kit.dash) == 0 && danger_at(pos) > 0) {
        vec2 bestp = pos;
        float bs = score(pos);
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = -2; dy <= 2; ++dy) {
                if (iabs(dx) < 2 && iabs(dy) < 2) continue;
                const vec2 q{pos.x + dx, pos.y + dy};
                if (!in_bounds(q) || blocked.is_set(q)) continue;
                const float sc = score(q);
                if (sc < bs) {
                    bs = sc;
                    bestp = q;
                }
            }
        if (bestp != pos && raw_cast(kit.dash, 0, 0, bestp) == 1) {
            pos = bestp;
            st &= ~ST_CAST;
        }
    }
    vec2 best = pos;
    float best_sc = score(pos);
    for (auto d : DIRS) {
        const vec2 q = pos + d;
        if (!in_bounds(q) || blocked.is_set(q)) continue;
        const float sc = score(q);
        if (sc < best_sc) {
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
    // Ranged units that could not move still shoot back.
    if (!moved && (st & ST_ATTACK)) {
        for (auto& e : enemies)
            if (cheb(pos, e.pos) <= range) {
                attack_unit_handle(e.h, true);
                break;
            }
    }
}

bool Bot::shop_trip() {
    if (type == static_cast<int>(bc::champion_type::tavi) || round > 1500) return false;
    if (world.ally_spawn.is_set(pos)) return false;
    const int sd = world.spawn_f().get(pos);
    if (sd == UNREACH) return false;
    // A shard is worth a short detour early; recall covers long ones.
    const int need = P_SHOP_BASE + (sd > 6 ? 60 : sd * 10);
    if (gold < need) return false;
    if (gems >= 20) return false;
    for (auto& e : enemies)
        if (cheb(e.pos, pos) <= 8) return false;
    if (sd > 6 && !camp_near) {
        if (bc::detail::sdk_recall() == 0) return true;
    }
    try_dash_field(world.spawn_f());
    step_field(world.spawn_f(), true);
    return true;
}

bool Bot::answer_rally() {
    if (!sh.has_rally() || sh.rally_age(round) > 3) return false;
    const vec2 r = sh.rally();
    const int d = cheb(pos, r);
    if (d <= 2 || d > 14 || gems >= 40 || hp < 0.55f * max_hp) return false;
    if (!can_walk()) return true;
    bitboard roots;
    roots.set(r);
    bitboard visited;
    bc::distance_field rf;
    rf.compute(world.static_block, roots, visited, 24);
    if (rf.get(pos) > 20) return false;
    try_dash_field(rf);
    step_field(rf, false);
    return true;
}

bool Bot::combat_old() {
    if (enemies.empty()) return false;
    const UnitView* nearest = nullptr;
    int nd = 99;
    float e_hp = 0, e_dps = 0;
    for (auto& e : enemies) {
        const int d = cheb(pos, e.pos);
        if (world.enemy_spawn.is_set(e.pos) && d > e.range + 1) continue;
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
        if (kit.dmg.slot >= 0 && cooldown(kit.dmg.slot) == 0) burst += kit.dmg.mod * focus;
        killable = best->hp <= burst && d <= range + (kit.melee ? 1 : 0);
    }
    const bool low = hp < 0.3f * max_hp;
    const bool fight = best && (killable || (adv >= 1.25f && !low) || (adv >= 2.5f));
    if (fight) {
        engage_old(*best);
        return true;
    }
    // Disadvantaged: back off if they can reach us soon.
    bool threatened = false;
    for (auto& e : enemies) {
        const int lim = world.enemy_spawn.is_set(e.pos) ? e.range + 1 : e.range + 2;
        if (cheb(pos, e.pos) <= lim) threatened = true;
    }
    if (threatened) {
        retreat_old();
        return true;
    }
    return false;
}

void Bot::engage_old(const UnitView& e) {
    int d = cheb(pos, e.pos);
    // Opening spell (damage/stun) if in range.
    for (const Spell* sp : {&kit.cc, &kit.dmg}) {
        if (!(st & ST_CAST) || sp->slot < 0 || d > static_cast<int>(sp->range) || cooldown(sp->slot) != 0) continue;
        const bool need_walk = kit.melee && d > 2;
        if (sp->excl && need_walk) continue;
        if (raw_cast(sp->slot, 1, e.h, {-1, -1}) == 1) {
            st &= ~ST_CAST;
            if (sp->excl) cast_excl = true;
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

void Bot::retreat_old() {
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

float Bot::camp_threat_at(vec2 q) const {
    float t = 0;
    for (const Camp* c : near_camps)
        if (cheb(c->pos, q) <= 3) t += c->str;
    return t;
}

void Bot::assess_camps() {
    camp_in = 0;
    camp_near = false;
    near_camps.clear();
    for (auto& g : world.groups) {
        if (cheb(g.center, pos) > 9 || g.state == GState::respawning || g.state == GState::reward) continue;
        for (auto& c : g.camps) {
            if (c.dead || c.str <= 0 || c.hp >= c.max_hp) continue;
            // Only damaged camps are awake; untouched ones do not attack.
            if (cheb(c.pos, pos) <= 5) near_camps.push_back(&c);
            if (cheb(c.pos, pos) > 3) continue;
            camp_near = true;
            // Monsters hit the highest-strength champion in their range.
            bool top = true;
            for (auto& a : allies)
                if (a.str > str && cheb(a.pos, c.pos) <= 3) top = false;
            for (auto& e : enemies)
                if (e.str > str && cheb(e.pos, c.pos) <= 3) top = false;
            if (top) camp_in += c.str;
        }
    }
}

int Bot::threat_level() const {
    int n = 0;
    for (auto& m : world.enemy_mem)
        if (round - m.round <= 10 && cheb(m.pos, pos) <= 10) ++n;
    if (CFG_HIDDEN && round - hidden_round <= 6 && cheb(hidden_pos, pos) <= 8) n += 2;
    return n;
}

bool Bot::need_heal() const {
    if (healing) return true;
    if (camp_in > 0 && hp <= camp_in * 2.5f + 20.f) return true;
    float thr = P_HEAL;
    for (auto& e : enemies)
        if (cheb(e.pos, pos) <= 6) thr = 0.45f;
    if (CFG_SAFETY && CFG_THREAT_HEAL) {
        const int t = threat_level();
        if (t >= 2) thr = std::max(thr, 0.6f);
        else if (t == 1) thr = std::max(thr, 0.5f);
    }
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
        if (cheb(e.pos, pos) <= (CFG_SAFE_RECALL ? 10 : 5)) danger = true;
    if (CFG_SAFE_RECALL && threat_level() > 0) danger = true;
    if (camp_near && can_walk()) {
        // Leave monster range first; recall would be interrupted.
        vec2 best = pos;
        float best_sc = camp_threat_at(pos) * 100.f + static_cast<float>(sd);
        for (auto d : DIRS) {
            const vec2 q = pos + d;
            if (!in_bounds(q) || blocked.is_set(q)) continue;
            const int v = sf.get(q);
            const float sc = camp_threat_at(q) * 100.f + static_cast<float>(v == UNREACH ? 500 : v);
            if (sc < best_sc) {
                best_sc = sc;
                best = q;
            }
        }
        int rm = best != pos ? raw_move(best) : 9;
        if (BEAST_LOG && hp < 0.25f * max_hp) LOG("ESC R%d sd=%d best(%d,%d) sc=%.0f rm=%d thr=%.0f\n", round, sd, best.x, best.y, best_sc, rm, camp_threat_at(pos));
        if (rm == 1) {
            last_pos = pos;
            pos = best;
            moved = true;
            st &= ~ST_MOVE;
        }
        return;
    }
    if (!danger && sd > 5) {
        if (bc::detail::sdk_recall() == 0) return;
    }
    try_dash_field(sf);
    step_field(sf, true);
}

// ---------------------------------------------------------------- banking

bool Bot::should_bank() {
    if (gems <= 0) return false;
    const int team_total = static_cast<int>(bc::global_submitted_gems(team::ally));
    if (team_total + gems >= 1000) return true;
    const int da = world.altars_f().get(pos);
    if (da == UNREACH) return false;
    if (gems >= P_BANK_MAX) return true;
    if (gems >= P_BANK_MID && da <= P_BANK_MID_D) return true;
    if (da <= P_BANK_NEAR_D) return true;
    if (CFG_BANK2) {
        if (gems >= 20 && da <= 8) return true;
        // Late in the race only banked gems count: cash in whatever we carry.
        const int enemy_total = static_cast<int>(bc::global_submitted_gems(team::enemy));
        if ((team_total >= P_ENDGAME || enemy_total >= P_ENDGAME) && gems >= 10 && da <= 20) return true;
    }
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
    const int pack_cur = sh.pack();
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
        const int other_claims = claims_on(gi);
        const int helpers = other_claims + (CFG_SQUAD ? squad_alive - 1 : 0) + (CFG_PAIR == 3 && !follower && partner_slot >= 0 ? 1 : 0);
        if (CFG_SQUAD && other_claims > 0 && gi != target && g.str_total <= 0) continue;
        if (gi == pack_cur && ((g.state == GState::respawning && wait > 12) || (g.state == GState::reward && helpers > 0)))
            pcur_valid = false;
        if (g.state == GState::reward && !g.is_tile(pos) && (ally_capturing(g) || (helpers > 0 && gi != target))) continue;
        const float gold_w = round < 150 ? P_GOLD_W1 : (round < 400 ? P_GOLD_W2 : P_GOLD_W3);
        float value = static_cast<float>(g.gems) + gold_w * static_cast<float>(g.gold);
        if (P_CONTEST_PEN < 1.f && round - g.enemy_round < 20) value *= P_CONTEST_PEN;
        auto score = [&](int h) {
            const float team_dps = dps * (1.f + 0.85f * static_cast<float>(h));
            const float kill_t = hpl / team_dps;
            const float dmg_me = kill_t * strl * 0.6f / (1.f + static_cast<float>(h));
            float time = static_cast<float>(std::max(d, wait)) + kill_t + P_CAPT_T;
            if (dmg_me > hp * 0.8f) {
                const int sd = sf.get(g.center);
                const float trips = std::ceil((dmg_me - hp * 0.8f) / (max_hp * 0.65f));
                time += trips * (2.f * static_cast<float>(sd == UNREACH ? 50 : sd) + 8.f);
            }
            // Score per champion: helpers spend their time here too.
            float v = value / (1.f + static_cast<float>(h));
            if (h > 0 && (g.str_total <= 0 || hpl < 1600)) v *= P_DOGPILE;
            if (CFG_SQUAD) v = value * (other_claims > 0 ? 0.3f : 1.f);
            return v / time;
        };
        float sc = score(helpers);
        const bool big = g.str_total > 0 && g.hp_total >= 2000;
        if (!CFG_SQUAD && big && n_alive > 1) {
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
        if (CFG_COHESION && n_alive > 1) {
            // Prefer camps close to where teammates are working: support arrives in a few turns.
            int dc = 99;
            const int lim = std::min(world.team_size, 6);
            for (int i = 0; i < lim; ++i) {
                if (i == world.my_slot) continue;
                const int cg = sh.claim(i);
                if (cg < 0 || cg >= static_cast<int>(world.groups.size())) continue;
                dc = std::min(dc, cheb(world.groups[cg].center, g.center));
            }
            if (dc < 99) sc /= 1.f + P_COHESION * static_cast<float>(std::max(0, dc - 6)) / 10.f;
        }
        sc *= seed_jitter(gi, world.my_slot, 0.06f);
        gscore[gi] = sc;
        if (sc > best_sc) {
            best_sc = sc;
            best = gi;
        }
    }
    // The pack target only changes once it is finished or unreachable.
    if (pbest >= 0 && !pcur_valid) {
        sh.set_pack(pbest);
        gscore[pbest] = std::max(gscore[pbest], pbest_sc);
        if (pbest_sc > best_sc) best = pbest;
    }
    (void)pcur_sc;
    return best;
}

bool Bot::ally_capturing(const Group& g) const {
    for (auto& a : allies)
        if (g.is_tile(a.pos)) return true;
    return false;
}

bool Bot::target_valid(int gi) const {
    if (gi < 0) return false;
    const auto& g = world.groups[gi];
    if (g.state == GState::respawning) return g.respawn_round - round <= 6;
    if (g.state == GState::reward || (g.hp_left() <= 0 && g.seen_round == round)) {
        if (g.is_tile(pos)) return true;
        return !ally_capturing(g);
    }
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
    if (CFG_CAMP_STUN && (st & ST_CAST) && kit.cc.slot >= 0 && cooldown(kit.cc.slot) == 0 && (!kit.cc.excl || moved == false)) {
        // Stun the strongest awake monster in reach: it skips its next attack on us or an ally.
        Camp* tgt = nullptr;
        for (auto& c : g.camps) {
            if (c.dead || c.str < P_STUN_MIN_STR || c.hp >= c.max_hp || !vis.is_set(c.pos)) continue;
            if (cheb(pos, c.pos) > static_cast<int>(kit.cc.range)) continue;
            if (!tgt || c.str > tgt->str) tgt = &c;
        }
        const bool dmg_ready = kit.cc.mod <= 0 && kit.dmg.slot >= 0 && cooldown(kit.dmg.slot) == 0;
        if (tgt && enemies.empty() && !dmg_ready) {
            if (raw_cast(kit.cc.slot, 2, tgt->handle, {-1, -1}) == 1) {
                st &= ~ST_CAST;
                if (kit.cc.excl) cast_excl = true;
            }
        }
    }
    if (enemies.empty()) cast_on_camp(*best);
    if ((st & ST_CAST) && kit.shield >= 0 && g.str_left() > 0 && hp < 0.9f * max_hp && cooldown(kit.shield) == 0) {
        if (raw_cast(kit.shield, 0, 0, {-1, -1}) == 1) st &= ~ST_CAST;
    }
    return attack_unit_handle(best->handle, false);
}

bool Bot::opportunistic_attack(int skip_gi) {
    if (!(st & ST_ATTACK)) return false;
    Camp* best = nullptr;
    float best_sc = 1e18f;
    for (int gi = 0; gi < static_cast<int>(world.groups.size()); ++gi) {
        if (gi == skip_gi) continue;
        auto& g = world.groups[gi];
        if (cheb(g.center, pos) > range + 3 || g.state == GState::respawning || g.state == GState::reward) continue;
        for (auto& c : g.camps) {
            if (c.dead || cheb(pos, c.pos) > range || !vis.is_set(c.pos)) continue;
            // Never wake a dangerous camp just for a free hit.
            if (c.str > 0 && c.hp >= c.max_hp) continue;
            if (c.str > 0 && hp < 0.6f * max_hp) continue;
            const float sc = c.hp + (c.str > 0 ? 5000.f : 0.f);
            if (sc < best_sc) {
                best_sc = sc;
                best = &c;
            }
        }
    }
    if (!best) return false;
    return attack_unit_handle(best->handle, false);
}

void Bot::act_group(int gi) {
    auto& g = world.groups[gi];
    const auto& f = world.group_field(gi);
    const bool all_dead = g.state == GState::reward ||
                          (g.state != GState::respawning && g.hp_left() <= 0 && g.seen_round == round);
    if (all_dead) {
        act = A_F_CAPT;
        if (!g.is_tile(pos)) {
            try_dash_field(f);
            step_field(f, true);
        }
        opportunistic_attack(gi);
        return;
    }
    if (g.state == GState::respawning) {
        act = A_F_WAIT;
        const int wait = g.respawn_round - round;
        // Spend idle respawn time shopping if the round trip fits.
        const int sd = world.spawn_f().get(pos);
        if (gold >= 90 && sd != UNREACH && 2 * sd + 2 <= wait && type != static_cast<int>(bc::champion_type::tavi)) {
            try_dash_field(world.spawn_f());
            step_field(world.spawn_f(), true);
            return;
        }
        if (f.get(pos) > 2) {
            try_dash_field(f);
            step_field(f, false);
        }
        opportunistic_attack(gi);
        return;
    }
    act = A_F_ATK;
    if (attack_camp(g)) return;
    act = A_F_TRAV;
    if (!moved) {
        if (f.get(pos) <= 6) {
            // Close in on living camps only; dead spawn tiles are not targets.
            bitboard roots;
            for (auto& c : g.camps)
                if (!c.dead) roots.set(c.pos);
            if (!roots.is_empty()) {
                bitboard visited;
                bc::distance_field lf;
                lf.compute(world.static_block, roots, visited, 14);
                if (kit.melee || lf.get(pos) > static_cast<std::uint16_t>(range)) step_field(lf, false);
            }
        } else {
            try_dash_field(f);
            step_field(f, false);
        }
    }
    if (kit.melee && attack_camp(g)) {
        act = A_F_ATK;
        return;
    }
    if (kit.melee || !moved) opportunistic_attack(gi);
}

bool Bot::follow_partner() {
    if (!CFG_PAIR || !follower || partner_slot < 0) return false;
    if (CFG_PAIR == 2) {
        bool threat = false;
        for (auto& m : world.enemy_mem)
            if (round - m.round <= 25 && cheb(m.pos, pos) <= 16) threat = true;
        if (!threat) return false;
    }
    if (CFG_PAIR == 3) {
        const int lt = sh.claim(partner_slot);
        if (lt < 0 || lt >= static_cast<int>(world.groups.size()) || !target_valid(lt)) return false;
        target = lt;
        claim(lt);
        act_group(lt);
        return true;
    }
    const UnitView* p = nullptr;
    for (auto& a : allies)
        if (a.h == partner_h) p = &a;
    const int pt = sh.claim(partner_slot);
    if (pt >= 0 && pt < static_cast<int>(world.groups.size())) {
        target = pt;
        claim(pt);
        const auto& g = world.groups[pt];
        // Fight alongside Issac at his camp; otherwise keep close to him.
        if (p && cheb(p->pos, g.center) > 6 && cheb(pos, p->pos) > 2) {
            act = A_F_TRAV;
            step_towards(p->pos);
            opportunistic_attack(-1);
            return true;
        }
        act_group(pt);
        return true;
    }
    if (p) {
        act = A_F_TRAV;
        if (cheb(pos, p->pos) > 2) step_towards(p->pos);
        opportunistic_attack(-1);
        return true;
    }
    return false;
}

int Bot::squad_leader() const {
    int lead = -1;
    for (int x : squad) {
        std::int32_t a = 0;
        if (bc::detail::sdk_champion_is_alive(world.ally_handles[x], &a) != 0 || !a) continue;
        if (slot_type[x] == static_cast<int>(bc::champion_type::cassi) && squad.size() > 1) continue;
        if (lead < 0 || x < lead) lead = x;
    }
    return lead < 0 ? world.my_slot : lead;
}

bool Bot::follow_squad() {
    if (!CFG_SQUAD || squad.size() < 2) return false;
    squad_alive = 0;
    for (int x : squad) {
        std::int32_t a = 0;
        if (bc::detail::sdk_champion_is_alive(world.ally_handles[x], &a) == 0 && a) ++squad_alive;
    }
    const int lead = squad_leader();
    if (lead == world.my_slot) return false;
    const int lt = sh.claim(lead);
    if (lt < 0 || lt >= static_cast<int>(world.groups.size()) || !target_valid(lt)) return false;
    target = lt;
    claim(lt);
    act_group(lt);
    return true;
}

void Bot::farm() {
    if (follow_squad()) return;
    if (!CFG_SQUAD && follow_partner()) return;
    if (gscore.size() != world.groups.size()) gscore.assign(world.groups.size(), 0.f);
    std::fill(gscore.begin(), gscore.end(), 0.f);
    int cand = choose_group();
    if (target_valid(target) && cand != target) {
        const auto& g = world.groups[target];
        const bool engaged = cheb(pos, g.center) <= 4;
        if (engaged || gscore[target] * P_STICKY >= gscore[cand < 0 ? target : cand]) cand = target;
    }
    target = cand;
    claim(target);
    if (BEAST_LOG && type == 0 && round % 4 == 0 && round < 260)
        LOG("R%d ryth pos(%d,%d) tgt=%d st=%d d=%d hpl=%.0f claims=%d pack=%d\n", round, pos.x, pos.y, target,
            target >= 0 ? (int)world.groups[target].state : -1, target >= 0 ? (int)world.group_field(target).get(pos) : -1,
            target >= 0 ? world.groups[target].hp_left() : 0.f, target >= 0 ? claims_on(target) : 0, sh.pack());
    if (target < 0) {
        act = A_F_NONE;
        step_field(world.spawn_f(), true);
        return;
    }
    act_group(target);
}

// ---------------------------------------------------------------- turn

void Bot::turn() {
    if (!inited) init();
    act = A_OTHER;
    turn_inner();
    ++acts[act];
    if (BEAST_LOG) {
        const auto left = bc::fuel_remaining();
        const auto used = (round <= 1 ? 200000ull : 100000ull) - left;
        if (used > max_used) {
            max_used = used;
            LOG("FUEL R%d slot%d used=%llu act=%d\n", round, world.my_slot, (unsigned long long)used, act);
        }
    }
    if (BEAST_LOG && alive_state() && hp < 0.25f * max_hp)
        LOG("LOW R%d slot%d pos(%d,%d) hp=%.0f act=%d camp_in=%.0f near=%d moved=%d st=%u\n", round, world.my_slot, pos.x, pos.y, hp, act,
            camp_in, (int)camp_near, (int)moved, st);
    if (BEAST_LOG && (round == 300 || round == 600)) {
        static const char* names[] = {"other", "dead", "chan_rec", "chan_sub", "combat", "heal", "bank", "f_trav", "f_atk", "f_capt", "f_wait", "f_none"};
        LOG("ACT R%d slot%d type%d:", round, world.my_slot, type);
        for (int i = 0; i < A_N; ++i) LOG(" %s=%d", names[i], acts[i]);
        LOG(" gold=%d str=%.0f hp=%.0f\n", gold, str, max_hp);
    }
}

void Bot::turn_inner() {
    snapshot();
    if (!alive_state()) {
        if (BEAST_LOG && was_alive)
            LOG("DEATH R%d slot%d type%d last: %s\n", round, world.my_slot, type, last_note);
        was_alive = false;
        act = A_DEAD;
        healing = false;
        shop();
        claim(-1);
        return;
    }
    if (st & (ST_RECALL | ST_SUBMIT | ST_TELE)) {
        act = (st & ST_SUBMIT) ? A_CHAN_SUB : A_CHAN_REC;
        if (world.ally_spawn.is_set(pos)) shop();
        return;
    }
    was_alive = true;
    observe_groups();
    predict_groups();
    assess_camps();
    if (CFG_HIDDEN) {
        // Damage that awake camps and visible enemies cannot explain comes from someone in a bush.
        if (prev_round == round - 1 && prev_hp > 0) {
            const float lost = prev_hp - hp;
            bool seen_hitter = false;
            for (auto& e : enemies)
                if (cheb(e.pos, pos) <= std::max(e.range, 4) + 1) seen_hitter = true;
            bool camp_close = false;
            for (auto& g : world.groups) {
                if (cheb(g.center, pos) > 7 || g.state == GState::respawning || g.state == GState::reward) continue;
                for (auto& c : g.camps)
                    if (!c.dead && c.str > 0 && cheb(c.pos, pos) <= 4) camp_close = true;
            }
            if (lost > prev_camp_in + 25.f && !seen_hitter && !camp_close) {
                hidden_round = round;
                hidden_pos = pos;
            }
        }
        prev_hp = hp;
        prev_round = round;
        prev_camp_in = camp_threat_at(pos);
    }
    if (BEAST_LOG && hp < 0.4f * max_hp)
        std::snprintf(last_note, sizeof last_note, "R%d pos(%d,%d) hp=%.0f/%.0f camp_in=%.0f near=%d heal=%d tgt=%d en=%d", round, pos.x,
                      pos.y, hp, max_hp, camp_in, (int)camp_near, (int)healing, target, (int)enemies.size());
    const bool at_spawn = world.ally_spawn.is_set(pos);
    if (at_spawn || type == static_cast<int>(bc::champion_type::tavi)) shop();
    if (bc::fuel_remaining() < 20000) return;

    if (CFG_COMBAT_OLD ? combat_old() : combat()) {
        act = A_COMBAT;
        return;
    }
    if (CFG_HIDDEN && round - hidden_round <= 3 && cheb(hidden_pos, pos) <= 6 && hp < 0.75f * max_hp) {
        act = A_COMBAT;
        retreat();
        return;
    }
    if (need_heal()) {
        act = A_HEAL;
        claim(-1);
        go_heal();
        return;
    }
    if (CFG_RALLY && answer_rally()) {
        act = A_COMBAT;
        return;
    }
    if (should_bank()) {
        act = A_BANK;
        go_bank();
        return;
    }
    if (CFG_SHOP_TRIP && shop_trip()) {
        act = A_OTHER;
        return;
    }
    if (bc::fuel_remaining() < 25000) return;
    farm();
}
