#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
import utils;
import ncurses;

import grid;

import examples;

using namespace examples;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");

  // auto example = examples::dual_retraction;
  // auto example = examples::fire_noise;
  // auto example = examples::basic_partitioning;
  auto example = examples::basic_dungeon_growth;

  const auto square_size = 67u;
  auto grid = TracedGrid{std::dims<3>{1u, square_size, square_size}, example.symbols[0]};
  if (example.origin) grid[div(toSentinel(grid.extents), 2u)] = example.symbols[1];

  auto window = ncurses::window{grid.extents.extent(1), grid.extents.extent(2)};
  window.say(example.title);
  window.waitchar();

  const auto offsets = std::views::zip(example.symbols, std::views::iota(0))
    | std::ranges::to<std::unordered_map<char, int>>();

  for (const auto& [u, value] : std::views::zip(mdiota(grid.extents), grid)) {
    if (offsets.contains(value))
      window.addch(u.y, u.x, value, offsets.at(value));
    else
      window.addch(u.y, u.x, value);
  }
  window.refresh();
  
  for (auto changes : example.program(grid)) {
    for (const auto& [u, value] : changes) {
      if (offsets.contains(value))
        window.addch(u.y, u.x, value, offsets.at(value));
      else
        window.addch(u.y, u.x, value);
    }
    window.refresh();
  }

  return 0;
}