module consoleapp;

import log;
import stormkit.core;

import geometry;

using namespace stormkit;
using namespace ftxui;
using namespace std::string_literals;
using namespace std::chrono_literals;
using clk = std::chrono::high_resolution_clock;

static const auto DEFAULT_PALETTE_FILE = "resources/palette.xml"s;
static const auto DEFAULT_MODEL_FILE   = "models/GoToGradient.xml"s;

static constexpr auto DEFAULT_GRID_EXTENT = std::dims<3>{1u, 59u, 59u};
static constexpr auto DEFAULT_TICKRATE = 60;

Decorator window_wrap(std::string title) {
  return [title](Element inner) {
    return window(text(title), inner);
  };
}

template <typename T>
struct GridScroll {
  T x;
  T y;
};

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
    | std::views::transform([&](auto character) noexcept {
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

  auto tabs = std::vector<std::tuple<std::string, Component>> {
    { "World", Renderer([&grid, &palette]{ return render::grid(grid, palette); }) },
    { "Potential 0", Renderer([]{ return text("<Potential 0>"); }) },
    { "Potential 1", Renderer([]{ return text("<Potential 1>"); }) },
  };

  auto tabnames = std::views::keys(tabs) | std::ranges::to<std::vector>();
  auto tabcomponents = std::views::values(tabs) | std::ranges::to<Components>();
  auto tabselect = 0;

  auto tickrate_enabled = true;
  auto tickrate = DEFAULT_TICKRATE;

  auto grid_scroll = GridScroll { 0, 0 };

  auto screen = ScreenInteractive::Fullscreen();
  // screen.TrackMouse(false);

  auto model_paused = false;
  auto pause_cv = std::condition_variable{};
  auto pause_m  = std::mutex {};
  auto program_runtime = [&](std::stop_token stop) mutable noexcept {
    auto last_time = clk::now();
    // swap the two next lines
    for (auto _ : model.program(grid)) {
      if (stop.stop_requested()) break;

      if (tickrate_enabled and tickrate != 0) {
        const auto tickperiod = std::chrono::duration_cast<clk::duration>( 1000ms / tickrate );
        const auto elapsed = clk::now() - last_time;
        const auto missing = tickperiod - std::min(elapsed, tickperiod);
        std::this_thread::sleep_for(missing);
      }
      {
        auto l = std::unique_lock{ pause_m };
        pause_cv.wait(l, [&model_paused]{ return not model_paused; });
      }

      screen.RequestAnimationFrame();
      last_time = clk::now();
    }
    model.halted = true;
    screen.RequestAnimationFrame(); 
  };

  auto program_thread = std::jthread{ program_runtime };

  auto view = Container::Horizontal({
    Container::Vertical({
      Renderer([]{
        return text("<Model Name>")
          | hcenter | border | xflex_grow;
      }),
      Renderer([&model, &palette]{
        return render::symbols(model.symbols, palette)
          | window_wrap("symbols");
      }),
      Renderer([&model, &palette]{
        return render::nodeRunner(model.program, palette)
      // Renderer([]{
      //   return text("<program>")
          | vscroll_indicator
          | yframe
          | window_wrap("program");
      }),
      Container::Vertical({
        Container::Horizontal({
          Button("play/pause", [&]{
            {
              auto l = std::lock_guard{ pause_m };
              model_paused ^= true;
            }
            pause_cv.notify_one();
          }),
          Button("reset", [&]{
            {
              auto l = std::lock_guard{ pause_m };
              model_paused = true;
            }
            pause_cv.notify_one();

            reset(model.program);
            grid = TracedGrid{extent, model.symbols[0]};
            if (model.origin) grid[grid.area().center()] = model.symbols[1];
          }),
        }),
          Slider<decltype(tickrate)>({
            .value = &tickrate,
            .direction = Direction::Right,
            .on_change = nullptr,
          })
            | Renderer(border),
        Container::Horizontal({
          Renderer([&tickrate]{
            return text(std::format("{} tick/s ", tickrate));
          }),
          Checkbox({
            .label = "tickrate",
            .checked = &tickrate_enabled,
            .transform = nullptr,
          })
            | Renderer(vcenter),
        }),
        Container::Horizontal({
          // Button("previous", []{}),
          Button("next", []{}),
        })
          | Renderer(hcenter),
      })
        | Renderer(window_wrap("controls")),
    }),
    Renderer([]{ return separator(); }),
    Container::Vertical({
      Toggle(&tabnames, &tabselect),
      Container::Tab(tabcomponents, &tabselect)
        | Renderer(
            focusPosition(grid_scroll.x, grid_scroll.y)
              | vscroll_indicator | hscroll_indicator | frame
              | border | center | flex_grow
      )
    })
      | Renderer(flex_grow),
  })
    | Renderer(flex_grow);

  screen.Loop(view);

  return 0;
}
