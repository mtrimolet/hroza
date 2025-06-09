module render;

import grid;
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
  auto&& w = g.extents.extent(2), h = g.extents.extent(1);
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
      ip.character = i;
      ip.background_color = palette.contains(i) ? palette.at(i) : Color::Default;

      auto&& op = output.PixelAt(u.x, u.y);
      op.character = o;
      op.background_color = palette.contains(o) ? palette.at(o) : Color::Default;
    }
  );

  auto w = input.dimx(), h = input.dimy();
  return hbox({
    canvasFromImage(std::move(input))
    | size(WIDTH, EQUAL, w)
    | size(HEIGHT, EQUAL, h),
    text("->") | vcenter,
    canvasFromImage(std::move(output))
    | size(WIDTH, EQUAL, w)
    | size(HEIGHT, EQUAL, h),
  });
}

Element ruleNode(const Action& node, const Palette& palette, std::optional<UInt> count) noexcept {
  if (const auto one = node.target<One>(); one != nullptr) {
    auto nodes = Elements{
      text("one"),
    };
    if (count) nodes.push_back(text(std::format("({}/?)", *count)));
    nodes.append_range(one->rules | std::views::transform(bindBack(rule, palette)));
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

  return canvasFromImage(std::move(texture));
}

Element model(const ::Model& model, const Palette& palette) noexcept {
  return vbox({
    window(text("symbols"), symbols(model.symbols, palette)),
    window(text("program"), executionNode(model.program, palette)),
  });
}

}
