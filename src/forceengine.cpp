module forceengine;

import stormkit.core;
import geometry;

using namespace stormkit;

auto ForceEngine::weight(const Grid<char>& grid, const Match& match) noexcept -> std::optional<int> {
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
    | std::views::transform([&] (auto&& _o) noexcept -> std::optional<int> {
        auto&& [u, o] = _o;

        auto&& new_value = o;
        auto&& old_value = grid[u];

        if (not potentials.contains(new_value)) return std::nullopt;

        auto&& new_p = potentials.at(new_value)[u];
        auto&& old_p = potentials.contains(old_value) ? potentials.at(old_value)[u] : -1;

        return new_p - old_p;
    }),
    std::optional{0}, [](auto&& a, auto&& v) static noexcept {
      return a.and_then([&v](auto&& a) noexcept {
        return v.transform([&a](auto&& v) noexcept {
          return a + v;
        });
      });
    }
  );
}

auto ForceEngine::score_projection(const Grid<char>& grid, std::span<const Match> matches) noexcept -> std::function<double(const Match&)> {
  static auto rg = std::mt19937{std::random_device{}()};
  static auto prob = std::uniform_real_distribution<>{};

  if (std::ranges::empty(potentials)) {
    // std::ranges::shuffle(matches, rg);
    return [](auto&&) static noexcept { return prob(rg); };
  }

  auto&& weights = matches
      | std::views::transform(bindFront(&ForceEngine::weight, this, grid));

  auto&& scores = std::views::zip(
    matches,
    std::move(weights) | std::views::transform([&](auto&& w) noexcept {
      if (not w) return std::numeric_limits<double>::signaling_NaN();

      auto&& u = prob(rg);

      return temperature > 0.0
        /** Boltzmann distribution: `p(r) ~ exp(-w(r)/t)` */
        ? std::pow(u, std::exp((*w) / temperature))   
        /** frozen config */
        : -(*w) + 0.001 * u;
    }))
    | std::ranges::to<std::unordered_map<Match, double>>();

  return [scores = std::move(scores)](auto&& m) noexcept {
    return scores.at(m);
  };
}
