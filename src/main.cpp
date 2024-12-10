#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
// import glm;
import stormkit.Core;
import ranges;
import utils;
import ncurses;

import grid;
import rule;
import node;

using namespace std::literals;
using namespace stormkit;
using symbol = char;

constexpr symbol Zsep = ' ';
constexpr symbol Ysep = '/';
constexpr symbol Ignored = '*';

const auto basic_snake_offsets = std::views::zip(
  "BWDPGR"sv, std::views::iota(0u)
) 
// | std::views::take(std::ranges::size("BWDPGR") - 1)
| std::ranges::to<std::unordered_map<symbol, UInt>>();

auto basic_snake = Sequence<symbol>{{
  NoLimit<symbol>{All{std::array{
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "WBB", "**D"),
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "DBB", "**D"),
  } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}},
  NoLimit<symbol>{One{std::array{
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "WBD", "PGR"),
  } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}},
  Limit<symbol>{2, One{std::array{
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "RBD", "GGR"),
  } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}},
  Limit<symbol>{15, One<symbol>{{
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "D", "W"),
  }}},
  Markov<symbol>{{
    NoLimit<symbol>{One{std::array{
      Rule<symbol>::parse(Zsep, Ysep, Ignored, "RBW", "GGR"),
    } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}},
    NoLimit<symbol>{All{std::array{
      Rule<symbol>::parse(Zsep, Ysep, Ignored, "RBD", "GGR"),
      Rule<symbol>::parse(Zsep, Ysep, Ignored, "PGG", "DBP"),
    } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}},
  }},
}};

const auto seq_snake_offsets = std::views::zip(
  "BWEPRUG"sv, std::views::iota(0u)
) 
| std::ranges::to<std::unordered_map<symbol, UInt>>();

auto seq_snake = Sequence<symbol>{{
  NoLimit<symbol>{One{std::array{
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "WBB", "PER"),
  } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}},
  Limit<symbol>{10, One{std::array{
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "RBB", "EER"),
  } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}},
  Markov<symbol>{{
    NoLimit<symbol>{One{std::array{
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "RBB", "GGU"),
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "EEG", "GGG"),
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "PEG", "BBP"),
    } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}},
    NoLimit<symbol>{All<symbol>{{
      Rule<symbol>::parse(Zsep, Ysep, Ignored, "G", "E"),
      Rule<symbol>::parse(Zsep, Ysep, Ignored, "U", "R"),
    }}},
    NoLimit<symbol>{All<symbol>{{
      Rule<symbol>::parse(Zsep, Ysep, Ignored, "R", "P"),
      Rule<symbol>::parse(Zsep, Ysep, Ignored, "P", "R"),
    }}},
  }},
}};

auto growth = Markov<symbol>{{
  NoLimit<symbol>{One{std::array{
    Rule<symbol>::parse(Zsep, Ysep, Ignored, "WB", "WW"),
  } | std::views::transform(symmetries<symbol>) | std::views::join | std::ranges::to<std::vector>()}}
}};

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");

  auto grid = TracedGrid<symbol>{{67u, 67u, 1u}, 'B'};
  grid[{
    grid.size.x / 2,
    grid.size.y / 2,
    grid.size.z / 2,
  }] = 'W';

  auto window = ncurses::window{grid.size.y, grid.size.x};
  window.say("hello!");
  window.waitchar();

  // for (auto [line, y] : std::views::zip(grid.values | std::views::chunk(grid.size.x), std::views::iota(0u))) {
  for (auto [value, u] : std::views::zip(grid.values, std::views::iota(0u) | std::views::transform(bindBack(fromIndex, grid.size)))) {
    if (basic_snake_offsets.contains(value))
      window.addch(u.y, u.x, value, basic_snake_offsets.at(value));
    else
      window.addch(u.y, u.x, value);
  }
  window.refresh();
  
  for (auto changes : basic_snake(grid)) {
    for (auto &[u, value] : changes) {
      if (basic_snake_offsets.contains(value))
        window.addch(u.y, u.x, value, basic_snake_offsets.at(value));
      else
        window.addch(u.y, u.x, value);
    }
    window.refresh();
  }

  return 0;
}