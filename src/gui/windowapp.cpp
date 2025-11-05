module gui.windowapp;

import log;
import stormkit.core;

import grid;
import geometry;

import engine.model;
import engine.rulenode;
import parser;
import controls;

import stormkit.wsi;
// import gui.render;

using namespace stormkit;
using namespace std::string_literals;
using clk = std::chrono::high_resolution_clock;

static const auto DEFAULT_PALETTE_FILE = "resources/palette.xml"s;
static const auto DEFAULT_MODEL_FILE   = "models/GoToGradient.xml"s;

static constexpr auto DEFAULT_GRID_EXTENT = std::dims<3>{1u, 59u, 59u};
static constexpr auto DEFAULT_TICKRATE = 60;

static constexpr auto WINDOW_TITLE = "Hroza";
static constexpr auto WINDOW_SIZE  = math::Extent2<u32>{800, 600};

auto WindowApp::operator()(std::span<const std::string_view> args) noexcept -> int {
  wsi::parse_args(args);
  
  auto palettefile = DEFAULT_PALETTE_FILE;
  auto default_palette = parser::Palette(parser::document(palettefile));

  auto modelarg = std::ranges::find_if(args, [](const auto& arg) static noexcept {
    return std::ranges::cbegin(std::ranges::search(arg, "models/"s)) == std::ranges::cbegin(arg);
  });
  auto modelfile =
    modelarg != std::ranges::end(args) ? std::string{*modelarg} : DEFAULT_MODEL_FILE;

  auto model = parser::Model(parser::document(modelfile));

  auto extent = DEFAULT_GRID_EXTENT;
  auto grid = TracedGrid{extent, model.symbols[0]};
  if (model.origin) grid[grid.area().center()] = model.symbols[1];

  auto controls = Controls {
    .tickrate = DEFAULT_TICKRATE,
    .onReset = [&grid, &model]{
      reset(model.program);
      grid = TracedGrid{grid.extents, model.symbols[0]};
      if (model.origin) grid[grid.area().center()] = model.symbols[1];
    },
  };

  auto program_thread = std::jthread{ [&grid, &model, &controls](std::stop_token stop) mutable noexcept {
    auto last_time = clk::now();
    // swap the two next lines
    for (auto _ : model.program(grid)) {
      // animation::RequestAnimationFrame();

      if (stop.stop_requested()) break;

      controls.sleep_missing(last_time);
      controls.maybe_pause();

      last_time = clk::now();
    }

    model.halted = true;
    // animation::RequestAnimationFrame(); 
  } };

  auto window = wsi::Window::open(
    WINDOW_TITLE, WINDOW_SIZE, wsi::WindowFlag::DEFAULT
  );

  window.on<wsi::EventType::CLOSED>([] static noexcept { return true; });

  window.event_loop([] static noexcept {
    std::this_thread::yield();
  });

  return 0;
}
