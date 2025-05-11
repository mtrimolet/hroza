module forceengine;

using namespace stormkit;

auto ForceEngine::gain(Grid<char>::ConstView grid, const Match& match) noexcept -> double {
  return std::ranges::fold_left(
    std::views::zip(
      mdiota(match.area()),
      match.rule.output
    )
    | std::views::filter([&grid](auto&& _o) noexcept {
        auto&& [u, o] = _o;
        // locations where value is preserved, their difference is 0
        return o != std::nullopt and *o != grid[u.z, u.y, u.x];
    })
    | std::views::transform([&grid, &potentials{potentials}] (auto&& _o) noexcept {
        auto&& [u, o] = _o;

        auto&& new_value = *o;
        auto&& old_value = grid[u.z, u.y, u.x];

        auto&&
          new_p = potentials.contains(new_value) ? potentials.at(new_value)[u] : std::numeric_limits<double>::signaling_NaN(),
          old_p = potentials.contains(old_value) ? potentials.at(old_value)[u] : -1.0;

        return new_p - old_p;
    }),
    0.0, std::plus{}
  );
}

auto ForceEngine::applyPotentials(Grid<char>::ConstView grid, std::span<Match> matches) noexcept -> void {
  static auto rg = std::mt19937{std::random_device{}()};
  if (std::ranges::empty(potentials)) {
    std::ranges::shuffle(matches, rg);
    // return std::ranges::subrange(std::ranges::end(matches), std::ranges::end(matches));
    return;
  }

  auto&& gains = matches
      | std::views::transform(bindFront(&ForceEngine::gain, this, grid));

  auto&& values = std::views::zip(
    matches,
    gains
      | std::views::transform([&](auto&& g) noexcept {
          static auto prob = std::uniform_real_distribution<>{};
          auto&& u = prob(rg);

          return temperature > 0.0
            ? std::pow(u, std::exp(g / temperature))   // `e^(dE/T)`
            : -g + 0.001 * u;                          // what's this arbitrary function used for the cold configuration ?;
        })
  )
    | std::ranges::to<std::unordered_map<Match, double>>();

  auto&& get_value = [&](auto&& m) noexcept {
    auto&& v = values.at(m);
    // ensures(v == 0.0 or std::isnormal(v), "anormal score value in dijkstra inference");
    return v;
  };

  auto&& thrown = std::ranges::remove(matches, std::numeric_limits<double>::signaling_NaN(), get_value);

  auto&& remains = std::ranges::subrange(std::ranges::begin(matches), std::ranges::begin(thrown));
  ::sort(remains, std::ranges::greater{}, get_value);

  // return std::ranges::subrange(std::ranges::begin(thrown), std::ranges::end(thrown));
}
