#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
// import glm;
import stormkit.Core;
import ranges;

import grid;
import rule;
import node;

// using namespace std::literals;
using namespace stormkit;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");

  std::println("hello world!");
  using symbol = decltype('B');

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

  auto grid = Grid{{13u, 13u, 1u}, 'B'};
  grid.values[toIndex({
    grid.size.x / 2,
    grid.size.y / 2,
    grid.size.z / 2,
  }, grid.size)] = 'W';

  for (auto y : std::views::iota(0u, grid.size.y))
    std::println("{}", std::ranges::subrange(
      std::begin(grid.values) + y * grid.size.x,
      std::begin(grid.values) + (y + 1) * grid.size.x
    ));
  std::println();
  for (auto current_grid : seq_snake(grid)) {
  // for (auto current_grid : growth(grid)) {
    for (auto y : std::views::iota(0u, current_grid.size.y))
      std::println("{}", std::ranges::subrange(
        std::begin(current_grid.values) + y * current_grid.size.x,
        std::begin(current_grid.values) + (y + 1) * current_grid.size.x
      ));
    std::println();
  }

  return 0;
}