module render;

import grid;
import forceengine;
import log;

using namespace ftxui;
using namespace stormkit;

namespace render {

Element canvasFromImage(const Image& img) noexcept {
  return canvas(img.dimx(), img.dimy(), bindBack(&Canvas::DrawImage, 0, 0, img));
}

// TODO here's the issue, canvas don't get sized properly
Element canvasFromImage(Image&& img) noexcept {
  return canvas(img.dimx(), img.dimy(), bindBack(&Canvas::DrawImage, 0, 0, std::move(img)));
}

Element grid(const ::TracedGrid<char>& g, const Palette& palette) noexcept {
  auto texture = Image{
    static_cast<int>(g.extents.extent(2)),
    static_cast<int>(g.extents.extent(1))
  };
  std::ranges::for_each(std::views::zip(mdiota(g.area()), g), [&](auto&& u_char) noexcept {
    auto&& [u, character] = u_char;
    auto&& pixel = texture.PixelAt(u.x, u.y);
    pixel.character = /* character; */ " ";
    pixel.background_color = palette.contains(character) ? palette.at(character) : Color::Default;
  });
  auto&& w = texture.dimx(), h = texture.dimy();
  return canvasFromImage(std::move(texture))
    | size(WIDTH, EQUAL, w)
    | size(HEIGHT, EQUAL, h);
}

Element rule(const ::RewriteRule& rule, const Palette& palette) noexcept {
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
    [&input, &output, &palette](auto&& uio) noexcept {
      auto&& [u, i, o] = uio;

      auto&& ip = input.PixelAt(u.x, u.y);
      ip.character = palette.contains(i) ? ' ' : i;
      ip.background_color = palette.contains(i) ? palette.at(i) : Color::Default;

      auto&& op = output.PixelAt(u.x, u.y);
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
    text("->") | vcenter,
    canvasFromImage(std::move(output))
      | size(WIDTH, EQUAL, w)
      | size(HEIGHT, EQUAL, h)
      | border,
  });
}

Element potential_grid(const ::Potential& g) noexcept {
  auto texture = Image{
    static_cast<int>(g.extents.extent(2)),
    static_cast<int>(g.extents.extent(1))
  };
  auto [min_g, max_g] = std::ranges::fold_left(g, std::make_pair(0.0, 0.0), [](auto&& a, auto&& p) static noexcept {
    return std::make_pair(
      std::min(std::get<0>(a), p),
      std::max(std::get<1>(a), p)
    );
  });

  auto normalize = [&min_g, &max_g](double t) noexcept {
    t /= t > 0.0 ? max_g : t < 0.0 ? min_g : 1.0; // go to [-1, 1]
    t += 1.0;                                       // go to [0, 2]
    t /= 2.0;                                       // go to [0, 1]
    return t;
  };
  std::ranges::for_each(std::views::zip(mdiota(g.area()), g), [&](auto&& u_val) noexcept {
    auto&& [u, value] = u_val;
    auto&& pixel = texture.PixelAt(u.x, u.y);
    auto&& normal = value == 0.0 or std::isnormal(value);
    pixel.character = value == 0.0 ? "•" : value == std::numeric_limits<double>::signaling_NaN() ? "*" : " ";
    pixel.background_color = normal ? Color::Interpolate(normalize(value), Color::Blue, Color::Red) : Color::Default;
  });
  auto&& w = texture.dimx(), h = texture.dimy();
  return canvasFromImage(std::move(texture))
    | size(WIDTH, EQUAL, w)
    | size(HEIGHT, EQUAL, h);
}

Element potential(char c, const Potential& pot, const Palette& palette) noexcept {
  auto&& col = palette.contains(c) ? palette.at(c) : Color::Default;
  return window(
    text(std::string{c}) | color(col) | inverted,
    potential_grid(pot)
  );
}

