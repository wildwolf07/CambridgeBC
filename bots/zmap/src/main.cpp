#include <cambc.h>
#include <cstdio>
#include <string>
using namespace cambc;

int main() {
    int step = 0;
    while (true) {
        try {
            if (is_draft()) {
                auto mine = draft::current_draft(team::ally);
                const champion_draft order[] = {champion_draft::vendetta, champion_draft::varg, champion_draft::ryth,
                    champion_draft::marcella, champion_draft::tavi, champion_draft::willow};
                draft::pick(order[mine.size()]);
            } else if (self().champion_name() == champion_type::vendetta) {
                int W = map::width(), H = map::height();
                static std::string grid;
                if (grid.empty()) {
                    grid.assign((size_t)W * H, '.');
                    auto walls = map::walls(), holes = map::holes(), bush = map::bushes(), alt = map::altars();
                    auto sa = map::spawn_region(team::ally), se = map::spawn_region(team::enemy);
                    for (int y = 0; y < H; ++y)
                        for (int x = 0; x < W; ++x) {
                            vec2 p{x, y};
                            char c = '.';
                            if (walls.is_set(p)) c = '#';
                            else if (holes.is_set(p)) c = 'o';
                            else if (bush.is_set(p)) c = '"';
                            if (alt.is_set(p)) c = 'A';
                            if (sa.is_set(p)) c = 'S';
                            if (se.is_set(p)) c = 'E';
                            grid[(size_t)y * W + x] = c;
                        }
                    for (auto& g : map::camp_groups())
                        for (auto& c : g.camps()) {
                            const char* k = "wkgGM";
                            grid[(size_t)c.spawn_position().y * W + c.spawn_position().x] = k[(int)c.kind()];
                        }
                    std::printf("MAP %dx%d team_size=%u sym=%d\n", W, H, draft::team_size(), (int)map::symmetry());
                }
                int rows_per = 10;
                for (int y = step * rows_per; y < H && y < (step + 1) * rows_per; ++y) std::printf("%3d %s\n", y, grid.substr((size_t)y * W, W).c_str());
                ++step;
            }
        } catch (const std::exception& e) {
            std::printf("ERR %s\n", e.what());
        }
        end_turn();
    }
}
