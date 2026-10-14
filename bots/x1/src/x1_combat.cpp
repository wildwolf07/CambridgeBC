#include "bot.h"
#include "types.h"
#include <cmath>

// x1 fight engine: threat map, cluster race with heals/shields/spells, and a planner that scores every legal
// (dash / step / act) combination of the turn. Enabled with CFG_X1_COMBAT.
namespace {
constexpr std::uint32_t ST_RECALL = 1u << 1, ST_SUBMIT = 1u << 2, ST_MOVE = 1u << 3, ST_ATTACK = 1u << 4,
                        ST_CAST = 1u << 11, ST_TELE = 1u << 12;
int raw_move(vec2 d) {
    std::int32_t ok = 0;
    if (bc::detail::sdk_move_to(d.x, d.y, &ok) != 0) return -1;
    return ok ? 1 : 0;
}
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
bool acting(const UnitView& e) { return !e.stunned && !(e.state & (ST_RECALL | ST_SUBMIT | ST_TELE)); }
}  // namespace

bool Bot::x_step(vec2 q) {
    if (!can_walk() || q == pos || !in_bounds(q) || blocked.is_set(q) || cheb(q, pos) != 1) return false;
    if (raw_move(q) != 1) return false;
    last_pos = pos;
    pos = q;
    moved = true;
    st &= ~ST_MOVE;
    return true;
}

bool Bot::x_cast_on(const UnitView& e, bool allow_excl) {
    bool any = false;
    const int d = cheb(pos, e.pos);
    for (const Spell* sp : {&kit.cc, &kit.dmg}) {
        if (!(st & ST_CAST) || sp->slot < 0 || d > static_cast<int>(sp->range) || cooldown(sp->slot) != 0) continue;
        if (sp->excl && !allow_excl) continue;
        if (sp == &kit.cc && e.stunned) continue;
        if (raw_cast(sp->slot, 1, e.h, {-1, -1}) == 1) {
            st &= ~ST_CAST;
            if (sp->excl) cast_excl = true;
            any = true;
        }
    }
    if ((st & ST_CAST) && kit.aoe.slot >= 0 && d <= static_cast<int>(kit.aoe.range) && cooldown(kit.aoe.slot) == 0 &&
        (allow_excl || !kit.aoe.excl)) {
        if (raw_cast(kit.aoe.slot, 0, 0, e.pos) == 1) {
            st &= ~ST_CAST;
            if (kit.aoe.excl) cast_excl = true;
            any = true;
        }
    }
    return any;
}

// ============================================================== fight model


// Expected damage the visible enemies can put on tile q during their next turn.
// Melee: step + hit (reach 2), dash adds reach 4 if the dash is ready. Ranged basic: no step (reach = range).
// Spells: movement-exclusive casts have reach 4. Unknown cooldowns are weighted by P_X_CD.
float Bot::x_threat(vec2 q) const {
    float t = 0;
    for (auto& e : enemies) {
        if (!acting(e) || world.enemy_spawn.is_set(e.pos)) continue;
        const auto& k = kit_of(e.type);
        const int d = cheb(q, e.pos);
        float dm = 0;
        if (k.melee) {
            if (d <= 2) dm += e.str * (k.reset ? 1.f + P_X_CD : 1.f);
            else if (k.dash && d <= 4) dm += P_X_CD * e.str;
        } else if (d <= e.range) {
            dm += e.str * (k.reset ? 1.f + P_X_CD : 1.f);
        }
        if (d <= 4) dm += P_X_CD * (k.dmg + k.cc + k.aoe) * e.focus;
        t += dm;
    }
    return t;
}

