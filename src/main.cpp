#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
import utils;
import ncurses;

import grid;

import examples;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");

  const auto palette = examples::parseXmlPalette("./resources/palette.xml");

  // auto example = examples::parseXmlExample("Growth", "./models/Growth.xml");
  // auto example = examples::parseXmlExample("Basic Snake", "./models/BasicSnake.xml");
  // auto example = examples::parseXmlExample("Sequential Snake", "./models/SequentialSnake.xml");
  // auto example = examples::parseXmlExample("Basic Partitioning", "./models/BasicPartitioning.xml");
  // auto example = examples::parseXmlExample("Basic Brick Wall", "./models/BasicBrickWall.xml");
  // auto example = examples::parseXmlExample("Cycles", "./models/Cycles.xml");
  // auto example = examples::parseXmlExample("Dual Retraction", "./models/DualRetraction.xml");
  // auto example = examples::parseXmlExample("Fire Noise", "./models/FireNoise.xml");
  // auto example = examples::parseXmlExample("Basic Dungeon Growth", "./models/BasicDungeonGrowth.xml");
  // auto example = examples::parseXmlExample("Nystrom Dungeon", "./models/NystromDungeon.xml");
  // auto example = examples::parseXmlExample("Basic Keys", "./models/BasicKeys.xml");
  // auto example = examples::parseXmlExample("Biased Growth", "./models/BiasedGrowth.xml");
  auto example = examples::parseXmlExample("Keys", "./models/Keys.xml");

  const auto square_size = 67u;
  auto grid = TracedGrid{std::dims<3>{1u, square_size, square_size}, example.symbols[0]};
  if (example.origin) grid[div(toSentinel(grid.extents), 2u)] = example.symbols[1];

  auto window = ncurses::window{grid.extents.extent(1), grid.extents.extent(2)};
  window.say(example.title);
  window.waitchar();

  window.setpalette(example.symbols
    | std::views::transform([&palette](const auto& s) noexcept {
        return palette.contains(s) 
          ? palette.at(s) 
          : 0xffffff;
    })
    | std::ranges::to<std::vector>()
  );

  const auto offsets = std::views::zip(example.symbols, std::views::iota(0u))
    | std::ranges::to<std::unordered_map<char, unsigned int>>();

  for (const auto& [u, value] : std::views::zip(mdiota(grid.extents), grid)) {
    if (window.hascolors()) {
      const auto offset = offsets.contains(value) ? offsets.at(value) : 0;
      window.addch(u.y, u.x, value, offset);
    } else {
      window.addch(u.y, u.x, value);
    }
  }
  window.refresh();

  for (auto changes : example.program(grid)) {
    for (const auto& [u, value] : changes) {
      if (window.hascolors()) {
        const auto offset = offsets.contains(value) ? offsets.at(value) : 0;
        window.addch(u.y, u.x, value, offset);
      } else {
        window.addch(u.y, u.x, value);
      }
    }
    window.refresh();
  }

  return 0;
}