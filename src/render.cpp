module render;

import grid;
import potentials;
import log;

using namespace ftxui;
using namespace stormkit;

namespace render {

Element canvasFromImage(const Image& img) noexcept {
  return canvas(img.dimx(), img.dimy(), std::bind_back(&Canvas::DrawImage, 0, 0, img));
}

// TODO canvas don't get sized properly
Element canvasFromImage(Image&& img) noexcept {
  return canvas(img.dimx(), img.dimy(), std::bind_back(&Canvas::DrawImage, 0, 0, std::move(img)));
}

Element grid(const ::TracedGrid<char>& g, const Palette& palette) noexcept {
  auto texture = Image{
    static_cast<int>(g.extents.extent(2)),
    static_cast<int>(g.extents.extent(1))
  };
  std::ranges::for_each(std::views::zip(mdiota(g.area()), g), [&](auto&& u_char) noexcept {
    auto [u, character] = u_char;
    auto& pixel = texture.PixelAt(u.x, u.y);
    pixel.character = /* character; */ " ";
    pixel.background_color = palette.contains(character) ? palette.at(character) : Color::Default;
  });
  auto w = texture.dimx(), h = texture.dimy();
  return canvasFromImage(std::move(texture))
    | size(WIDTH, EQUAL, w)
    | size(HEIGHT, EQUAL, h)
    | vscroll_indicator
    | hscroll_indicator
    | frame;
}

Element rule(const ::RewriteRule& rule, const Palette& palette, cpp::UInt count = 1) noexcept {
  auto input = Image{
    static_cast<int>(rule.input.extents.extent(2)),
    static_cast<int>(rule.input.extents.extent(1))
  };
  auto output = Image{
    static_cast<int>(rule.output.extents.extent(2)),
    static_cast<int>(rule.output.extents.extent(1))
  };

  std::ranges::for_each(
    std::views::zip(mdiota(rule.input.area()), rule.input, rule.output),
    [&input, &output, &palette](auto uio) noexcept {
      auto [u, i, o] = uio;

      auto& ip = input.PixelAt(u.x, u.y);
      ip.character = palette.contains(i) ? ' ' : i;
      ip.background_color = palette.contains(i) ? palette.at(i) : Color::Default;

      auto& op = output.PixelAt(u.x, u.y);
      op.character = palette.contains(i) ? ' ' : i;
      op.background_color = palette.contains(o) ? palette.at(o) : Color::Default;
    }
  );

  auto w = input.dimx(), h = input.dimy();
  return hbox({
    canvasFromImage(std::move(input))
      | size(WIDTH, EQUAL, w)
      | size(HEIGHT, EQUAL, h)
      | border,
    text("→") | vcenter,
    canvasFromImage(std::move(output))
      | size(WIDTH, EQUAL, w)
      | size(HEIGHT, EQUAL, h)
      | border,
    text(std::format("x{}", count)),
  });
}

Element potential_grid(const ::Potential& g) noexcept {
  auto texture = Image{
    static_cast<int>(g.extents.extent(2)),
    static_cast<int>(g.extents.extent(1))
  };
  auto [min_g, max_g] = std::ranges::fold_left(
    g, std::make_pair(0.0, 0.0),
    [](auto&& a, auto p) static noexcept {
      return std::make_pair(
        std::min(std::get<0>(a), p),
        std::max(std::get<1>(a), p)
      );
    }
  );

  auto normalize = [&min_g, &max_g](double t) noexcept {
    t /= t > 0.0 ? max_g : t < 0.0 ? min_g : 1.0;   // go to [-1, 1]
    t += 1.0;                                       // go to [ 0, 2]
    t /= 2.0;                                       // go to [ 0, 1]
    return t;
  };
  std::ranges::for_each(std::views::zip(mdiota(g.area()), g), [&](auto u_val) noexcept {
    auto [u, value] = u_val;
    auto& pixel = texture.PixelAt(u.x, u.y);
    auto normal = value == 0.0 or std::isnormal(value);
    pixel.character =
        value == 0.0             ? "•"
      : not std::isnormal(value) ? "*"
                                 : " ";
    pixel.background_color = Color::Interpolate(
      not normal ? 0.5 : normalize(value),
      Color::Blue, Color::Red
    );
  });
  auto w = texture.dimx(), h = texture.dimy();
  return canvasFromImage(std::move(texture))
    | size(WIDTH, EQUAL, w)
    | size(HEIGHT, EQUAL, h);
}

Element potential(char c, const Potential& pot, const Palette& palette) noexcept {
  auto col = palette.contains(c) ? palette.at(c) : Color::Default;
  return window(
    text(std::string{c}) | color(col) | inverted,
    potential_grid(pot)
  );
}

Element ruleRunner(const RuleRunner& node, const Palette& palette) noexcept {
  auto rulenode = node.rulenode.target<RuleNode>();
  if (rulenode == nullptr) return text("<unknown_rule_node>");
  
  auto elements = Elements{};

  auto tag =
      rulenode->mode == RuleNode::Mode::ONE ? "one"
    : rulenode->mode == RuleNode::Mode::ALL ? "all"
    :                                         "prl";

  auto steps = node.steps != 0 ? std::format("{}", node.steps) : std::string{"∞"};
  elements.push_back(text(std::format("{} ({}/{})", tag, node.step, steps)));

  for(
    auto irule = std::ranges::cbegin(rulenode->rules);
    irule != std::ranges::cend(rulenode->rules);
  ) {
    auto next_rule = std::ranges::find_if(
      std::ranges::subrange(irule, std::ranges::cend(rulenode->rules))
        | std::views::drop(1),
      &RewriteRule::original
    );
    elements.push_back(rule(*irule, palette, std::ranges::distance(irule, next_rule)));
    irule = next_rule;
  }
  // elements.append_range(rulenode->rules
  //   | std::views::filter(&RewriteRule::original)
  //   | std::views::transform(std::bind_back(rule, palette)));
  elements.push_back(hbox(rulenode->potentials
    | std::views::transform([&palette](const auto& p) noexcept {
        return potential(std::get<0>(p), std::get<1>(p), palette);
      })
    | std::ranges::to<Elements>()));
  return vbox(elements);
}

Element treeRunner(const TreeRunner& node, const Palette& palette, bool selected) noexcept {
  auto elements = Elements{};
  auto tag =
      node.mode == TreeRunner::Mode::SEQUENCE ? "sequence"
    :                                           "markov";
  elements.push_back(text(tag));

  elements.append_range(std::views::zip(node.nodes, std::views::iota(ioffset{ 0 }))
    | std::views::transform([&palette, &selected, current_index = node.current_index()](auto&& ni) noexcept {
        const auto& [n, i] = ni;
        return nodeRunner(n, palette, selected and current_index == i);
      }));

  auto element = vbox(std::move(elements));
  if (selected) element |= focus;
  return element;
}

Element nodeRunner(const NodeRunner& node, const Palette& palette, bool selected) noexcept {
  if (const auto t = node.target<TreeRunner>(); t != nullptr) {
    return treeRunner(*t, palette, selected);
  }
  if (const auto r = node.target<RuleRunner>(); r != nullptr) {
    return ruleRunner(*r, palette);
  }
  return text("<unknown_node_runner>");
}

Element symbols(std::string_view values, const Palette& palette) noexcept {
  auto texture = Image{8, 1 + static_cast<int>(std::ranges::size(values)) / 8};
  std::ranges::for_each(
    std::views::zip(
      values,
      mdiota(std::dims<3>{1, texture.dimy(), texture.dimx()})
    ),
    [&](auto&& cu) noexcept {
      auto [character, u] = cu;
      auto& pixel = texture.PixelAt(u.x, u.y);
      pixel.character = character;
      pixel.background_color = palette.at(character);
    }
  );

  auto w = texture.dimx(), h = texture.dimy();
  return canvasFromImage(std::move(texture))
      | size(WIDTH, EQUAL, w)
      | size(HEIGHT, EQUAL, h);
}

Element model(const ::Model& model, const Palette& palette) noexcept {
  return vbox({
    window(text("symbols"), symbols(model.symbols, palette)),
    window(text(model.halted ? "program (H)" : "program"),
           nodeRunner(model.program, palette)
             | vscroll_indicator | frame
    ),
  });
}

}
