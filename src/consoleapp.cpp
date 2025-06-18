module consoleapp;

import log;
import stormkit.core;

import geometry;

using namespace stormkit;
using namespace ftxui;
using namespace std::string_literals;

static const auto DEFAULT_PALETTE_FILE = "resources/palette.xml"s;
static const auto DEFAULT_MODEL_FILE   = "models/GoToGradient.xml"s;

static constexpr auto DEFAULT_GRID_EXTENT = std::dims<3>{1u, 50u, 100u};

auto ConsoleApp::operator()(std::span<const std::string_view> args) noexcept -> int {
  auto palettefile = DEFAULT_PALETTE_FILE;
  // ilog("loading palette {}", palettefile.string());
  auto default_palette = parser::Palette(parser::document(palettefile));

  auto modelarg = std::ranges::find_if(args, [](const auto& arg) static noexcept {
    return std::ranges::cbegin(std::ranges::search(arg, "models/"s)) == std::ranges::cbegin(arg);
  });
  auto modelfile =
    modelarg != std::ranges::end(args) ? std::string{*modelarg} : DEFAULT_MODEL_FILE;
  // ilog("loading model {}", modelfile.string());

  auto model = parser::Model(parser::document(modelfile));
  auto palette = model.symbols
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

  auto extent = DEFAULT_GRID_EXTENT;
  auto grid = TracedGrid{extent, model.symbols[0]};
  if (model.origin) grid[grid.area().center()] = model.symbols[1];

  auto view = Container::Horizontal({
    Renderer([&grid, &palette]() noexcept { return render::grid(grid, palette); }),
    Renderer([&model, &palette]() noexcept { return render::model(model, palette); }),
  });

  auto screen = ScreenInteractive::TerminalOutput();
  screen.TrackMouse(false);

  auto program_thread = std::jthread{[&grid, &model, &screen](std::stop_token stop) mutable noexcept {
    for (auto&& changes : model.program(grid)) {
      if (stop.stop_requested()) return;

      // TODO find how to handle bottom-up signal using custom Component or whatever
      screen.RequestAnimationFrame(); 
    }
  }};

  screen.Loop(view);

  // auto main_loop = Loop{&screen, view};
  // while (not main_loop.HasQuitted()) {
  //   main_loop.RunOnce();
  // }

  // main_loop.RunOnce();
  // for (auto&& changes : model.program(grid)) {
  //   // TODO find how to handle bottom-up signal using custom Component or whatever
  //   screen.RequestAnimationFrame();

  //   main_loop.RunOnce();
  //   if (main_loop.HasQuitted()) break;
  // }

  return 0;
}