namespace {
struct SU {
    float hp, dps, val;
    bool me;
};
// Focus-fire race: each side hits the other's lowest-HP unit. Value = bodies won (C&I count 2) minus bodies lost
// (our own death costs half a body more) plus half the HP-fraction difference.
float sim_race(SU* A, int na, SU* E, int ne) {
    if (ne == 0) return 1.f;
    float a0 = 0, e0 = 0;
    for (int i = 0; i < na; ++i) a0 += A[i].hp;
    for (int i = 0; i < ne; ++i) e0 += E[i].hp;
    auto hit = [](SU* side, int n, float dmg) {
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
    for (int r = 0; r < 30; ++r) {
        float da = 0, de = 0;
        for (int i = 0; i < na; ++i)
            if (A[i].hp > 0) da += A[i].dps;
        for (int i = 0; i < ne; ++i)
            if (E[i].hp > 0) de += E[i].dps;
        if (da <= 0 || de <= 0) break;
        hit(E, ne, da);
        hit(A, na, de);
    }
    float v = 0, a1 = 0, e1 = 0;
    for (int i = 0; i < ne; ++i) {
        if (E[i].hp <= 0) v += E[i].val;
        e1 += std::max(0.f, E[i].hp);
    }
    for (int i = 0; i < na; ++i) {
        if (A[i].hp <= 0) v -= A[i].val + (A[i].me ? 0.5f : 0.f);
        a1 += std::max(0.f, A[i].hp);
    }
    return v + 0.5f * ((e0 - e1) / std::max(1.f, e0) - (a0 - a1) / std::max(1.f, a0));
}
}  // namespace

// Race between everyone of ours within P_X_CLA and every visible enemy within P_X_CLE of me.
// Heals and shields enter as extra HP; the first exchange includes burst (reset + ready spells).
float Bot::x_race(bool include_me) const {
    SU A[10], E[10];
    int na = 0, ne = 0;
    auto su = [&](int t, float h, float s, float f, bool me) {
        const auto& k = kit_of(t);
        float extra = 0;
        if (k.shield > 0) extra += P_X_CD * k.shield * f;
        // spread the opening burst over the first 4 rounds of the race
        const float d = sustained_dps(t, s, f) + 0.25f * P_X_CD * (burst(t, s, f) - s);
        return SU{h + extra, d, is_ci(t) ? 2.f : 1.f, me};
    };
    float heal_a = 0, heal_e = 0;
    if (include_me) A[na++] = su(type, hp, str, focus, true);
    for (auto& a : allies) {
        if (na >= 10 || cheb(a.pos, pos) > P_X_CLA || (a.state & (ST_RECALL | ST_TELE))) continue;
        A[na++] = su(a.type, a.hp, a.str, a.focus, false);
        if (kit_of(a.type).heal > 0) heal_a += kit_of(a.type).heal * a.focus;
    }
    if (kit.heal >= 0) heal_a += 3.f * focus;
    for (auto& e : enemies) {
        if (ne >= 10 || cheb(e.pos, pos) > P_X_CLE || world.enemy_spawn.is_set(e.pos)) continue;
        if (e.state & (ST_RECALL | ST_TELE)) continue;
        E[ne++] = su(e.type, e.hp, e.str, e.focus, false);
        if (kit_of(e.type).heal > 0) heal_e += P_X_CD * kit_of(e.type).heal * e.focus;
    }
    if (ne == 0) return 1.f;
    // A heal lands on whoever is focused first: add it to the weakest unit of that side.
    auto add_heal = [](SU* S, int n, float h) {
        int t = -1;
        for (int i = 0; i < n; ++i)
            if (t < 0 || S[i].hp < S[t].hp) t = i;
        if (t >= 0) S[t].hp += h;
    };
    add_heal(A, na, heal_a);
    add_heal(E, ne, heal_e);
    return sim_race(A, na, E, ne);
}

// Value of dealing `dmg` to enemy e: fraction of its HP removed, with a large bonus for a kill.
// C&I count double (one kill removes both), carried gems are taken by the killer, casters are high-value targets.
float Bot::x_target_value(const UnitView& e, float dmg) const {
    const float body = is_ci(e.type) ? 2.f : 1.f;
    float v = std::min(1.f, dmg / std::max(1.f, e.hp)) * body;
    if (dmg >= e.hp) v += body * (1.5f + static_cast<float>(e.gems) / 40.f);
    if (is_caster(e.type)) v *= 1.15f;
    if (!acting(e)) v *= 1.2f;
    return v;
}

// Best offense available from tile `from`: basic attack if a target is in reach (melee: adjacent; ranged: only
// when we have not moved), ready spells (movement-exclusive ones only if excl_ok). Returns the value and the target.
float Bot::x_offense(vec2 from, bool moved_already, bool excl_ok, std::uint32_t& best_h) const {
    float best = 0;
    best_h = 0;
    const int focus_idx = sh.focus();
    for (auto& e : enemies) {
        if (world.enemy_spawn.is_set(e.pos)) continue;
        const int d = cheb(from, e.pos);
        float dmg = 0;
        const bool basic_ok = (st & ST_ATTACK) && d <= range && (kit.melee || !moved_already);
        if (basic_ok) dmg += str * ((kit.reset >= 0 && (st & ST_CAST) && cooldown(kit.reset) == 0) ? 2.f : 1.f);
        if (st & ST_CAST) {
            for (const Spell* sp : {&kit.dmg, &kit.cc, &kit.aoe}) {
                if (sp->slot < 0 || sp->mod <= 0 || d > static_cast<int>(sp->range) || cooldown(sp->slot) != 0) continue;
                if (sp->excl && (!excl_ok || moved_already)) continue;
                dmg += sp->mod * focus;
            }
        }
        if (dmg <= 0) continue;
        float v = x_target_value(e, dmg);
        int ei = -1;
        for (int i = 0; i < static_cast<int>(world.enemy_handles.size()); ++i)
            if (world.enemy_handles[i] == e.h) ei = i;
        if (ei >= 0 && ei == focus_idx) v *= 1.3f;
        if (v > best) {
            best = v;
            best_h = e.h;
        }
    }
    return best;
}

// Enumerate legal ways to spend the turn and score them.
// engage: offense - risk; disengage: minimise risk, keep free hits.
Plan Bot::x_best_plan(bool engage) {
    Plan best;
    const float hp_eff = std::max(50.f, hp);
    int sd0 = world.spawn_f().get(pos);
    if (sd0 == UNREACH) sd0 = 99;
    auto support = [&](vec2 q) {
        int n = 0;
        for (auto& a : allies)
            if (cheb(a.pos, q) <= 2) ++n;
        return static_cast<float>(std::min(n, 2));
    };
    auto score_tile = [&](vec2 q, float off) {
        const float risk = (x_threat(q) + camp_threat_at(q) * 0.5f) / hp_eff;
        float s;
        if (engage) {
            s = off - P_X_RISK * risk + 0.15f * support(q);
            if (risk >= 1.f) s -= 3.f;  // tile where we can die this turn
        } else {
            int sd = world.spawn_f().get(q);
            if (sd == UNREACH) sd = 99;
            s = 0.35f * off - 3.f * risk - 0.05f * static_cast<float>(sd - sd0) + 0.1f * support(q);
        }
        return s;
    };
    std::vector<vec2> tiles{pos};
    if (can_walk())
        for (auto d : DIRS) {
            const vec2 q = pos + d;
            if (in_bounds(q) && !blocked.is_set(q)) tiles.push_back(q);
        }
    for (auto q : tiles) {
        const bool moving = q != pos;
        std::uint32_t h = 0;
        // move (if any) then act
        const float off = x_offense(q, moving, !moving, h);
        const float s = score_tile(q, off);
        if (s > best.score) {
            best.score = s;
            best.end = q;
            best.attack_first = false;
            best.target = h;
            best.use_excl = !moving;
        }
        // melee: act from the start tile, then step (hit-and-run / hit-and-follow)
        if (kit.melee && moving) {
            std::uint32_t h0 = 0;
            const float off0 = x_offense(pos, false, false, h0);
            if (h0) {
                const float s0 = score_tile(q, off0);
                if (s0 > best.score) {
                    best.score = s0;
                    best.end = q;
                    best.attack_first = true;
                    best.target = h0;
                    best.use_excl = false;
                }
            }
        }
    }
    // Dash: engage onto a target that is out of reach, or escape when the current tile is deadly.
    if (kit.dash >= 0 && (st & ST_CAST) && cooldown(kit.dash) == 0 && can_walk()) {
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = -2; dy <= 2; ++dy) {
                if (iabs(dx) < 2 && iabs(dy) < 2) continue;
                const vec2 q{pos.x + dx, pos.y + dy};
                if (!in_bounds(q) || blocked.is_set(q)) continue;
                std::uint32_t h = 0;
                const float off = x_offense(q, false, false, h);
                const float s = score_tile(q, off) - 0.2f;  // keep the dash unless it buys something
                if (s > best.score) {
                    best.score = s;
                    best.dash_to = q;
                    best.end = q;
                    best.attack_first = false;
                    best.target = h;
                    best.use_excl = false;
                }
            }
    }
    return best;
}

