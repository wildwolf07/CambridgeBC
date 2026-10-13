#include <cambc.h>
#include <algorithm>

int main() {
    while (true) {
        try {
            if (cambc::is_draft()) {
                const auto mine = cambc::draft::current_draft(cambc::team::ally);
                for (int i = 0; i < 11; ++i) {
                    auto p = static_cast<cambc::champion_draft>(i);
                    if (std::find(mine.begin(), mine.end(), p) == mine.end()) { cambc::draft::pick(p); break; }
                }
            }
        } catch (...) {
        }
        cambc::end_turn();
    }
}
