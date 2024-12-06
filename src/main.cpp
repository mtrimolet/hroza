#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
// import glm;
import stormkit.Core;
import ranges;
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

  auto grid = Grid<chtype>{{67u, 67u, 1u}, 'B'};
  grid[{
    grid.size.x / 2,
    grid.size.y / 2,
    grid.size.z / 2,
}] = 'W';

  auto window = ncurses::window{grid.size.y, grid.size.x};
  window.say("hello!");
  window.waitchar();

  const auto lines = std::views::iota(0u, std::ranges::size(grid.values))
    | std::views::stride(grid.size.x)
    | std::views::transform([&grid](auto i) noexcept {
        return std::ranges::subrange(
          std::ranges::begin(grid.values) + i,
          std::ranges::begin(grid.values) + i + grid.size.x,
        );
    });
  // for (auto [line, y] : std::views::zip(grid.values | std::views::chunk(grid.size.x), std::views::iota(0u))) {
  for (auto [line, y] : std::views::zip(lines, std::views::iota(0u))) {
    window.addchstr(y, 0u, line | std::ranges::to<std::vector>());
  }
  window.refresh();
  // for (auto current_grid : /*growth*/seq_snake(grid)) {
  //   const auto lines = std::views::iota(0u, std::ranges::size(current_grid.values))
  //     | std::views::stride(current_grid.size.x)
  //     | std::views::transform([&current_grid](auto i) noexcept {
  //         return std::ranges::subrange(
  //           std::ranges::begin(current_grid.values) + i,
  //           std::ranges::begin(current_grid.values) + i + current_grid.size.x,
  //         );
  //     });
  //   // for (auto [line, y] : std::views::zip(current_grid.values | std::views::chunk(current_grid.size.x), std::views::iota(0u))) {
  //   for (auto [line, y] : std::views::zip(lines, std::views::iota(0u))) {
  //     window.addchstr(y, 0u, line | std::ranges::to<std::vector>());
  //   }
  //   window.refresh();
  // }
  for (auto changes : /*growth*/seq_snake(grid)) {
    if (std::ranges::empty(changes)) continue;
    for (auto &[u, value] : changes) {
      window.addch(u.y, u.x, value);
    }
    window.refresh();
  }

  return 0;
}