module match;

import log;

auto Match::conflict(const Match& other) const noexcept -> bool {
  auto&& overlap = area().meet(other.area());
  return std::ranges::any_of(
    mdiota(overlap),
    [](const auto& p) static noexcept {
      return std::get<0>(p) != IGNORED_SYMBOL
         and std::get<1>(p) != IGNORED_SYMBOL;
    },
    [&a = *this, &b = other](auto u) noexcept {
      return std::tie(
        a.rules[a.r].output.at(u - a.u),
        b.rules[b.r].output.at(u - b.u)
      );
    }
  );
}

auto Match::match(const Grid<char>& grid, const RewriteRule::Unions& unions) const noexcept -> bool {
  auto m = std::ranges::all_of(
    std::views::zip(mdiota(area()), rules[r].input),
    [&grid, &unions](const auto& input) noexcept {
      auto [u, i] = input;
      auto m = i == IGNORED_SYMBOL
          or (unions.contains(i)
          and unions.at(i).contains(grid[u]));
      return m;
    }
  );

  return m;
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
  return std::ranges::fold_left(
    std::views::zip(mdiota(area()), rules[r].output)
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
    }),
    0.0, std::plus{}
  );
}