Element ruleNode(const Action& node, const Palette& palette, std::optional<UInt> count) noexcept {
  if (const auto one = node.target<One>(); one != nullptr) {
    auto nodes = Elements{
      text("one"),
    };
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(one->rules
      | std::views::filter(std::not_fn(&RewriteRule::transformed))
      | std::views::transform(bindBack(rule, palette)));
    nodes.push_back(hbox(one->dijkstra.potentials
      | std::views::transform([&palette](auto&& p) noexcept {
          return potential(std::get<0>(p), std::get<1>(p), palette);
        })
      | std::ranges::to<Elements>()));
    return vbox(std::move(nodes));
  }
  if (const auto all = node.target<All>(); all != nullptr) {
    auto nodes = Elements{
      text("all"),
    };
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(all->rules
      | std::views::filter(std::not_fn(&RewriteRule::transformed))
      | std::views::transform(bindBack(rule, palette)));
    nodes.push_back(hbox(all->dijkstra.potentials
      | std::views::transform([&palette](auto&& p) noexcept {
          return potential(std::get<0>(p), std::get<1>(p), palette);
        })
      | std::ranges::to<Elements>()));
    return vbox(std::move(nodes));
  }
  if (const auto prl = node.target<Prl>(); prl != nullptr) {
    auto nodes = Elements{
      text("prl"),
    };
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(prl->rules
      | std::views::filter(std::not_fn(&RewriteRule::transformed))
      | std::views::transform(bindBack(rule, palette)));
    return vbox(std::move(nodes));
  }
  return text("<unknown_rule_node>");
}

Element executionNode(const ::ExecutionNode& node, const Palette& palette, bool selected) noexcept {
  if (const auto markov = node.target<Markov>(); markov != nullptr) {
    auto nodes = Elements{
      text("markov"),
    };
    nodes.append_range(std::views::zip(markov->nodes, std::views::iota(0))
      | std::views::transform([&palette, &selected, current_index = markov->current_index()](auto&& ni) noexcept {
          auto&& [n, i] = ni;
          return executionNode(n, palette, selected and i == current_index);
        }));
    if (selected) return vbox(std::move(nodes)) | focus;
    else return vbox(std::move(nodes));
  }
  if (const auto sequence = node.target<Sequence>(); sequence != nullptr) {
    auto nodes = Elements{
      text("sequence"),
    };
    nodes.append_range(std::views::zip(sequence->nodes, std::views::iota(0))
      | std::views::transform([&palette, &selected, current_index = sequence->current_index()](auto&& ni) noexcept {
          auto&& [n, i] = ni;
          return executionNode(n, palette, selected and i == current_index);
        }));
    if (selected) return vbox(std::move(nodes)) | focus;
    else return vbox(std::move(nodes));
  }
  if (const auto limit = node.target<Limit>(); limit != nullptr) {
    auto n = ruleNode(limit->action, palette, limit->count);
    if (selected) return hbox({std::move(n), text("<")}) | focus;
    else return hbox({std::move(n), text(" ")});
  }
  if (const auto nolimit = node.target<NoLimit>(); nolimit != nullptr) {
    auto n = ruleNode(nolimit->action, palette);
    if (selected) return hbox({std::move(n), text("<")}) | focus;
    else return hbox({std::move(n), text(" ")});
  }
  return text("<unknown_execution_node>");
}

Element symbols(std::string_view values, const Palette& palette) noexcept {
  auto texture = Image{8, 1 + static_cast<int>(std::ranges::size(values)) / 8};
  std::ranges::for_each(
    std::views::zip(
      values,
      mdiota(std::dims<3>{1, texture.dimy(), texture.dimx()})
    ),
    [&](auto&& cu) noexcept {
      auto&& [character, u] = cu;
      auto&& pixel = texture.PixelAt(u.x, u.y);
      pixel.character = character;
      pixel.background_color = palette.at(character);
    }
  );

  auto&& w = texture.dimx(), h = texture.dimy();
  return canvasFromImage(std::move(texture))
      | size(WIDTH, EQUAL, w)
      | size(HEIGHT, EQUAL, h);
}

Element model(const ::Model& model, const Palette& palette) noexcept {
  return vbox({
    window(text("symbols"), symbols(model.symbols, palette)),
    window(text("program"), executionNode(model.program, palette) | vscroll_indicator | yframe),
  });
}

}
