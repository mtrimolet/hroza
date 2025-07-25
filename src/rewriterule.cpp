module rewriterule;

import frozen;
import log;
import utils;

using namespace stormkit;

RewriteRule::RewriteRule(Grid<char>&& _input, Grid<char>&& _output, double p, bool _original) noexcept
: input{std::move(_input)},
  output{std::move(_output)},
  draw{p},
  original{_original},
  ishifts{std::views::zip(input, mdiota(input.area())) | std::ranges::to<Shifts>()}
{}

template <class T>
const auto square_groups = std::array<function<T(const T&)>, 8> {
    [](const T& x) static noexcept { return T{ x }                                            ; },
    [](const T& x) static noexcept { return x                                    .xreflected(); },
    [](const T& x) static noexcept { return x.xyrotated()                                     ; },
    [](const T& x) static noexcept { return x.xyrotated()                        .xreflected(); },
    [](const T& x) static noexcept { return x.xyrotated().xyrotated()                         ; },
    [](const T& x) static noexcept { return x.xyrotated().xyrotated()            .xreflected(); },
    [](const T& x) static noexcept { return x.xyrotated().xyrotated().xyrotated()             ; },
    [](const T& x) static noexcept { return x.xyrotated().xyrotated().xyrotated().xreflected(); }
  };

constexpr auto square_subgroups =
  frozen::make_unordered_map<frozen::string, const std::array<bool, 8>, 6>({
    {"()",     {true, false, false, false, false, false, false, false}},
    {"(x)",    {true,  true, false, false, false, false, false, false}},
    {"(y)",    {true, false, false, false,  true, false, false, false}},
    {"(x)(y)", {true,  true, false, false,  true,  true, false, false}},
    {"(xy+)",  {true, false,  true, false,  true, false,  true, false}},
    {"(xy)",   {true,  true,  true,  true,  true,  true,  true,  true}}
  });

auto RewriteRule::symmetries(std::string_view subgroup) const noexcept -> std::vector<RewriteRule> {
  return std::ranges::fold_left(
    std::views::zip(square_groups<RewriteRule>, square_subgroups.at(std::empty(subgroup) ? "(xy)" : subgroup))
      | std::views::filter(monadic::get<1>())
      | std::views::transform(monadic::get<0>())
      | std::views::transform([this](const auto& s) noexcept { return s(*this); }),
    std::vector<RewriteRule>{},
    [](auto&& acc, auto&& r) static noexcept {
      if (not std::ranges::contains(acc, r))
        acc.push_back(std::move(r));
      return acc;
    }
  );
}
