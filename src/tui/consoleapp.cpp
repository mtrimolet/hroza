module tui.consoleapp;

import log;
import stormkit.core;

import grid;
import geometry;

import engine.model;
import engine.rulenode;
import parser;
import controls;

import ftxui;
import tui.render;

namespace stdr = std::ranges;
namespace stdv = std::views;

using namespace ftxui;
using namespace std::string_literals;
using clk = std::chrono::high_resolution_clock;

static const auto DEFAULT_PALETTE_FILE = "resources/palette.xml"s;
static const auto DEFAULT_MODEL_FILE   = "models/GoToGradient.xml"s;

static constexpr auto DEFAULT_GRID_EXTENT = std::dims<3>{1u, 59u, 59u};
static constexpr auto DEFAULT_TICKRATE = 60;

auto ConsoleApp::operator()(std::span<const std::string_view> args) noexcept -> int {
  auto palettefile = DEFAULT_PALETTE_FILE;
  auto default_palette = parser::Palette(parser::document(palettefile));

  auto modelarg = stdr::find_if(args, [](const auto& arg) static noexcept {
    return stdr::cbegin(stdr::search(arg, "models/"s)) == stdr::cbegin(arg);
  });
  auto modelfile =
    modelarg != stdr::end(args) ? std::string{ *modelarg }
                                : DEFAULT_MODEL_FILE;

  auto model = parser::Model(parser::document(modelfile));
  auto palette = model.symbols
    | stdv::transform([&](auto character) noexcept {
        return std::make_pair(
          character,
          default_palette.contains(character)
            ? Color::RGB((default_palette.at(character) >> 16) & 0xff,
                         (default_palette.at(character) >>  8) & 0xff,
                         (default_palette.at(character)      ) & 0xff)
            : Color::Default
        );
    })
    | stdr::to<render::Palette>();

  auto extent = DEFAULT_GRID_EXTENT;
  auto grid = TracedGrid{ extent, model.symbols[0] };
  if (model.origin) grid[grid.area().center()] = model.symbols[1];

  auto controls = Controls {
    .tickrate = DEFAULT_TICKRATE,
    .onReset = [&grid, &model]{
      reset(model.program);
      grid = { grid.extents, model.symbols[0] };
      if (model.origin) grid[grid.area().center()] = model.symbols[1];
    },
  };

  auto program_thread = std::jthread{ [&grid, &model, &controls](std::stop_token stop) mutable noexcept {
    auto last_time = clk::now();
    // swap the two next lines
    for (auto _ : model.program(grid)) {
      animation::RequestAnimationFrame();

      if (stop.stop_requested()) break;

      controls.rate_limit(last_time);
      controls.wait_unpause();

      last_time = clk::now();
    }

    model.halted = true;
    animation::RequestAnimationFrame(); 
  } };

  auto view = render::MainView(grid, model, controls, palette);

  auto screen = ScreenInteractive::Fullscreen();
  // screen.TrackMouse(false);
  screen.Loop(view);

  return 0;
}
