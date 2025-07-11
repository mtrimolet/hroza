module match;

import log;

auto Match::conflict(const Match& other) const noexcept -> bool {
  auto overlap = mdiota(area().meet(other.area()))
    | std::views::transform([&a = *this, &b = other](auto u) noexcept {
        return std::tie(
          a.rules[a.r].output.at(u - a.u),
          b.rules[b.r].output.at(u - b.u)
        );
      });
  return std::any_of(
    std::execution::par,
    std::ranges::begin(overlap),
    std::ranges::end(overlap),
    [](const auto& p) static noexcept {
      return std::get<0>(p) != IGNORED_SYMBOL
         and std::get<1>(p) != IGNORED_SYMBOL;
    }
  );
}

auto Match::match(const Grid<char>& grid, const RewriteRule::Unions& unions) const noexcept -> bool {
  auto zone = std::views::zip(mdiota(area()), rules[r].input);
  return std::all_of(
    std::execution::par,
    std::ranges::begin(zone),
    std::ranges::end(zone),
    [&grid, &unions](const auto& input) noexcept {
      auto [u, i] = input;
      return i == IGNORED_SYMBOL
          or (unions.contains(i)
          and unions.at(i).contains(grid[u]));
    }
  );
}

auto Match::changes(const Grid<char>& grid) const noexcept -> std::vector<Change<char>> {
  // ilog("r = {}, u = {}", r, u);
  return std::views::zip(mdiota(area()), rules[r].output)
    | std::views::filter([&grid](const auto& output) noexcept {
        auto [u, value] = output;
        return value != IGNORED_SYMBOL
           and value != grid[u];
    })
    | std::views::transform([](auto&& output) static noexcept {
        return Change{std::get<0>(output), std::get<1>(output)};
    })
    | std::ranges::to<std::vector>();
}

auto Match::delta(const Grid<char>& grid, const Potentials& potentials) noexcept -> double {
  auto vals = std::views::zip(mdiota(area()), rules[r].output)
    | std::views::filter([&grid](auto&& _o) noexcept {
        auto&& [u, o] = _o;
        return o != IGNORED_SYMBOL
           and o != grid[u];
    })
    | std::views::transform([&grid, &potentials] (const auto& _o) noexcept {
        auto [u, o] = _o;

        auto new_value = o;
        auto old_value = grid[u];

        auto new_p = potentials.contains(new_value) ? potentials.at(new_value)[u] : std::numeric_limits<double>::signaling_NaN();
        auto old_p = potentials.contains(old_value) ? potentials.at(old_value)[u] : -1.0;

        return new_p - old_p;
    });
  return std::reduce(
    std::execution::par,
    std::ranges::begin(vals),
    std::ranges::end(vals)
  );
}
