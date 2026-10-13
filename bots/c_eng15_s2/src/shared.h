#pragma once
#include "common.h"

// Team shared memory packed into one 64-bit word:
//   bits 0-35  six 6-bit group claims (group index + 1, 0 = none), one per ally slot
//   bits 36-41 pack target (group index + 1)
//   bits 42-44 focus enemy (enemy index + 1)
//   bits 45-51 rally x, 52-58 rally y, 59-63 rally round (low 5 bits)
struct Shared {
    std::uint64_t w = 0;

    void load() { bc::detail::sdk_read_shared_memory(0, &w, 8); }
    void store() const { bc::detail::sdk_write_shared_memory(0, &w, 8); }

    int get(int off, int bits) const { return static_cast<int>((w >> off) & ((1ull << bits) - 1)); }
    void put(int off, int bits, int v) {
        const std::uint64_t m = ((1ull << bits) - 1) << off;
        const std::uint64_t nw = (w & ~m) | ((static_cast<std::uint64_t>(v) << off) & m);
        if (nw != w) {
            w = nw;
            store();
        }
    }

    int claim(int slot) const { return slot < 6 ? get(slot * 6, 6) - 1 : -1; }
    void set_claim(int slot, int gi) {
        if (slot < 6) put(slot * 6, 6, gi < 0 ? 0 : gi + 1);
    }
    int pack() const { return get(36, 6) - 1; }
    void set_pack(int gi) { put(36, 6, gi < 0 ? 0 : gi + 1); }
    int focus() const { return get(42, 3) - 1; }
    void set_focus(int ei) { put(42, 3, ei < 0 ? 0 : ei + 1); }
    vec2 rally() const { return {get(45, 7), get(52, 7)}; }
    int rally_age(int round) const { return (round - get(59, 5)) & 31; }
    bool has_rally() const { return get(45, 14) != 0; }
    void set_rally(vec2 p, int round) {
        std::uint64_t nw = w & ~(((1ull << 19) - 1) << 45);
        nw |= (static_cast<std::uint64_t>(p.x & 127) << 45) | (static_cast<std::uint64_t>(p.y & 127) << 52) |
              (static_cast<std::uint64_t>(round & 31) << 59);
        if (nw != w) {
            w = nw;
            store();
        }
    }
};
