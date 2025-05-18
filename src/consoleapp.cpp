module consoleapp;

import stormkit.core;
import geometry;

using namespace stormkit;
using namespace ftxui;

using Palette = std::unordered_map<char, Color>;

auto ConsoleApp::run([[maybe_unused]] std::span<const std::string_view> args) noexcept -> int {
  load_palette(DEFAULT_PALETTE_FILE);

  load_model(std::ranges::size(args) >= 2 ? args[1] : DEFAULT_MODEL_FILE);

  auto model_palette = model.symbols
    | std::views::transform([&](auto&& character) noexcept {
        return std::make_pair(
          character,
          palette.contains(character)
            ? Color::RGB((palette.at(character) >> 16) & 0xff,
                         (palette.at(character) >>  8) & 0xff,
                         (palette.at(character)      ) & 0xff)
            : Color::Default
        );
    })
    | std::ranges::to<Palette>();

  auto palette_texture = Image{8, static_cast<int>(std::ranges::size(model.symbols)) / 8};
  std::ranges::for_each(
    std::views::zip(
      model.symbols,
      mdiota(std::dims<3>{1, palette_texture.dimy(), palette_texture.dimx()})
    ),
    [&](auto&& cu) noexcept {
      auto&& [character, u] = cu;
      auto&& pixel = palette_texture.PixelAt(u.x, u.y);
      pixel.character = character;
      pixel.background_color = model_palette.at(character);
    }
  );

  init_grid(DEFAULT_GRID_EXTENT);

  auto grid_texture = Image{
    static_cast<int>(grid.extents.extent(2)),
    static_cast<int>(grid.extents.extent(1))
  };
  auto update_grid_texture = [&](auto&& changes) noexcept {
    std::ranges::for_each(changes, [&](auto&& change) noexcept {
      auto&& [u, character] = change;
      auto&& pixel = grid_texture.PixelAt(u.x, u.y);
      pixel.character = character;
      pixel.background_color = model_palette.at(character);
    });
  };
  update_grid_texture(std::views::zip(mdiota(grid.area()), grid));

  auto screen = ScreenInteractive::Fullscreen();
  auto main_loop = Loop{&screen, Renderer([&]() noexcept {
    return window(text("<model_title>"), hbox({
      canvas(grid_texture.dimx(), grid_texture.dimy(),
             bindBack(&Canvas::DrawImage, 0, 0, grid_texture))
        | size(WIDTH, GREATER_THAN, grid_texture.dimx())
        | size(HEIGHT, GREATER_THAN, grid_texture.dimy())
        | center | flex_grow,
      separator(),
      vbox({
        window(text("symbols"), canvas(
          palette_texture.dimx(), palette_texture.dimy(),
          bindBack(&Canvas::DrawImage, 0, 0, palette_texture)
        )),
        window(text("program"), text("<instruction_tree>")),
      }),
    }));
  })};

  auto program_thread = std::jthread([&](std::stop_token stop) mutable noexcept {
    for (auto&& changes : model.program(grid)) {
      if (stop.stop_requested()) return;
      update_grid_texture(changes);
      screen.RequestAnimationFrame();
    }
  });

  main_loop.Run();

  // while (not main_loop.HasQuitted()) {
  //   main_loop.RunOnce();
  // }

  return 0;
}
