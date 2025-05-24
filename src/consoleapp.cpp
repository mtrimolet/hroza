module consoleapp;

import stormkit.core;
import geometry;

import executionnode;
import rulenode;
import rewriterule;

using namespace std::string_literals;
using namespace stormkit;
using namespace ftxui;

using Palette = std::unordered_map<char, Color>;

inline constexpr auto canvasFromImage(const Image& img) noexcept -> decltype(auto) {
  return canvas(img.dimx(), img.dimy(), bindBack(&Canvas::DrawImage, 0, 0, img))
    | size(WIDTH, GREATER_THAN, img.dimx())
    | size(HEIGHT, GREATER_THAN, img.dimy());
}

inline constexpr auto rule(const RewriteRule&, const Palette&) noexcept -> Element {
  return text("<rule>");
  // auto input = Image{
  //   static_cast<int>(rule.input.extents.extent(2)),
  //   static_cast<int>(rule.input.extents.extent(1))
  // };
  // auto output = Image{
  //   static_cast<int>(rule.output.extents.extent(2)),
  //   static_cast<int>(rule.output.extents.extent(1))
  // };

  // std::ranges::for_each(
  //   std::views::zip(mdiota(rule.input.area()), rule.input, rule.output),
  //   [&input, &output, &palette](auto&& uio) noexcept {
  //     auto&& [u, i, o] = uio;

  //     auto&& ip = input.PixelAt(u.x, u.y);
  //     // TODO input should be char refering to a set, not a set ; the indirection is required
  //     ip.character = i ? '?' : '*';
  //     // ip.character = i.value_or('*');
  //     // ip.background_color = palette.at(i);

  //     auto&& op = output.PixelAt(u.x, u.y);
  //     op.character = o.value_or('*');
  //     op.background_color = o and palette.contains(*o) ? palette.at(*o) : Color::Default;
  //   }
  // );

  // return hbox({
  //   canvasFromImage(input) | border,
  //   text("->") | vcenter,
  //   canvasFromImage(output) | border,
  // });
}

inline constexpr auto ruleNode(const Action& node, const Palette& palette, std::optional<UInt> count = {}) noexcept -> Element {
  if (const auto one = node.target<One>(); one != nullptr) {
    auto nodes = Elements{text("one")};
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(one->rules | std::views::transform(bindBack(rule, palette)));
    return vbox(std::move(nodes));
  }
  if (const auto all = node.target<All>(); all != nullptr) {
    auto nodes = Elements{text("all")};
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(all->rules | std::views::transform(bindBack(rule, palette)));
    return vbox(std::move(nodes));
  }
  if (const auto prl = node.target<Prl>(); prl != nullptr) {
    auto nodes = Elements{text("prl")};
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(prl->rules | std::views::transform(bindBack(rule, palette)));
    return vbox(std::move(nodes));
  }
  return text("<unknown_rule_node>");
}

inline constexpr auto executionNode(const ExecutionNode& node, const Palette& palette) noexcept -> Element {
  if (const auto markov = node.target<Markov>(); markov != nullptr) {
    auto nodes = Elements{text("markov")};
    nodes.append_range(markov->nodes | std::views::transform(bindBack(executionNode, palette)));
    return vbox(std::move(nodes));
  }
  if (const auto sequence = node.target<Sequence>(); sequence != nullptr) {
    auto nodes = Elements{text("sequence")};
    nodes.append_range(sequence->nodes | std::views::transform(bindBack(executionNode, palette)));
    return vbox(std::move(nodes));
  }
  if (const auto limit = node.target<Limit>(); limit != nullptr) {
    // auto nodes = Elements{text(std::format("<rulenode> (?/{})", limit->count))};
    return ruleNode(limit->action, palette, limit->count);
  }
  if (const auto nolimit = node.target<NoLimit>(); nolimit != nullptr) {
    return ruleNode(nolimit->action, palette);
  }
  return text("<unknown_execution_node>");
}

auto ConsoleApp::run(std::span<const std::string_view> args) noexcept -> int {
  load_palette(DEFAULT_PALETTE_FILE);

  load_model(std::ranges::size(args) >= 2 ? args[1] : DEFAULT_MODEL_FILE);

  auto symbols_palette = model.symbols
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

  auto symbols_texture = Image{8, static_cast<int>(std::ranges::size(model.symbols)) / 8};
  std::ranges::for_each(
    std::views::zip(
      model.symbols,
      mdiota(std::dims<3>{1, symbols_texture.dimy(), symbols_texture.dimx()})
    ),
    [&](auto&& cu) noexcept {
      auto&& [character, u] = cu;
      auto&& pixel = symbols_texture.PixelAt(u.x, u.y);
      pixel.character = character;
      pixel.background_color = symbols_palette.at(character);
    }
  );

  auto symbols_view = Renderer([&]() noexcept {
    return canvasFromImage(symbols_texture);
  });
  auto program_view = Renderer([&]() noexcept {
    return executionNode(model.program, symbols_palette);
  });

  init_grid(DEFAULT_GRID_EXTENT);

  auto grid_texture = Image{
    static_cast<int>(grid.extents.extent(2)),
    static_cast<int>(grid.extents.extent(1))
  };
  auto update_grid_texture = [&](auto&& changes) noexcept {
    std::ranges::for_each(changes, [&](auto&& change) noexcept {
      auto&& [u, character] = change;
      auto&& pixel = grid_texture.PixelAt(u.x, u.y);
      pixel.character = /* character; */ " ";
      pixel.background_color = symbols_palette.at(character);
    });
  };
  update_grid_texture(std::views::zip(mdiota(grid.area()), grid));

  auto grid_view = Renderer([&]() noexcept {
    return canvasFromImage(grid_texture);
  });

  auto root_view = Renderer([&]() noexcept {
    return window(text("<model_title>"), hbox({
      grid_view->Render()
        | center | flex_grow,
      separator(),
      vbox({
        window(text("symbols"), symbols_view->Render()),
        window(text("program"), program_view->Render()),
      }),
    }));
  });

  auto screen = ScreenInteractive::Fullscreen();
  auto main_loop = Loop{&screen, root_view};

  auto program_thread = std::jthread([&](std::stop_token stop) mutable noexcept {
    for (auto&& changes : model.program(grid)) {
      if (stop.stop_requested()) return;
      update_grid_texture(changes);
      screen.RequestAnimationFrame(); // TODO find how to handle bottom-up signal using custom Component or whatever
    }
  });

  main_loop.Run();

  // while (not main_loop.HasQuitted()) {
  //   main_loop.RunOnce();
  // }

  return 0;
}
