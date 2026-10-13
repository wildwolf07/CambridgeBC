#include <cambc.h>
#include <cstdio>
#include <string>
#include <variant>

using namespace cambc;

// elkring: 5 champions attack elk at (3,6) from adjacent tiles; slot0 logs all ally hp each round.
int main() {
    int turn = 0;
    while (true) {
        try {
            if (is_draft()) {
                auto mine = draft::current_draft(team::ally);
                const champion_draft order[] = {champion_draft::vendetta, champion_draft::varg, champion_draft::ryth,
                    champion_draft::marcella, champion_draft::tavi};
                draft::pick(order[mine.size()]);
            } else {
                ++turn;
                auto& me = self();
                vec2 elk{3, 6};
                auto p = me.position();
                if (p.chebyshev_distance(elk) > 1) {
                    const vec2 spots[] = {{2, 5}, {3, 5}, {4, 5}, {2, 6}, {4, 6}, {2, 7}, {4, 7}};
                    int k = (int)me.champion_name() % 7;
                    me.move_towards(spots[k]);
                    if (me.position().chebyshev_distance(elk) > 1) me.move_towards(spots[(k + 3) % 7]);
                }
                bool hold = me.champion_name() == champion_type::vendetta && round_number() >= 12;
                if (!hold && vision::has_camp(elk) && me.position().chebyshev_distance(elk) <= me.attack_range() && me.can_attack()) {
                    me.attack(vision::camp_at(elk));
                }
                if (me.champion_name() == champion_type::vendetta && turn < 40) {
                    std::printf("R%u", round_number());
                    for (auto& c : champions(team::ally)) {
                        if (c.is_visible()) std::printf(" %d:%.0f@(%d,%d)", (int)c.champion_name(), c.health(), c.position().x, c.position().y);
                    }
                    if (vision::has_camp(elk)) std::printf(" elk=%.0f", vision::camp_at(elk).health());
                    std::printf("\n");
                }
            }
        } catch (const std::exception& e) {
            std::printf("ERR %s\n", e.what());
        }
        end_turn();
    }
}
