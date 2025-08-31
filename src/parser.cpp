module parser;

import log;

using namespace stormkit;
using namespace std::literals;

namespace parser {

inline constexpr auto is_tag(std::string_view tag) noexcept -> decltype(auto) {
  return [tag](const pugi::xml_node& c) noexcept {
    return c.name() == tag;
  };
}

auto Model(const pugi::xml_document& xmodel) noexcept -> ::Model {
  auto&& xnode = xmodel.first_child();

  ensures(xnode.attribute("values"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "values", "[root]", xnode.offset_debug()));
  auto symbols = std::string{xnode.attribute("values").as_string()};
  ensures(!std::ranges::empty(symbols),
          std::format("empty '{}' attribute in '{}' node [:{}]", "values", "[root]", xnode.offset_debug()));
  // ensure no duplicate
  auto&& origin = xnode.attribute("origin").as_bool(false);

  auto unions = RewriteRule::Unions{ { '*', symbols | std::ranges::to<std::set>() } };
  unions.insert_range(symbols
    | std::views::transform([](auto&& c) static noexcept { 
        return std::make_pair(c, std::set{ c });
    }));

  auto program = NodeRunner(xnode, unions);
  if (const auto p = program.target<TreeRunner>(); p == nullptr)
    program = TreeRunner{TreeRunner::Mode::MARKOV, {std::move(program)}};

  return ::Model{
    // title,
    std::move(symbols), std::move(unions),
    std::move(origin),
    std::move(program)
  };
}

auto Union(const pugi::xml_node& xnode) noexcept -> decltype(auto) {
  ensures(xnode.attribute("symbol"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "symbol", "union", xnode.offset_debug()));
  auto&& symbol_str = std::string{xnode.attribute("symbol").as_string()};
  ensures(!std::ranges::empty(symbol_str),
          std::format("empty '{}' attribute in '{}' node [:{}]", "symbol", "union", xnode.offset_debug()));
  ensures(std::ranges::size(symbol_str) == 1,
          std::format("only one character allowed for '{}' attribute of '{}' node [:{}]", "symbol", "union", xnode.offset_debug()));

  ensures(xnode.attribute("values"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "values", "union", xnode.offset_debug()));
  auto&& values = std::string{xnode.attribute("values").as_string()};
  ensures(!std::ranges::empty(values),
          std::format("empty '{}' attribute in '{}' node [:{}]", "values", "union", xnode.offset_debug()));

  return std::tuple{ symbol_str[0], std::move(values) | std::ranges::to<std::set>() };
}

auto NodeRunner(
  const pugi::xml_node& xnode,
  RewriteRule::Unions unions,
  std::string_view symmetry
) noexcept -> ::NodeRunner {
  symmetry = xnode.attribute("symmetry").as_string(std::data(symmetry));

  unions.insert_range(xnode.children("union") 
    | std::views::transform(Union));

  auto&& tag = xnode.name();
  if (tag == "sequence"s
   or tag == "markov"s
  ) {
    auto mode = tag == "sequence"s ? TreeRunner::Mode::SEQUENCE : TreeRunner::Mode::MARKOV;
    return TreeRunner{
      mode,
      xnode.children()
        | std::views::filter(std::not_fn(is_tag("union")))
        | std::views::transform(std::bind_back(NodeRunner, unions, symmetry))
        | std::ranges::to<std::vector>()
    };
  }
  if (tag == "one"s
   or tag == "prl"s
   or tag == "all"s
  ) {
    return RuleRunner{
      RuleNode(xnode, unions, symmetry),
      xnode.attribute("steps").as_uint(0)
    };
  }

  ensures(false, std::format("unknown tag '{}' [:{}]", tag, xnode.offset_debug()));
  std::unreachable();
}

auto RuleNode(
  const pugi::xml_node& xnode,
  RewriteRule::Unions unions,
  std::string_view symmetry
) noexcept -> ::RuleNode {
  auto&& tag = xnode.name();
  auto mode =
      tag == "one"s ? ::RuleNode::Mode::ONE
    : tag == "all"s ? ::RuleNode::Mode::ALL
    :                 ::RuleNode::Mode::PRL;

  if (xnode.attribute("search").as_bool(false)) {
    return ::RuleNode{
      mode, Rules(xnode, unions, symmetry),
      std::move(unions),
      Observes(xnode),
      xnode.attribute("limit").as_uint(0),
      xnode.attribute("depthCoefficient").as_double(0.5)
    };
  }

  if (not std::ranges::empty(xnode.children("observe"))) {
    return ::RuleNode{
      mode, Rules(xnode, unions, symmetry),
      std::move(unions),
      Observes(xnode),
      xnode.attribute("temperature").as_double(0.0)
    };
  }

  if (not std::ranges::empty(xnode.children("field"))) {
    return ::RuleNode{
      mode, Rules(xnode, unions, symmetry),
      std::move(unions),
      Fields(xnode),
      xnode.attribute("temperature").as_double(0.0)
    };
  }

  return ::RuleNode{
    mode, Rules(xnode, unions, symmetry),
    std::move(unions)
  };
}

auto Rule(
  const pugi::xml_node& xnode,
  const RewriteRule::Unions& unions
) noexcept -> ::RewriteRule {
  ensures(xnode.attribute("in"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "in", "[rule]", xnode.offset_debug()));
  auto&& input = std::string{xnode.attribute("in").as_string()};
  ensures(!std::ranges::empty(input),
          std::format("empty '{}' attribute in '{}' node [:{}]", "in", "[rule]", xnode.offset_debug()));

  ensures(xnode.attribute("out"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "out", "[rule]", xnode.offset_debug()));
  auto&& output = std::string{xnode.attribute("out").as_string()};
  ensures(!std::ranges::empty(output),
          std::format("empty '{}' attribute in '{}' node [:{}]", "out", "[rule]", xnode.offset_debug()));

  ensures(std::ranges::size(input) == std::ranges::size(output),
          std::format("attributes '{}' and '{}' of '{}' node must be of same shape [:{}]", "in", "out", "[rule]", xnode.offset_debug()));

  return RewriteRule::parse(
    unions,
    input, output,
    xnode.attribute("p").as_double(1.0)
  );
}

auto Rules(
  const pugi::xml_node& xnode,
  const RewriteRule::Unions& unions,
  std::string_view symmetry
) noexcept -> std::vector<RewriteRule> {
  auto xrules = xnode.children("rule") | std::ranges::to<std::vector>();
  if (std::ranges::empty(xrules)) xrules.push_back(xnode);
  auto rs = std::move(xrules)
     | std::views::transform(std::bind_back(Rule, unions))
     | std::views::transform(std::bind_back(&RewriteRule::symmetries, symmetry))
     | std::views::join
     | std::ranges::to<std::vector>();
  return rs;
}

auto Field(const pugi::xml_node& xnode) noexcept -> std::pair<char, ::Field> {
  ensures(xnode.attribute("for"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "for", "field", xnode.offset_debug()));
  auto&& _for = std::string{xnode.attribute("for").as_string()};
  ensures(!std::ranges::empty(_for),
          std::format("empty '{}' attribute in '{}' node [:{}]", "for", "field", xnode.offset_debug()));
  ensures(std::ranges::size(_for) == 1,
          std::format("only one character allowed for '{}' attribute of '{}' node [:{}]", "for", "field", xnode.offset_debug()));

  ensures(xnode.attribute("on"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "on", "field", xnode.offset_debug()));
  auto&& substrate = std::string{xnode.attribute("on").as_string()};
  ensures(!std::ranges::empty(substrate),
          std::format("empty '{}' attribute in '{}' node [:{}]", "on", "field", xnode.offset_debug()));

  ensures(xnode.attribute("from") or xnode.attribute("to"),
          std::format("missing one of '{}' or '{}' attributes in '{}' node [:{}]", "from", "to", "field", xnode.offset_debug()));
  ensures(not (xnode.attribute("from") and xnode.attribute("to")),
          std::format("only one of '{}' or '{}' attributes allowed in '{}' node [:{}]", "from", "to", "field", xnode.offset_debug()));

  auto&& inversed = not xnode.attribute("to");
  auto&& zero = std::string{xnode.attribute("from").as_string(xnode.attribute("to").as_string())};
  ensures(!std::ranges::empty(zero),
          std::format("empty '{}' attribute in '{}' node [:{}]", inversed ? "from" : "to", "field", xnode.offset_debug()));

  return std::make_pair(_for[0], ::Field{
    xnode.attribute("recompute").as_bool(false),
    xnode.attribute("essential").as_bool(false),
    inversed,
    substrate | std::ranges::to<std::unordered_set>(),
    zero | std::ranges::to<std::unordered_set>()
  });
}

auto Fields(const pugi::xml_node& xnode) noexcept -> ::Fields {
  return xnode.children("field")
    | std::views::transform(Field)
    | std::ranges::to<::Fields>();
}

auto Observe(const pugi::xml_node& xnode) noexcept -> std::pair<char, ::Observe> {
  ensures(xnode.attribute("value"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "value", "observe", xnode.offset_debug()));
  auto&& value = std::string{xnode.attribute("value").as_string()};
  ensures(!std::ranges::empty(value),
          std::format("empty '{}' attribute in '{}' node [:{}]", "value", "observe", xnode.offset_debug()));
  ensures(std::ranges::size(value) == 1,
          std::format("only one character allowed value '{}' attribute of '{}' node [:{}]", "value", "observe", xnode.offset_debug()));

  auto from = std::string{xnode.attribute("from").as_string()};
  ensures(not xnode.attribute("from") or !std::ranges::empty(from),
          std::format("empty '{}' attribute in '{}' node [:{}]", "from", "observe", xnode.offset_debug()));
  ensures(not xnode.attribute("from") or std::ranges::size(from) == 1,
          std::format("only one character allowed for '{}' attribute of '{}' node [:{}]", "from", "observe", xnode.offset_debug()));

  return std::make_pair(value[0], ::Observe{
    not xnode.attribute("from") ? std::nullopt : std::optional{from[0]},
    std::string{xnode.attribute("to").as_string()} | std::ranges::to<std::unordered_set>()
  });
}

auto Observes(const pugi::xml_node& xnode) noexcept -> ::Observes {
  return xnode.children("observe")
    | std::views::transform(Observe)
    | std::ranges::to<std::unordered_map>();
}

auto Palette(const pugi::xml_document& xpalette) noexcept -> ColorPalette {
  return xpalette.child("colors").children("color")
    | std::views::transform([](auto&& xcolor) static noexcept {
      ensures(xcolor.attribute("symbol"),
              std::format("missing '{}' attribute in '{}' node [:{}]", "symbol", "color", xcolor.offset_debug()));
        auto&& symbol_str = std::string{xcolor.attribute("symbol").as_string()};
        ensures(!std::ranges::empty(symbol_str),
                std::format("empty '{}' attribute in '{}' node [:{}]", "symbol", "color", xcolor.offset_debug()));
        ensures(std::ranges::size(symbol_str) == 1,
                std::format("only one character allowed for '{}' attribute of '{}' node [:{}]", "symbol", "color", xcolor.offset_debug()));
        
        ensures(xcolor.attribute("value"),
                std::format("missing '{}' attribute in '{}' node [:{}]", "value", "color", xcolor.offset_debug()));
        auto&& value = std::string{xcolor.attribute("value").as_string()};
        ensures(!std::ranges::empty(value),
                std::format("empty '{}' attribute in '{}' node [:{}]", "value", "color", xcolor.offset_debug()));
        
        return std::make_pair(symbol_str[0], (255u << 24u) + fromBase<u32>(value, 16));
    })
    | std::ranges::to<ColorPalette>();
}

auto document(std::span<const std::byte> buffer) noexcept -> pugi::xml_document {
  auto xdocument = pugi::xml_document{};
  auto&& result = xdocument.load_buffer(std::data(buffer), std::size(buffer));
  ensures(result, std::format("Error while parsing xml (<buffer>:{}) : {}", result.offset, result.description()));
  return xdocument;
}

auto document(const std::filesystem::path& filepath) noexcept -> pugi::xml_document {
  auto xdocument = pugi::xml_document{};
  auto&& result = xdocument.load_file(filepath.c_str());
  ensures(result, std::format("Error while parsing xml ({}:{}) : {}", filepath.generic_string(), result.offset, result.description()));
  return xdocument;
}

}
