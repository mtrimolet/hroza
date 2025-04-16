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

  auto&& window = ncurses::window{};

  auto&& palette = examples::parseXmlPalette("./resources/palette.xml");
  auto&& model = std::ranges::size(args) >= 2 ? args[1] : "Dense SAW";

  window.say(model);
  window.waitchar();

  window.say("Loading model...");
  auto&& filename = model 
    | std::views::filter(std::not_fn([](auto&& c) static noexcept { return std::isspace(c); }))
    | std::ranges::to<std::string>();
  auto&& example = examples::parseXmlExample(model, std::format("./models/{}.xml", filename));

  auto&& grid = TracedGrid{std::dims<3>{1u, std::get<0>(window.getmaxyx()), std::get<1>(window.getmaxyx())}, example.symbols[0]};
  if (example.origin) grid[(grid.area() / 2u).outerbound()] = example.symbols[1];

  window.setpalette(example.symbols
    | std::views::transform([&palette](auto&& s) noexcept {
        return palette.contains(s) 
          ? palette.at(s) 
          : 0xffffff;
    })
    | std::ranges::to<std::vector>()
  );

  auto&& offsets = std::views::zip(example.symbols, std::views::iota(0u))
    | std::ranges::to<std::unordered_map<char, unsigned int>>();

  auto&& addch = [&window, &offsets](auto&& _value) noexcept {
    auto&& [u, value] = _value;
    if (window.hascolors()) {
      auto&& offset = offsets.contains(value) ? offsets.at(value) : 0;
      window.addch(u.y, u.x, value, offset);
    } else {
      window.addch(u.y, u.x, value);
    }
  };

  std::ranges::for_each(std::views::zip(mdiota(grid.area()), grid), addch);
  window.refresh();
  window.waitchar();

  for (auto&& changes : example.program(grid)) {
    std::ranges::for_each(changes, addch);
    window.refresh();
  }

  return 0;
}
