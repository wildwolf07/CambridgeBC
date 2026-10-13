#include <cambc.h>
#include <cstdio>
using namespace cambc;
// Probe: does a ward placed by one champion give vision to a far-away teammate?
int main() {
    while (true) {
        try {
            if (is_draft()) {
                auto mine = draft::current_draft(team::ally);
                const champion_draft order[] = {champion_draft::vendetta, champion_draft::varg, champion_draft::ryth, champion_draft::tavi, champion_draft::marcella};
                draft::pick(order[mine.size()]);
            } else {
                auto& me = self();
                auto r = round_number();
                vec2 far{map::width() / 2, map::height() / 2};
                if (me.champion_name() == champion_type::vendetta) {
                    if (r < 40) me.move_towards(far);
                    if (r == 40) {
                        bool ok = false;
                        for (int dx = -2; dx <= 2 && !ok; ++dx)
                            for (int dy = -2; dy <= 2 && !ok; ++dy) {
                                vec2 q{me.position().x + dx, me.position().y + dy};
                                if (me.ward_placement_tiles().is_set(q)) { try { ok = me.place_ward(q); } catch (...) {} }
                            }
                        std::printf("R40 vendetta at (%d,%d) placed=%d wards=%zu\n", me.position().x, me.position().y, (int)ok, me.ward_positions().size());
                    }
                    if (r > 40 && r < 60) me.move_towards(vec2{2, 2});
                }
                if (me.champion_name() == champion_type::varg && (r == 45 || r == 70)) {
                    auto v = vision::visible_tiles();
                    std::printf("R%u varg at (%d,%d) sees %zu tiles; centre(%d,%d) visible=%d\n", r, me.position().x, me.position().y, v.count(), far.x, far.y, (int)v.is_set(far));
                }
            }
        } catch (const std::exception& e) {
            std::printf("ERR %s\n", e.what());
        }
        end_turn();
    }
}
