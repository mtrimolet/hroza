#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
import glm;
import stormkit.Core;
import ncurses;

import geometry;
import grid;

import examples;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");

  const auto palette = examples::parseXmlPalette("./resources/palette.xml");
  const auto model = std::ranges::size(args) >= 2 ? args[1] : "Grow To";
  const auto filename = model 
    | std::views::filter(std::not_fn([](auto&& c) static noexcept { return std::isspace(c); }))
    | std::ranges::to<std::string>();
  const auto example = examples::parseXmlExample(model, std::format("./models/{}.xml", filename));

  const auto square_size = 60u;
  auto grid = TracedGrid{std::dims<3>{1u, square_size, 3 * square_size}, example.symbols[0]};
  if (example.origin) grid[toSentinel(grid.extents) / 2u] = example.symbols[1];

  auto window = ncurses::window{grid.extents.extent(1), grid.extents.extent(2)};
  window.say(example.title);
  window.waitchar();

  window.setpalette(example.symbols
    | std::views::transform([&palette](auto&& s) noexcept {
        return palette.contains(s) 
          ? palette.at(s) 
          : 0xffffff;
    })
    | std::ranges::to<std::vector>()
  );

  const auto offsets = std::views::zip(example.symbols, std::views::iota(0u))
    | std::ranges::to<std::unordered_map<char, unsigned int>>();

  for (auto&& [u, value] : std::views::zip(mdiota(grid.extents), grid)) {
    if (window.hascolors()) {
      const auto offset = offsets.contains(value) ? offsets.at(value) : 0;
      window.addch(u.y, u.x, value, offset);
    } else {
      window.addch(u.y, u.x, value);
    }
  }
  window.refresh();

  for (auto changes : example.program(grid)) {
    for (auto&& [u, value] : changes) {
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