void Bot::x_execute(const Plan& p) {
    const UnitView* tgt = nullptr;
    for (auto& e : enemies)
        if (e.h == p.target) tgt = &e;
    auto act = [&](bool allow_excl) {
        if (!tgt) return;
        x_cast_on(*tgt, allow_excl);
        if (cheb(pos, tgt->pos) <= range) attack_unit_handle(tgt->h, true);
    };
    if (p.dash_to.x >= 0) {
        if (raw_cast(kit.dash, 0, 0, p.dash_to) == 1) {
            pos = p.dash_to;
            st &= ~ST_CAST;
        }
        act(false);
        return;
    }
    if (p.attack_first) {
        act(false);
        if (p.end != pos) x_step(p.end);
        return;
    }
    // Non-exclusive casts (e.g. Cassi's stun) can go before a step.
    if (p.end != pos && tgt) x_cast_on(*tgt, false);
    if (p.end != pos) x_step(p.end);
    act(p.use_excl && !moved);
}

// Willow: heal the most injured ally in range before anything else (does not cost the move).
bool Bot::x_try_heal_unused() {
    if (kit.heal < 0 || !(st & ST_CAST) || cooldown(kit.heal) != 0) return false;
    const float amount = 3.0f * focus;
    std::uint32_t best_h = 0;
    float best_sc = 0;
    auto consider = [&](std::uint32_t h, float h_, float m_, vec2 p) {
        if (cheb(p, pos) > 4) return;
        const float missing = m_ - h_;
        bool fighting = false;
        for (auto& e : enemies)
            if (cheb(e.pos, p) <= 3) fighting = true;
        if (missing < 0.6f * amount && !(fighting && h_ < 0.5f * m_)) return;
        const float sc = std::min(missing, amount) * (fighting ? 1.5f : 1.f) * (h_ < 0.35f * m_ ? 1.5f : 1.f);
        if (sc > best_sc) {
            best_sc = sc;
            best_h = h;
        }
    };
    consider(world.self_handle, hp, max_hp, pos);
    for (auto& a : allies) consider(a.h, a.hp, a.max_hp, a.pos);
    if (best_h && raw_cast(kit.heal, 1, best_h, {-1, -1}) == 1) {
        st &= ~ST_CAST;
        return true;
    }
    return false;
}

