#pragma once
#include <cambc.h>
#include "config.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

namespace bc = cambc;
using bc::bitboard;
using bc::team;
using bc::vec2;

#ifndef BEAST_LOG
#define BEAST_LOG 0
#endif
#define LOG(...)                                                                                                     \
    do {                                                                                                             \
        if (BEAST_LOG) std::printf(__VA_ARGS__);                                                                     \
    } while (0)

inline int iabs(int v) { return v < 0 ? -v : v; }
inline int cheb(vec2 a, vec2 b) {
    int dx = iabs(a.x - b.x), dy = iabs(a.y - b.y);
    return dx > dy ? dx : dy;
}

inline constexpr std::array<vec2, 8> DIRS{vec2{0, 1}, vec2{1, 1}, vec2{1, 0}, vec2{1, -1},
                                           vec2{0, -1}, vec2{-1, -1}, vec2{-1, 0}, vec2{-1, 1}};

struct CampBase {
    float hp;
    float str;
};
inline CampBase camp_base(bc::camp_type t) {
    switch (t) {
    case bc::camp_type::wolf: return {1350.f, 20.f};
    case bc::camp_type::elk: return {2400.f, 60.f};
    case bc::camp_type::gem_cluster: return {500.f, 0.f};
    case bc::camp_type::gold_cluster: return {500.f, 0.f};
    case bc::camp_type::monolith: return {20000.f, 200.f};
    }
    return {1000.f, 0.f};
}

constexpr std::uint16_t UNREACH = bc::distance_field::unreachable;
