module match;

auto Match::conflict(const Match& other) const noexcept -> bool {
  auto&& overlap = area().meet(other.area());
  return std::ranges::any_of(
    mdiota(overlap),
    [](auto&& p) static noexcept {
      return std::get<0>(p) != IGNORED_SYMBOL
         and std::get<1>(p) != IGNORED_SYMBOL;
    },
    [&a = *this, &b = other](auto&& u) noexcept {
      return std::make_tuple(
        a.rules[a.r].output.at(u - a.u),
        b.rules[b.r].output.at(u - b.u)
      );
    }
  );
}

auto Match::match(const Grid<char>& grid, const RewriteRule::Unions& unions) const noexcept -> bool {
  return std::ranges::all_of(
    // reverse for Boyer-Moore ?
    // std::views::reverse(std::views::zip(mdiota(area()), rule.input)),
    std::views::zip(mdiota(area()), rules[r].input),
    [&grid, &unions](const auto& input) noexcept {
      auto [u, i] = input;
      auto m = i == IGNORED_SYMBOL
          or (unions.contains(i)
          and std::ranges::contains(unions.at(i), grid[u]));
      return m;
    }
  );
}

auto Match::changes(const Grid<char>& grid) const noexcept -> std::vector<Change<char>> {
  return std::views::zip(mdiota(area()), rules[r].output)
    | std::views::filter([&grid](const auto& output) noexcept {
        const auto& [u, value] = output;
        return value != IGNORED_SYMBOL
           and value != grid[u];
    })
    | std::views::transform([/* &grid */](auto&& output) static noexcept {
        // auto&& [u, value] = output;
        // grid[u] = value;
        return Change{std::get<0>(output), std::get<1>(output)};
    })
    | std::ranges::to<std::vector>();
}