// Fight handling. Returns true if the turn was spent on champions.
bool Bot::combat_x1() {
    bool near = false;
    for (auto& e : enemies)
        if (!world.enemy_spawn.is_set(e.pos) && cheb(pos, e.pos) <= 7) near = true;
    if (!near) return false;
    const float v = x_race(true);
    // Team-consistent verdict: if an ally declared a fight close by in the last 2 rounds, use the hold threshold.
    const bool anchored = sh.has_rally() && sh.rally_age(round) <= 2 && cheb(sh.rally(), pos) <= 6;
    const bool linked = is_ci(type);
    const bool low = hp < (linked ? P_X_LOW_CI : P_X_LOW) * max_hp;
    const bool rich = gems >= 40;
    float need = P_X_ENGAGE + (rich ? 0.5f : 0.f) + (linked ? 0.2f : 0.f) + (low ? 1.0f : 0.f);
    if (anchored || round - engaged_round <= 2) need = std::min(need, P_X_HOLD + (low ? 1.0f : 0.f));
    const bool engage = v > need;
    if (engage) {
        engaged_round = round;
        sh.set_rally(pos, round);
    }
    const bool threatened = x_threat(pos) > 0;
    Plan p = x_best_plan(engage);
    if (!engage && !threatened) return false;  // nothing can hit us next turn: keep farming
    if (engage && p.target == 0 && !threatened) {
        // Nothing in reach. Only walk in to support a fight an ally declared next to us; never chase across the map
        // (top teams approach a weakened enemy 7% of the time).
        if (!(anchored && cheb(sh.rally(), pos) <= 5)) return false;
        const UnitView* near_e = nullptr;
        for (auto& e : enemies)
            if (!world.enemy_spawn.is_set(e.pos) && (!near_e || cheb(pos, e.pos) < cheb(pos, near_e->pos))) near_e = &e;
        if (!near_e) return false;
        vec2 best = pos;
        float bs = 1e9f;
        for (auto d : DIRS) {
            const vec2 q = pos + d;
            if (!in_bounds(q) || blocked.is_set(q)) continue;
            const float sc = static_cast<float>(cheb(q, near_e->pos)) + 2.f * x_threat(q) / std::max(50.f, hp);
            if (sc < bs) {
                bs = sc;
                best = q;
            }
        }
        if (best != pos && cheb(best, near_e->pos) < cheb(pos, near_e->pos)) return x_step(best);
        return false;
    }
    if (p.target) {
        int ei = -1;
        for (int i = 0; i < static_cast<int>(world.enemy_handles.size()); ++i)
            if (world.enemy_handles[i] == p.target) ei = i;
        if (engage && ei >= 0) sh.set_focus(ei);
    }
    const vec2 p0 = pos;
    const std::uint32_t s0 = st;
    x_execute(p);
    return engage || threatened || pos != p0 || st != s0;
}

