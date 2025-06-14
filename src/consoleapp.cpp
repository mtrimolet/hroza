module consoleapp;

import log;
import stormkit.core;

import geometry;

using namespace stormkit;
using namespace ftxui;

auto ConsoleApp::run(std::span<const std::string_view> args) noexcept -> int {
  load_palette(DEFAULT_PALETTE_FILE);

  auto modelfile = std::ranges::size(args) >= 2 ? args[1] : DEFAULT_MODEL_FILE;
  load_model(modelfile);

  init_grid(DEFAULT_GRID_EXTENT);

  auto view = Container::Horizontal({
    Renderer([this]() noexcept { return render::grid(grid, palette); }),
    Renderer([this]() noexcept { return render::model(model, palette); }),
  });

  auto screen = ScreenInteractive::Fullscreen();
  screen.TrackMouse(false);

  // auto program_thread = std::jthread{[this, &screen](std::stop_token stop) mutable noexcept {
  //   for (auto&& changes : model.program(grid)) {
  //     if (stop.stop_requested()) return;

  //     // TODO find how to handle bottom-up signal using custom Component or whatever
  //     screen.RequestAnimationFrame(); 
  //   }
  // }};

  // screen.Loop(view);

  auto main_loop = Loop{&screen, view};
  // // while (not main_loop.HasQuitted()) {
  // //   main_loop.RunOnce();
  // // }

  main_loop.RunOnce();
  for (auto&& changes : model.program(grid)) {
    // TODO find how to handle bottom-up signal using custom Component or whatever
    screen.RequestAnimationFrame();

    main_loop.RunOnce();
    if (main_loop.HasQuitted()) break;
  }

  return 0;
}

auto ConsoleApp::load_palette(const std::filesystem::path& palettefile) noexcept -> void {
  ilog("loading palette {}", palettefile.string());
  default_palette = parser::Palette(parser::document(palettefile));
}

auto ConsoleApp::load_model(const std::filesystem::path& modelfile) noexcept -> void {
  ilog("loading model {}", modelfile.string());
  model = parser::Model(parser::document(modelfile));
  palette = model.symbols
    | std::views::transform([&](auto&& character) noexcept {
        return std::make_pair(
          character,
          default_palette.contains(character)
            ? Color::RGB((default_palette.at(character) >> 16) & 0xff,
                         (default_palette.at(character) >>  8) & 0xff,
                         (default_palette.at(character)      ) & 0xff)
            : Color::Default
        );
    })
    | std::ranges::to<render::Palette>();
}

auto ConsoleApp::init_grid(TracedGrid<char>::Extents extents) noexcept -> void {
  grid = TracedGrid{extents, model.symbols[0]};
  if (model.origin) grid[grid.area().center()] = model.symbols[1];
}
