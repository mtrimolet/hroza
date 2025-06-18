module rewriterule;

import frozen;
import log;

using namespace stormkit;

RewriteRule::RewriteRule(Grid<char>&& _input, Grid<char>&& _output, double p, bool _original) noexcept
: input{std::move(_input)},
  output{std::move(_output)},
  draw{p},
  original{_original},
  ishifts{std::views::zip(input, mdiota(input.area())) | std::ranges::to<Shifts>()}
{}

auto&& square_groups = into_array<std::function<RewriteRule(const RewriteRule&)>>(
  [](auto&& rule) static noexcept { return auto{rule}; },
  [](auto&& rule) static noexcept { return rule.xreflected(); },
  [](auto&& rule) static noexcept { return rule.xyrotated(); },
  [](auto&& rule) static noexcept { return rule.xyrotated().xreflected(); },
  [](auto&& rule) static noexcept { return rule.xyrotated().xyrotated(); },
  [](auto&& rule) static noexcept { return rule.xyrotated().xyrotated().xreflected(); },
  [](auto&& rule) static noexcept { return rule.xyrotated().xyrotated().xyrotated(); },
  [](auto&& rule) static noexcept { return rule.xyrotated().xyrotated().xyrotated().xreflected(); }
);

constexpr auto square_subgroups =
  frozen::make_unordered_map<frozen::string, std::array<bool, 8>, 6>({
    {"()",     {true, false, false, false, false, false, false, false}},
    {"(x)",    {true,  true, false, false, false, false, false, false}},
    {"(y)",    {true, false, false, false,  true, false, false, false}},
    {"(x)(y)", {true,  true, false, false,  true,  true, false, false}},
    {"(xy+)",  {true, false,  true, false,  true, false,  true, false}},
    {"(xy)",   {true,  true,  true,  true,  true,  true,  true,  true}}
  });

template <>
struct std::hash<std::dims<3, std::size_t>> {
  inline constexpr auto operator()(std::dims<3, std::size_t> u) const noexcept -> std::size_t {
    auto h = std::hash<std::dims<3>::index_type>{};
    return h(u.extent(0))
         ^ h(u.extent(1))
         ^ h(u.extent(2));
  }
};

// template <class T>
// struct std::hash<std::unordered_set<T>> {
//   inline constexpr auto operator()(const std::unordered_set<T>& set) const noexcept -> std::size_t {
//     auto&& seed = std::hash<std::size_t>{}(set.size());
//     for (auto&& i : set) {
//       seed ^= std::hash<T>{}(i);
//     }
//     return seed;
//   }
// };

template <>
struct std::hash<RewriteRule> {
  inline constexpr auto operator()(const RewriteRule& rule) const noexcept -> std::size_t {
    auto seed = std::hash<std::dims<3>>{}(rule.input.extents);
    auto h = std::hash<char>{};
    for (auto v : rule.input) {
      seed ^= h(v);
    }
    for (auto v : rule.output) {
      seed ^= h(v);
    }
    return seed;
  }
};

auto RewriteRule::symmetries(std::string_view subgroup) const noexcept -> std::vector<RewriteRule> {
  return std::views::zip(square_groups, square_subgroups.at(std::empty(subgroup) ? "(xy)" : subgroup))
      | std::views::filter(monadic::get<1>())
      | std::views::transform([this](const auto& action) noexcept {
          return std::get<0>(action)(*this);
      })
      | std::ranges::to<std::vector>();
}
