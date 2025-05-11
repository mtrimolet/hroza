module consoleapp;

using namespace stormkit;

auto ConsoleApp::run(std::span<const std::string_view> args) noexcept -> int {
  init_window();

  load_palette(DEFAULT_PALETTE_FILE);
  load_model(std::ranges::size(args) >= 2 ? args[1] : DEFAULT_MODEL_FILE);

  auto&& [h, w] = window.getmaxyx();
  // auto&& h = 20, w = 20;
  init_grid(std::dims<3>{1, h, w});

  window.say("Press any key to start");
  window.waitchar();

  auto&& update = [this](auto&& data) noexcept { update_cell(data); };
  std::ranges::for_each(std::views::zip(mdiota(grid.area()), grid), update);
  window.refresh();

  for (auto&& changes : model.program(grid)) {
    std::ranges::for_each(changes, update);
    window.refresh();
  }

  return 0;
}
