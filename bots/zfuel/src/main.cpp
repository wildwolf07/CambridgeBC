#include <cambc.h>
#include <cstdio>
using namespace cambc;
// Probe: fuel available at the start of each turn, and what happens when a turn burns it all.
int main() {
    while (true) {
        try {
            if (is_draft()) {
                auto mine = draft::current_draft(team::ally);
                const champion_draft order[] = {champion_draft::vendetta, champion_draft::varg, champion_draft::ryth, champion_draft::tavi, champion_draft::marcella};
                draft::pick(order[mine.size()]);
            } else if (self().champion_name() == champion_type::vendetta) {
                auto r = round_number();
                auto f0 = fuel_remaining();
                if (r <= 4 || r % 50 == 0) std::printf("R%u start fuel=%llu\n", r, (unsigned long long)f0);
                if (r == 10) {
                    volatile unsigned long long x = 0;
                    unsigned long long last = f0;
                    for (unsigned long long i = 0; i < 100000000ull; ++i) {
                        x += i;
                        if ((i & 1023) == 0) {
                            last = fuel_remaining();
                            if (last < 500) break;
                        }
                    }
                    std::printf("R10 burned to %llu\n", (unsigned long long)last);
                }
                if (r == 11) std::printf("R11 alive start fuel=%llu\n", (unsigned long long)f0);
            }
        } catch (const std::exception& e) {
            std::printf("ERR %s\n", e.what());
        }
        end_turn();
    }
}
