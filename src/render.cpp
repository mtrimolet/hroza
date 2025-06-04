module render;

import grid;

using namespace ftxui;
using namespace stormkit;

namespace render {

Element canvasFromImage(const Image& img) noexcept {
  return canvas(img.dimx(), img.dimy(), bindBack(&Canvas::DrawImage, 0, 0, img));
  // return canvas(img.dimx(), img.dimy(), [&img](auto&& c) noexcept { c.DrawImage(0, 0, img); });
}

Element grid(const ::Grid<char>& g, const Palette& palette) noexcept {
  auto texture = Image{
    static_cast<int>(g.extents.extent(2)),
    static_cast<int>(g.extents.extent(1))
  };
  std::ranges::for_each(std::views::zip(mdiota(g.area()), g), [&](auto&& u_char) noexcept {
    auto&& [u, character] = u_char;
    auto&& pixel = texture.PixelAt(u.x, u.y);
    pixel.character = /* character; */ " ";
    pixel.background_color = palette.at(character);
  });
  return canvasFromImage(texture);
}

Component Grid(const ::Grid<char>& g, const Palette& palette) noexcept {
  return Renderer([&]() noexcept { return grid(g, palette); });
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
      ip.character = i;
      ip.background_color = palette.contains(o) ? palette.at(o) : Color::Default;

      auto&& op = output.PixelAt(u.x, u.y);
      op.character = o;
      op.background_color = palette.contains(o) ? palette.at(o) : Color::Default;
    }
  );

  return hbox({
    // canvasFromImage(input) | border,
    text(std::format("{}x{}", input.dimx(), input.dimy())) | border,
    text("->") | vcenter,
    // Renderer([output = std::move(output)]() noexcept { return canvasFromImage(output) | border; }),
    text(std::format("{}x{}", output.dimx(), output.dimy()))  | border,
  });
}

Component Rule(const ::RewriteRule& r, const Palette& palette) noexcept {
  return Renderer([&]() noexcept { return rule(r, palette); });
}

Element ruleNode(const Action& node, const Palette& palette, std::optional<UInt> count) noexcept {
  if (const auto one = node.target<One>(); one != nullptr) {
    auto nodes = Elements{
      text("one"),
    };
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    auto _ = std::ranges::size(one->rules);
    nodes.append_range(one->rules | std::views::transform([&palette](auto&& r) noexcept {
                       
                       return bindBack(rule, palette)(r);
                     }));
    return vbox(std::move(nodes));
  }
  if (const auto all = node.target<All>(); all != nullptr) {
    auto nodes = Elements{
      text("all"),
    };
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(all->rules | std::views::transform(bindBack(rule, palette)));
    return vbox(std::move(nodes));
  }
  if (const auto prl = node.target<Prl>(); prl != nullptr) {
    auto nodes = Elements{
      text("prl"),
    };
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(prl->rules | std::views::transform(bindBack(rule, palette)));
    return vbox(std::move(nodes));
  }
  return text("<unknown_rule_node>");
}

Component RuleNode(const Action& node, const Palette& palette, std::optional<UInt> count) noexcept {
  return Renderer([&]() noexcept { return ruleNode(node, palette, count); });
}

Element executionNode(const ::ExecutionNode& node, const Palette& palette) noexcept {
  if (const auto markov = node.target<Markov>(); markov != nullptr) {
    auto nodes = Elements{
      text("markov"),
    };
    nodes.append_range(markov->nodes | std::views::transform(bindBack(executionNode, palette)));
    return vbox(std::move(nodes));
  }
  if (const auto sequence = node.target<Sequence>(); sequence != nullptr) {
    auto nodes = Elements{
      text("sequence"),
    };
    nodes.append_range(sequence->nodes | std::views::transform(bindBack(executionNode, palette)));
    return vbox(std::move(nodes));
  }
  if (const auto limit = node.target<Limit>(); limit != nullptr) {
    return ruleNode(limit->action, palette, limit->count);
  }
  if (const auto nolimit = node.target<NoLimit>(); nolimit != nullptr) {
    return ruleNode(nolimit->action, palette);
  }
  return text("<unknown_execution_node>");
}

Component ExecutionNode(const ::ExecutionNode& node, const Palette& palette) noexcept {
  return Renderer([&]() noexcept { return executionNode(node, palette); });
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

  return canvasFromImage(texture);
}

Component Symbols(std::string_view values, const Palette& palette) noexcept {
  return Renderer([&]() noexcept { return symbols(values, palette); });
}

Element model(const ::Model& model, const Palette& palette) noexcept {
  return vbox({
    window(
      text("symbols"),
      symbols(model.symbols, palette)
    ),
    window(
      text("program"),
      executionNode(model.program, palette)
    ),
  });
}

Component Model(const ::Model& values, const Palette& palette) noexcept {
  return Renderer([&]() noexcept { return model(values, palette); });
}

}
