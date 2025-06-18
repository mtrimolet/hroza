module forceengine;

import glm;
import stormkit.core;
import log;
import geometry;

using namespace stormkit;

auto ForceEngine::weight(const Grid<char>& grid, const Match& match) noexcept -> double {
  return std::ranges::fold_left(
    std::views::zip(
      mdiota(match.area()),
      match.rules[match.r].output
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

        auto&& new_p = potentials.contains(new_value) ? potentials.at(new_value)[u] : std::numeric_limits<double>::signaling_NaN();
        auto&& old_p = potentials.contains(old_value) ? potentials.at(old_value)[u] : -1.0;

        return new_p - old_p;
    }),
    0.0, std::plus{}
  );
}

// auto ForceEngine::score_projection(const Grid<char>& grid, std::span<const Match> matches) noexcept -> std::function<double(const Match&)> {
//   static auto rg = std::mt19937{std::random_device{}()};
//   static auto prob = std::uniform_real_distribution<>{};

//   if (std::ranges::empty(potentials)) {
//     // std::ranges::shuffle(matches, rg);
//     return [](auto&&) static noexcept { return prob(rg); };
//   }

//   auto&& weights = matches
//       | std::views::transform(bindFront(&ForceEngine::weight, this, grid));

//   auto&& scores = std::views::zip(
//     matches,
//     std::move(weights) | std::views::transform([&](auto&& w) noexcept {
//       auto&& u = prob(rg);

//       return temperature > 0.0
//         /** Boltzmann distribution: `p(r) ~ exp(-w(r)/t)` */
//         ? std::pow(u, std::exp(w / temperature))   
//         /** frozen config */
//         : -w + 0.001 * u;
//     }))
//     | std::ranges::to<std::unordered_map<Match, double>>();

//   return [scores = std::move(scores)](auto&& m) noexcept {
//     return scores.at(m);
//   };
// }
