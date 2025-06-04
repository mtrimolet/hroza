module forceengine;

import sort;
import stormkit.core;
import geometry;

using namespace stormkit;

auto ForceEngine::weight(const Grid<char>& grid, const Match& match) noexcept -> double {
  return std::ranges::fold_left(
    std::views::zip(
      mdiota(match.area()),
      match.rule.output
    )
    | std::views::filter([&](auto&& _o) noexcept {
        auto&& [u, o] = _o;
        // locations where value is preserved, their difference is 0
        return o != Match::IGNORED_SYMBOL and o != grid[u];
    })
    | std::views::transform([&] (auto&& _o) noexcept {
        auto&& [u, o] = _o;

        auto&& new_value = o;
        auto&& old_value = grid[u];

        auto&&
          new_p = potentials.contains(new_value) ? potentials.at(new_value)[u] : std::numeric_limits<double>::signaling_NaN(),
          old_p = potentials.contains(old_value) ? potentials.at(old_value)[u] : -1.0;

        return new_p - old_p;
    }),
    0.0, std::plus{}
  );
}

auto ForceEngine::sort(const Grid<char>& grid, std::span<Match> matches) noexcept -> void {
  static auto rg = std::mt19937{std::random_device{}()};
  if (std::ranges::empty(potentials)) {
    std::ranges::shuffle(matches, rg);
    // return std::ranges::subrange(std::ranges::end(matches), std::ranges::end(matches));
    return;
  }

  auto&& weights = matches
      | std::views::transform(bindFront(&ForceEngine::weight, this, grid));

  auto&& scores = std::views::zip(
    matches,
    weights | std::views::transform([&](auto&& w) noexcept {
      static auto prob = std::uniform_real_distribution<>{};
      auto&& u = prob(rg);

      return temperature > 0.0
        /** Boltzmann distribution: `p(r) ~ exp(-w(r)/t)` */
        ? std::pow(u, std::exp(w / temperature))   
        /** frozen config */
        : -w + 0.001 * u;
    }))
    | std::ranges::to<std::unordered_map<Match, double>>();

  auto&& get_score = [&](auto&& m) noexcept {
    auto&& v = scores.at(m);
    // ensures(v == 0.0 or std::isnormal(v), "anormal score value in dijkstra inference");
    return v;
  };

  // auto&& thrown = std::ranges::remove(matches, std::numeric_limits<double>::signaling_NaN(), get_score);

  // auto&& remains = std::ranges::subrange(std::ranges::begin(matches), std::ranges::begin(thrown));
  ::sort(matches, {}, get_score);

  // return std::ranges::subrange(std::ranges::begin(thrown), std::ranges::end(thrown));
}
