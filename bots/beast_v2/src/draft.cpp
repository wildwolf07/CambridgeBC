#include "common.h"
#include "draft.h"

void play_draft() {
    const auto mine = bc::draft::current_draft(team::ally);
    static constexpr std::array prefs{
        bc::champion_draft::vendetta, bc::champion_draft::varg,     bc::champion_draft::ryth,
        bc::champion_draft::marcella, bc::champion_draft::tavi,     bc::champion_draft::stellar,
        bc::champion_draft::ren,      bc::champion_draft::vesper_vale, bc::champion_draft::willow,
        bc::champion_draft::prism,    bc::champion_draft::cassi_and_issac,
    };
    for (auto p : prefs) {
        if (std::find(mine.begin(), mine.end(), p) != mine.end()) continue;
        bc::draft::pick(p);
        return;
    }
}
