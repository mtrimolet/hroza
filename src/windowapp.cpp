module windowapp;

using namespace stormkit;

auto WindowApp::run(std::span<const std::string_view> args) noexcept -> int {
  wsi::parse_args(args);
  
  init_window();
  init_event_handler();

  load_palette(DEFAULT_PALETTE_FILE);
  load_model(std::ranges::size(args) >= 2 ? args[1] : DEFAULT_MODEL_FILE);

  auto&& h = 20, w = 20;
  init_grid(std::dims<3>{1, h, w});

  // auto&& update = [this](auto&& data) noexcept { update_cell(data); };
  // std::ranges::for_each(std::views::zip(mdiota(grid.area()), grid), update);

  auto&& program_thread = std::jthread([this](std::stop_token stop) mutable noexcept {
    while (model.program(grid)) {
      if (stop.stop_requested()) return;
      // std::ranges::for_each(changes, update);
    }
  });

  while (window.is_open()) {
    event_handler.update(window);
    std::this_thread::yield();
  }

  return 0;
}
