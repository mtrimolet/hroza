module xmlparser;

using namespace stormkit;
using namespace std::literals;

namespace xmlparser {

auto parseXmlModel(const pugi::xml_document& xmodel) noexcept -> Model {
  auto&& xnode = xmodel.first_child();

  ensures(xnode.attribute("values"),
          std::format("missing '{}' attribute in '{}' node [:{}]", "values", "[root]", xnode.offset_debug()));
  auto&& symbols = std::string{xnode.attribute("values").as_string()};
  ensures(!std::ranges::empty(symbols),
          std::format("empty '{}' attribute in '{}' node [:{}]", "values", "[root]", xnode.offset_debug()));
  // ensure no duplicate
  auto&& origin = xnode.attribute("origin").as_bool(false);

  auto&& unions = symbols
    | std::views::transform([](auto&& c) static noexcept { 
        return std::make_pair(c, std::unordered_set{c});
      })
    | std::ranges::to<std::unordered_map>();

  auto&& program = parseXmlNode(xnode, unions);
  if (xnode.name() != "sequence"s and xnode.name() != "markov"s)
    program = Markov{{program}};

  return Model{
    // title,
    std::move(symbols), std::move(origin),
    std::move(program)
  };
}

auto parseXmlNode(
  const pugi::xml_node& xnode,
  std::unordered_map<char, RewriteRule::Union> unions,
  std::string_view symmetry
) noexcept -> ExecutionNode {
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

      return std::make_pair(symbol_str[0], values | std::ranges::to<std::unordered_set>());
    })
  );
  
  auto&& tag = xnode.name();
  if (tag == "sequence"s) {
    return Sequence{
      xnode.children()
        | std::views::filter([](const pugi::xml_node& c) static noexcept { return c.name() != "union"s; })
        | std::views::transform(bindBack(parseXmlNode, unions, symmetry))
        | std::ranges::to<std::vector>()
    };
  }
  else if (tag == "markov"s) {
    return Markov{
      xnode.children()
        | std::views::filter([](const pugi::xml_node& c) static noexcept { return c.name() != "union"s; })
        | std::views::transform(bindBack(parseXmlNode, unions, symmetry))
        | std::ranges::to<std::vector>()
    };
  }
  else if (tag == "one"s
        or tag == "prl"s
        or tag == "all"s) {
    auto&& steps = xnode.attribute("steps").as_uint(0);
    if (steps == 0)
      return NoLimit{parseXmlRuleNode(xnode, unions, symmetry)};
    else
      return Limit{std::move(steps), parseXmlRuleNode(xnode, unions, symmetry)};
  }
  
  ensures(false, std::format("unknown tag '{}' [:{}]", tag, xnode.offset_debug()));
  std::unreachable();
}

auto parseXmlRuleNode(
  const pugi::xml_node& xnode,
  std::unordered_map<char, RewriteRule::Union> unions,
  std::string_view symmetry
) noexcept -> Action {
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
          parseXmlObservations(xnode),
          parseXmlRules(xnode, unions, symmetry)
        };
      }

      return One{
        xnode.attribute("temperature").as_double(0.0),
        parseXmlObservations(xnode),
        parseXmlRules(xnode, unions, symmetry)
      };
    }

    auto&& fields = xnode.children("field");
    if (not std::ranges::empty(fields)) {
      return One{
        xnode.attribute("temperature").as_double(0.0),
        parseXmlFields(xnode),
        parseXmlRules(xnode, unions, symmetry)
      };
    }

    return One{
      parseXmlRules(xnode, unions, symmetry)
    };
  }

  if (tag == "prl"s) {
    return Prl{
      parseXmlRules(xnode, unions, symmetry)
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
          parseXmlObservations(xnode),
          parseXmlRules(xnode, unions, symmetry)
        };
      }

      return All{
        xnode.attribute("temperature").as_double(0.0),
        parseXmlObservations(xnode),
        parseXmlRules(xnode, unions, symmetry)
      };
    }

    auto&& fields = xnode.children("field");
    if (not std::ranges::empty(fields)) {
      return All{
        xnode.attribute("temperature").as_double(0.0),
        parseXmlFields(xnode),
        parseXmlRules(xnode, unions, symmetry)
      };
    }

    return All{
      parseXmlRules(xnode, unions, symmetry)
    };
  }

  ensures(false, std::format("unknown rulenode {}", tag));
  std::unreachable();
}

auto parseXmlRule(
  const pugi::xml_node& xnode,
  std::unordered_map<char, RewriteRule::Union> unions
) noexcept -> RewriteRule {
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
    unions, input, output,
    xnode.attribute("p").as_double(1.0)
  );
}

auto parseXmlField(const pugi::xml_node& xnode) noexcept -> std::pair<char, DijkstraField> {
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

auto parseXmlObservation(const pugi::xml_node& xnode) noexcept -> std::pair<char, Observation> {
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
    Observation{
      not xnode.attribute("from") ? std::nullopt : std::optional{from[0]},
      std::string{xnode.attribute("to").as_string()} | std::ranges::to<std::unordered_set>()
    }
  );
}

auto parseXmlPalette(std::filesystem::path filepath) noexcept -> Palette {
  auto&& xpalette = pugi::xml_document{};
  auto&& result = xpalette.load_file(filepath.c_str());
  ensures(result, std::format("Error while parsing xml ({}:{}) : {}", filepath.generic_string(), result.offset, result.description()));

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
    | std::ranges::to<Palette>();
}

}
