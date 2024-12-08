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

// using namespace std::literals;
using namespace stormkit;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");
  using symbol = chtype;

  const auto seq_snake = Sequence<symbol>{{
    NoLimit<symbol>{One{std::array{
      symmetries<symbol>({{{{{{'W'}},{{'B'}},{{'B'}}}}}, {{{'P','E','R'}}}}),
    } | std::views::join | std::ranges::to<std::vector>()}},
    Limit<symbol>{10, One{std::array{
      symmetries<symbol>({{{{{{'R'}},{{'B'}},{{'B'}}}}}, {{{'E','E','R'}}}}),
    } | std::views::join | std::ranges::to<std::vector>()}},
    Markov<symbol>{{
      NoLimit<symbol>{One{std::array{
        symmetries<symbol>({{{{{{'R'}},{{'B'}},{{'B'}}}}}, {{{'G','G','U'}}}}),
        symmetries<symbol>({{{{{{'E'}},{{'E'}},{{'G'}}}}}, {{{'G','G','G'}}}}),
        symmetries<symbol>({{{{{{'P'}},{{'E'}},{{'G'}}}}}, {{{'B','B','P'}}}}),
      } | std::views::join | std::ranges::to<std::vector>()}},
      NoLimit<symbol>{All<symbol>{{
        {{{{{{'G'}}}}}, {{{'E'}}}},
        {{{{{{'U'}}}}}, {{{'R'}}}},
      }}},
      NoLimit<symbol>{All<symbol>{{
        {{{{{{'R'}}}}}, {{{'P'}}}},
        {{{{{{'P'}}}}}, {{{'R'}}}},
      }}},
    }},
  }};

  const auto growth = Markov<symbol>{{
    NoLimit<symbol>{One{std::array{
      symmetries<symbol>({{{{{{'W'}}, {{'B'}}}}}, {{{'W', 'W'}}}}),
    } | std::views::join | std::ranges::to<std::vector>()}}
  }};

  auto grid = Grid<symbol>{{67u, 67u, 1u}, 'B'};
  grid[{
    grid.size.x / 2,
    grid.size.y / 2,
    grid.size.z / 2,
  }] = 'W';

  const auto offsets = std::views::zip(
      std::vector<symbol>{'B','W','E','P','R','U','G'}, 
      std::views::iota(0u)
    )
    | std::ranges::to<std::unordered_map<symbol, UInt>>();

  auto window = ncurses::window{grid.size.y, grid.size.x};
  window.say("hello!");
  window.waitchar();

  // for (auto [line, y] : std::views::zip(grid.values | std::views::chunk(grid.size.x), std::views::iota(0u))) {
  for (auto [line, y] : std::views::zip(chunk(grid.values, grid.size.x), std::views::iota(0u))) {
    window.addchstr(y, 0u, line | std::ranges::to<std::vector>());
  }
  window.refresh();

  for (auto changes : /*growth*/seq_snake(grid)) {
    if (std::ranges::empty(changes)) continue;
    for (auto &[u, value] : changes) {
      if (offsets.contains(value)) window.addch(u.y, u.x, value, offsets.at(value));
      else window.addch(u.y, u.x, value);
    }
    window.refresh();
  }

  return 0;
}