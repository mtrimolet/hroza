module consoleapp;

using namespace stormkit;
using namespace ftxui;

using Palette = std::unordered_map<char, Color>;

inline constexpr auto update_texture(Image& img, const Palette& palette, const std::tuple<math::Vector3U, char>& value) noexcept -> decltype(auto) {
  auto&& [u, character] = value;
  auto&& pixel = img.PixelAt(u.x, u.y);
  pixel.character = character;
  pixel.background_color = palette.at(character);
}

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
    ) | std::views::transform([](auto&& p) static noexcept { auto&& [a, b] = p; return std::make_pair(b, a); }),
    // bindFront(update_texture, palette_texture, model_palette)
    [&](auto&& cu) noexcept {
      auto&& [u, character] = cu;
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

  // auto update_grid_texture = bindFront(update_texture, grid_texture, model_palette);
  auto update_grid_texture = [&](auto&& c) noexcept {
    auto&& [u, character] = c;
    auto&& pixel = grid_texture.PixelAt(u.x, u.y);
    pixel.character = character;
    pixel.background_color = model_palette.at(character);
  };
  std::ranges::for_each(std::views::zip(mdiota(grid.area()), grid), update_grid_texture);

  auto screen = ScreenInteractive::Fullscreen();
  auto main_loop = Loop{&screen, Renderer([&]() noexcept {
    return window(text("<model_title>"), hbox({
      canvas(grid_texture.dimx(), grid_texture.dimy(),
             bindBack(&Canvas::DrawImage, 0, 0, grid_texture))
        | size(WIDTH, GREATER_THAN, grid_texture.dimx())
        | size(HEIGHT, GREATER_THAN, grid_texture.dimy()),
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

  // for (auto&& changes : model.program(grid)) {
  //   if (main_loop.HasQuitted()) break;
  //   std::ranges::for_each(changes, [&](auto&& c) noexcept { auto&& [u, character] = c; update_grid_texture(std::make_tuple(u, character)); });
  //   main_loop.RunOnce();
  //   // std::this_thread::yield();
  //   // std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // }

  auto program_thread = std::jthread([&](std::stop_token stop) mutable noexcept {
    std::ranges::for_each(std::views::zip(mdiota(grid.area()), grid), update_grid_texture);
    for (auto&& changes : model.program(grid)) {
      if (stop.stop_requested()) return;
      std::ranges::for_each(changes, [&](auto&& c) noexcept { auto&& [u, character] = c; update_grid_texture(std::make_tuple(u, character)); });
      screen.RequestAnimationFrame();
    }
  });

  main_loop.Run();

  // while (not main_loop.HasQuitted()) {
  //   main_loop.RunOnce();
  // }

  return 0;
}
