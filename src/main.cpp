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

  auto grid = TracedGrid<symbol>{{37u, 37u, 1u}, 'B'};
  grid[{
    grid.extents.x / 2,
    grid.extents.y / 2,
    grid.extents.z / 2,
  }] = 'W';

  auto window = ncurses::window{grid.extents.y, grid.extents.x};
  window.say("hello!");
  window.waitchar();

  for (const auto& [u, value] : std::views::zip(locations(grid.extents), grid)) {
    if (basic_snake_offsets.contains(value))
      window.addch(u.y, u.x, value, basic_snake_offsets.at(value));
    else
      window.addch(u.y, u.x, value);
  }
  window.refresh();
  
  for (auto changes : basic_snake(grid)) {
    for (const auto& [u, value] : changes) {
      if (basic_snake_offsets.contains(value))
        window.addch(u.y, u.x, value, basic_snake_offsets.at(value));
      else
        window.addch(u.y, u.x, value);
    }
    window.refresh();
  }

  return 0;
}