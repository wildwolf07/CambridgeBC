#include "bot.h"
#include "draft.h"

int main() {
    Bot bot;
    while (true) {
        try {
            if (bc::is_draft())
                play_draft();
            else
                bot.turn();
        } catch (const std::exception& e) {
            LOG("ERR %s\n", e.what());
        } catch (...) {
        }
        bc::end_turn();
    }
}
