#include <cambc.h>
#include <cstdio>
using namespace cambc;
// Probe: is enemy visibility shared across the team? Every champion prints which enemies it can inspect.
int main() {
    while (true) {
        try {
            if (is_draft()) {
                auto mine = draft::current_draft(team::ally);
                const champion_draft order[] = {champion_draft::vendetta, champion_draft::varg, champion_draft::ryth, champion_draft::tavi, champion_draft::marcella};
                draft::pick(order[mine.size()]);
            } else {
                auto& me = self();
                if (round_number() % 5 == 0) {
                    std::printf("R%u me=%d(%d,%d) sees:", round_number(), (int)me.champion_name(), me.position().x, me.position().y);
                    for (auto& c : champions(team::enemy))
                        if (c.is_visible()) std::printf(" e%d(%d,%d)", (int)c.champion_name(), c.position().x, c.position().y);
                    std::printf(" | allies:");
                    for (auto& c : champions(team::ally))
                        if (c.is_alive()) { bool v = c.is_visible(); std::printf(" a%d:%d", (int)c.champion_name(), (int)v); if (v) std::printf("@%lld", (long long)c.position().chebyshev_distance(me.position())); }
                    std::printf(" | vis_tiles=%zu\n", vision::visible_tiles().count());
                }
                // wander toward map centre so champions spread
                vec2 tgt{map::width() / 2 + ((int)me.champion_name() % 3 - 1) * 8, map::height() / 2 + ((int)me.champion_name() % 2) * 6};
                me.move_towards(tgt);
            }
        } catch (const std::exception& e) {
            std::printf("ERR %s\n", e.what());
        }
        end_turn();
    }
}
