module rewriterule;

import frozen;

using namespace stormkit;

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

auto RewriteRule::symmetries(const RewriteRule& rule, std::string_view subgroup) noexcept -> std::unordered_set<RewriteRule> {
  return std::views::zip(square_groups, square_subgroups.at(std::empty(subgroup) ? "(xy)" : subgroup))
    | std::views::filter(monadic::get<1>())
    | std::views::transform([&rule](auto&& action) noexcept { return std::get<0>(action)(rule); })
    | std::ranges::to<std::unordered_set>();
}
