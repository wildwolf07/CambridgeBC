#include "common.h"
#include "draft.h"
#include "draft_prefs.h"

void play_draft() {
    using enum bc::champion_draft;
    const auto mine = bc::draft::current_draft(team::ally);
    static constexpr bc::champion_draft prefs[] = {BEAST_DRAFT_PREFS};
    for (auto p : prefs) {
        if (std::find(mine.begin(), mine.end(), p) != mine.end()) continue;
        bc::draft::pick(p);
        return;
    }
}
