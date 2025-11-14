module engine.rewriterule;

import frozen;
import log;
import utils;

namespace stk  = stormkit;
namespace stkm = stk::monadic;
namespace stdr = std::ranges;
namespace stdv = std::views;

auto RewriteRule::parse(
  const Unions& unions,
  std::string_view input,
  std::string_view output,
  double p
) noexcept -> RewriteRule {
  return {
    Grid<Input>::parse(input, [&unions](auto raw) noexcept -> Input {
      return raw == IGNORED_SYMBOL ? Input {} : Input { unions.contains(raw) ? unions.at(raw) : std::set{ raw } };
    }),
    Grid<Output>::parse(output, [](auto raw) noexcept -> Output {
      return raw == IGNORED_SYMBOL ? Output {} : Output { raw };
    }),
    p
  };
}

RewriteRule::RewriteRule(Grid<Input>&& _input, Grid<Output>&& _output, double p, bool _original) noexcept
: input{std::move(_input)},
  output{std::move(_output)},
  draw{p},
  original{_original},
  ishifts{
    std::from_range,
    stdv::zip(input, mdiota(input.area()))
      | stdv::transform([](auto&& p) noexcept {
          auto [i, u] = p;
          // TODO this must change when fixing size of state representation
          return i.value_or(std::set{ IGNORED_SYMBOL }) 
            | stdv::transform([u](auto c) noexcept {
                return std::tuple{ c, u };
            });
      })
      | stdv::join
  },
  oshifts{
    std::from_range,
    stdv::zip(output, mdiota(output.area()))
      | stdv::transform([](auto&& p) noexcept {
          auto [o, u] = p;
          return std::tuple{ o.value_or(IGNORED_SYMBOL), u };
      })
  }
{}

auto RewriteRule::get_ishifts(char c) const noexcept -> std::vector<glm::vec<3, stk::u32>>{
  auto shifts = std::vector<glm::vec<3, stk::u32>>{};

  auto ignored_bucket = ishifts.bucket(IGNORED_SYMBOL);
  auto bucket         = ishifts.bucket(c);

  shifts.append_range(
    stdr::subrange(ishifts.cbegin(ignored_bucket), ishifts.cend(ignored_bucket))
      | stdv::transform(stkm::get<1>())
  );
  shifts.append_range(
    stdr::subrange(ishifts.cbegin(bucket), ishifts.cend(bucket))
      | stdv::transform(stkm::get<1>())
  );

  return shifts;
}

auto RewriteRule::operator==(const RewriteRule& other) const noexcept -> bool {
  return input    == other.input
     and output   == other.output
     and draw.p() == other.draw.p();
}

auto RewriteRule::backward_neighborhood() const noexcept -> Area3I {
  const auto a = output.area();
  const auto shift = glm::vec<3, stk::i32>{1, 1, 1} - static_cast<glm::vec<3, stk::i32>>(a.size);
  return a + shift;
}

auto RewriteRule::xreflected() const noexcept -> RewriteRule {
  return {
    input.xreflected(),
    output.xreflected(),
    draw.p(),
    false
  };
}

auto RewriteRule::xyrotated() const noexcept -> RewriteRule {
  return {
    input.xyrotated(),
    output.xyrotated(),
    draw.p(),
    false
  };
}

auto RewriteRule::zyrotated() const noexcept -> RewriteRule {
  return {
    input.zyrotated(),
    output.zyrotated(),
    draw.p(),
    false
  };
}

template <class T>
const auto square_groups = std::array<std::function<T(const T&)>, 8> {
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
  return stdr::fold_left(
    stdv::zip(
      square_groups<RewriteRule>,
      square_subgroups.at(std::empty(subgroup) ? "(xy)" : subgroup)
    )
      | stdv::filter(stkm::get<1>())
      | stdv::transform(stkm::get<0>())
      | stdv::transform([this](const auto& s) noexcept { return s(*this); }),
    std::vector<RewriteRule>{},
    [](auto&& acc, auto&& r) static noexcept {
      if (not stdr::contains(acc, r))
        acc.push_back(std::move(r));
      return acc;
    }
  );
}
