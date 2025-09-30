module consoleapp;

import log;
import stormkit.core;

import geometry;
import rulenode;

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

struct Controls {
  bool tickrate_enabled = true;
  int tickrate = DEFAULT_TICKRATE;
  bool model_paused = false;
  std::condition_variable pause_cv = {};
  std::mutex pause_m = {};

  std::function<void()> onReset = nullptr;
  
  void play_pause() {
    {
      auto l = std::lock_guard{ pause_m };
      model_paused ^= true;
    }
    pause_cv.notify_one();
  }

  void reset() {
    {
      auto l = std::lock_guard{ pause_m };
      model_paused = true;
    }
    pause_cv.notify_one();

    onReset();
  }

  void sleep_missing(clk::time_point last_time) {
    if (tickrate_enabled and tickrate != 0) {
      const auto tickperiod = std::chrono::duration_cast<clk::duration>( 1000ms / tickrate );
      const auto elapsed = clk::now() - last_time;
      const auto missing = tickperiod - std::min(elapsed, tickperiod);
      std::this_thread::sleep_for(missing);
    }
  }

  void maybe_pause() {
    auto l = std::unique_lock{ pause_m };
    pause_cv.wait(l, [&paused = model_paused]{ return not paused; });
  }
};

Component ControlsView(Controls& controls) {
  return Container::Vertical({
    Container::Horizontal({
      Button("play/pause", std::bind_front(&Controls::play_pause, &controls)),
      Button("reset", std::bind_front(&Controls::reset, &controls)),
    }),
    Slider<decltype(controls.tickrate)>({
      .value = &controls.tickrate,
      .direction = Direction::Right,
      .on_change = nullptr,
    })
      | Renderer(border),
    Container::Horizontal({
      Renderer([&tickrate = controls.tickrate]{
        return text(std::format("{} tick/s ", tickrate));
      }),
      Checkbox({
        .label = "tickrate",
        .checked = &controls.tickrate_enabled,
        .transform = nullptr,
      })
        | Renderer(vcenter),
    }),
    Container::Horizontal({
      // Button("previous", []{}),
      Button("next", []{}),
    })
      | Renderer(hcenter),
  });
}

template <typename T>
struct GridScroll {
  T x;
  T y;
};

using namespace std::string_literals;

Component WorldAndPotentials(const TracedGrid<char>& grid, const Model& model, const render::Palette& palette) {
  struct Impl : ComponentBase {
    const Model& model;

    std::vector<std::string> tabnames = {};
    Components tabcomponents = {};
    int tabselect = 0;
    GridScroll<int> grid_scroll = { 0, 0 };

    Impl(const TracedGrid<char>& grid, const Model& _model, const render::Palette& palette)
    : model(_model)
    {
      tabnames = { "World" };
      tabcomponents = { Renderer([&grid, &palette]{
        return render::grid(grid, palette);
      }) };

      Add(Container::Vertical({
        Toggle(&tabnames, &tabselect),
        Container::Tab(tabcomponents, &tabselect)
          | Renderer([&grid_scroll = grid_scroll](Element e){
              return e | focusPosition(grid_scroll.x, grid_scroll.y)
                | vscroll_indicator | hscroll_indicator | frame
                | border | center | flex_grow;
          })
      }));
    }

    void ClearPotentials() {
      tabselect = 0;
      tabnames = { tabnames[0] };
      tabcomponents = { tabcomponents[0] };
    }

    void RefreshPotentials() {
      ClearPotentials();
      if (auto r = current(model.program).target<RuleNode>(); r != nullptr) {
        for (const auto& [sym, p] : r->potentials) {
          tabnames.push_back(std::format("{}", sym));
          tabcomponents.push_back(Renderer([&p]{
            return render::potential_grid(p);
          }));
        }
      }
    }

    void OnAnimation(animation::Params& params) {
      RefreshPotentials();
      ComponentBase::OnAnimation(params);
    }
  };
  return Make<Impl>(grid, model, palette);
}

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

  auto controls = Controls {
    .tickrate = DEFAULT_TICKRATE,
    .onReset = [&grid, &extent, &model]{
      reset(model.program);
      grid = TracedGrid{extent, model.symbols[0]};
      if (model.origin) grid[grid.area().center()] = model.symbols[1];
    },
  };

  auto program_runtime = [&grid, &model, &controls](std::stop_token stop) mutable noexcept {
    auto last_time = clk::now();
    // swap the two next lines
    for (auto _ : model.program(grid)) {
      animation::RequestAnimationFrame();

      if (stop.stop_requested()) break;

      controls.sleep_missing(last_time);
      controls.maybe_pause();

      last_time = clk::now();
    }

    model.halted = true;
    animation::RequestAnimationFrame(); 
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
          | vscroll_indicator
          | yframe
          | window_wrap("program");
      }),
      ControlsView(controls)
        | Renderer(window_wrap("controls")),
    }),
    Renderer([]{ return separator(); }),
    WorldAndPotentials(grid, model, palette)
      | Renderer(flex_grow),
  })
    | Renderer(flex_grow);

  auto screen = ScreenInteractive::Fullscreen();
  screen.TrackMouse(false);
  screen.Loop(view);

  return 0;
}
