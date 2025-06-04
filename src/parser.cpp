module parser;

import log;

using namespace stormkit;
using namespace std::literals;

namespace parser {

auto Model(const pugi::xml_document& xmodel) noexcept -> ::Model {
  auto&& xnode = xmodel.first_child();

  ensures(xnode.attribute("values"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "values", "[root]", xnode.offset_debug()));
  auto symbols = std::string{xnode.attribute("values").as_string()};
  ensures(!std::ranges::empty(symbols),
          std::format("empty '{}' attribute in '{}' node [:{}]", "values", "[root]", xnode.offset_debug()));
  // ensure no duplicate
  auto&& origin = xnode.attribute("origin").as_bool(false);

  auto unions = RewriteRule::Unions{{'*', auto{symbols}}};
  unions.insert_range(symbols
    | std::views::transform([](auto&& c) static noexcept { 
        return std::make_pair(c, std::string{c});
    }));
  // ilog("unions: {}", unions);

  auto&& program = ExecutionNode(xnode, unions);
  if (xnode.name() != "sequence"s and xnode.name() != "markov"s)
    program = Markov{{program}};

  return ::Model{
    // title,
    std::move(symbols), std::move(unions),
    std::move(origin),
    std::move(program)
  };
}

auto ExecutionNode(
  const pugi::xml_node& xnode,
  RewriteRule::Unions unions,
  std::string_view symmetry
) noexcept -> ::ExecutionNode {
  symmetry = xnode.attribute("symmetry").as_string(std::data(symmetry));

  unions.insert_range(xnode.children("union") 
    | std::views::transform([](const pugi::xml_node& xunion) static noexcept {
      ensures(xunion.attribute("symbol"),
              std::format("missing '{}' attribute in '{}' node [:{}]", "symbol", "union", xunion.offset_debug()));
      auto&& symbol_str = std::string{xunion.attribute("symbol").as_string()};
      ensures(!std::ranges::empty(symbol_str),
              std::format("empty '{}' attribute in '{}' node [:{}]", "symbol", "union", xunion.offset_debug()));
      ensures(std::ranges::size(symbol_str) == 1,
              std::format("only one character allowed for '{}' attribute of '{}' node [:{}]", "symbol", "union", xunion.offset_debug()));

      ensures(xunion.attribute("values"),
              std::format("missing '{}' attribute in '{}' node [:{}]", "values", "union", xunion.offset_debug()));
      auto&& values = std::string{xunion.attribute("values").as_string()};
      ensures(!std::ranges::empty(values),
              std::format("empty '{}' attribute in '{}' node [:{}]", "values", "union", xunion.offset_debug()));

      return std::make_pair(symbol_str[0], std::move(values));
    })
  );
  
  auto&& tag = xnode.name();
  if (tag == "sequence"s) {
    return Sequence{
      xnode.children()
        | std::views::filter([](const pugi::xml_node& c) static noexcept { return c.name() != "union"s; })
        | std::views::transform(bindBack(ExecutionNode, unions, symmetry))
        | std::ranges::to<std::vector>()
    };
  }
  else if (tag == "markov"s) {
    return Markov{
      xnode.children()
        | std::views::filter([](const pugi::xml_node& c) static noexcept { return c.name() != "union"s; })
        | std::views::transform(bindBack(ExecutionNode, unions, symmetry))
        | std::ranges::to<std::vector>()
    };
  }
  else if (tag == "one"s
        or tag == "prl"s
        or tag == "all"s) {
    auto&& steps = xnode.attribute("steps").as_uint(0);
    if (steps == 0)
      return NoLimit{RuleNode(xnode, symmetry)};
    else
      return Limit{std::move(steps), RuleNode(xnode, symmetry)};
  }
  
  ensures(false, std::format("unknown tag '{}' [:{}]", tag, xnode.offset_debug()));
  std::unreachable();
}

auto RuleNode(
  const pugi::xml_node& xnode,
  std::string_view symmetry
) noexcept -> ::Action {
  auto&& tag = xnode.name();
  if (tag == "one"s) {
    auto&& observes = xnode.children("observe");
    if (not std::ranges::empty(observes)) {
      auto&& search = xnode.attribute("search").as_bool(false);
      if (search) {
        return One{
          xnode.attribute("limit")
            ? std::optional{xnode.attribute("limit").as_uint()}
            : std::nullopt,
          xnode.attribute("depthCoefficient").as_double(0.5),
          Observations(xnode),
          Rules(xnode, symmetry)
        };
      }

      return One{
        xnode.attribute("temperature").as_double(0.0),
        Observations(xnode),
        Rules(xnode, symmetry)
      };
    }

    auto&& fields = xnode.children("field");
    if (not std::ranges::empty(fields)) {
      return One{
        xnode.attribute("temperature").as_double(0.0),
        Fields(xnode),
        Rules(xnode, symmetry)
      };
    }

    return One{
      Rules(xnode, symmetry)
    };
  }

  if (tag == "prl"s) {
    return Prl{
      Rules(xnode, symmetry)
    };
  }

  if (tag == "all"s) {
    auto&& observes = xnode.children("observe");
    if (not std::ranges::empty(observes)) {
      auto&& search = xnode.attribute("search").as_bool(false);
      if (search) {
        return All{
          xnode.attribute("limit")
            ? std::optional{xnode.attribute("limit").as_uint()}
            : std::nullopt,
          xnode.attribute("depthCoefficient").as_double(0.5),
          Observations(xnode),
          Rules(xnode, symmetry)
        };
      }

      return All{
        xnode.attribute("temperature").as_double(0.0),
        Observations(xnode),
        Rules(xnode, symmetry)
      };
    }

    auto&& fields = xnode.children("field");
    if (not std::ranges::empty(fields)) {
      return All{
        xnode.attribute("temperature").as_double(0.0),
        Fields(xnode),
        Rules(xnode, symmetry)
      };
    }

    return All{
      Rules(xnode, symmetry)
    };
  }

  ensures(false, std::format("unknown rulenode {}", tag));
  std::unreachable();
}

auto Rule(const pugi::xml_node& xnode) noexcept -> ::RewriteRule {
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
  
  return RewriteRule::parse(
    input, output,
    xnode.attribute("p").as_double(1.0)
  );
}

auto Rules(
  const pugi::xml_node& xnode,
  std::string_view symmetry
) noexcept -> std::vector<RewriteRule> {
  auto xrules = xnode.children("rule") | std::ranges::to<std::vector>();
  if (std::ranges::empty(xrules)) xrules.push_back(xnode);
  return std::move(xrules)
     | std::views::transform(Rule)
     | std::views::transform(bindBack(RewriteRule::symmetries, symmetry))
     | std::views::join
     | std::ranges::to<std::vector>();
}

auto Field(const pugi::xml_node& xnode) noexcept -> std::pair<char, ::DijkstraField> {
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
  return std::make_pair(
    _for[0],
    DijkstraField{
      xnode.attribute("recompute").as_bool(false),
      xnode.attribute("essential").as_bool(false),
      inversed,
      substrate | std::ranges::to<std::unordered_set>(),
      zero | std::ranges::to<std::unordered_set>()
    }
  );
}

auto Fields(const pugi::xml_node& xnode) noexcept -> DijkstraFields {
  return xnode.children("field")
    | std::views::transform(Field)
    | std::ranges::to<DijkstraFields>();
}

auto Observation(const pugi::xml_node& xnode) noexcept -> std::pair<char, ::Observation> {
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
  
  return std::make_pair(
    value[0],
    ::Observation{
      not xnode.attribute("from") ? std::nullopt : std::optional{from[0]},
      std::string{xnode.attribute("to").as_string()} | std::ranges::to<std::unordered_set>()
    }
  );
}

auto Observations(const pugi::xml_node& xnode) noexcept -> ::Observations {
  return xnode.children("observe")
    | std::views::transform(Observation)
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
        
        return std::make_pair(symbol_str[0], (255u << 24u) + fromBase<UInt32>(value, 16));
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
