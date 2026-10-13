#include <cambc.h>
#include <cstdio>
#include <variant>
using namespace cambc;

static const char* AK[] = {"?", "attack_reset", "damage", "aoe", "cc", "heal", "dash", "shield"};

static void dump_ab(const ability& a) {
    const auto& inf = a.info();
    if (!inf) { std::printf("    slot%u: none\n", a.slot()); return; }
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        std::printf("    slot%u: %s cd=%d targets=%u excl_move=%d", a.slot(), AK[(int)T::kind], v.max_cooldown,
                    (unsigned)v.valid_targets.bits(), (int)v.exclusive_with_movement);
        if constexpr (requires { v.range; }) std::printf(" range=%.2f", v.range);
        if constexpr (requires { v.modifier; }) std::printf(" mod=%.3f", v.modifier);
        if constexpr (requires { v.radius; }) std::printf(" radius=%.2f", v.radius);
        if constexpr (requires { v.duration; }) std::printf(" dur=%d", v.duration);
        if constexpr (requires { v.enhanced_attack; }) std::printf(" enhanced=%d", (int)v.enhanced_attack);
        std::printf("\n");
    }, *inf);
}

int main() {
    int step = 0;
    while (true) {
        try {
            if (is_draft()) {
                auto mine = draft::current_draft(team::ally);
                const champion_draft order[] = {champion_draft::vendetta, champion_draft::varg, champion_draft::ryth,
                    champion_draft::marcella, champion_draft::tavi, champion_draft::willow};
                draft::pick(order[mine.size()]);
            } else {
                int k = step++;
                if (self().champion_name() == champion_type::vendetta) {
                    namespace C = constants;
                    if (k == 0) {
                    std::printf("capture_time=%d camp_sleep=%d respawn=%d recall=%d\n", C::capture_time, C::camp_sleep_time, C::respawn_time, C::recall_time);
                    std::printf("submit mult=%.3f base=%d min=%d range=%d\n", C::submit_gem_duration_multiplier, C::submit_gem_base_duration, C::submit_gem_minimum_duration, C::submit_gems_range);
                    std::printf("max_rounds=%u winning_gems=%u kill_gold=%u assist_gold=%u passive_gold=%u spawn_heal=%.3f\n", C::max_rounds, C::winning_gems, C::kill_gold, C::assist_gold, C::passive_gold, C::spawn_healing);
                    std::printf("wards active=%u shared_mem=%u draft_mem=%u reveal=%u ward_cd=%d ward_life=%d ward_vis=%.2f ward_place=%.2f altar_lock_hits=%u tp_crystal_time=%d\n",
                        C::active_wards, C::shared_memory_bytes, C::draft_memory_bytes, C::tile_reveal_rounds, C::ward_cooldown, C::ward_lifetime, C::ward_vision_radius, C::ward_placement_radius, C::altar_lock_hits, C::tp_crystal_time);
                    std::printf("team_size=%u map %dx%d\n", draft::team_size(), map::width(), map::height());
                    }
                    for (int t = k - 1; t >= 0 && t < 12 && t == k - 1; ++t) {
                        champion c{(champion_type)t};
                        auto s = c.base_stats();
                        std::printf("CHAMP %d %s passive=%d vis=%d move=%d range=%d hp=%.0f+%.1f str=%.1f+%.2f foc=%.1f+%.2f campmult=%.2f\n", t,
                            to_string((champion_type)t).data(), (int)c.passive(), s.vision_radius, s.base_move_speed, s.base_attack_range, s.base_health, s.health_growth,
                            s.base_strength, s.strength_growth, s.base_focus, s.focus_growth, s.camp_damage_multiplier);
                        for (auto& a : c.abilities()) dump_ab(a);
                    }
                    for (std::uint32_t i = 3 * (k - 13); k >= 13 && i < (std::uint32_t)item_type::count && i < 3 * (k - 12); ++i) {
                        item_type it{(item_type::enum_type)i};
                        auto f = it.info();
                        std::printf("ITEM %u %s cost=%u comb=%u hp=%d str=%d foc=%d passive=%d mag=%.3f buy=%d\n", i, to_string(it).data(), f.cost, f.combine_count, f.health_points, f.strength_points, f.focus_points, f.passive ? (int)*f.passive : 0, f.passive_magnitude, (int)f.purchasable);
                    }
                    if (k == 30) for (auto ct : {consumable_type::tp_crystal, consumable_type::altar_lock}) {
                        auto f = ct.info();
                        std::printf("CONS %s cost=%u carry=%u\n", to_string(ct).data(), f.cost, f.carry_limit);
                    }
                    int gi = 0;
                    if (k >= 31) for (auto& g : map::camp_groups()) {
                        if (gi++ != k - 31) continue;
                        std::printf("GROUP gems=%u gold=%u maxresp=%d n=%zu:", g.gem_value(), g.gold_value(), g.max_respawn_timer(), g.camps().size());
                        for (auto& c : g.camps()) {
                            auto s = c.base_stats();
                            std::printf(" [%s@(%d,%d) hp=%.0f str=%.0f rng=%d vis=%.1f]", to_string(c.kind()).data(), c.spawn_position().x, c.spawn_position().y, s.base_health, s.base_strength, s.base_attack_range, s.vision_radius);
                        }
                        std::printf("\n");
                    }
                }
            }
        } catch (const std::exception& e) {
            std::printf("ERR %s\n", e.what());
        }
        end_turn();
    }
}
