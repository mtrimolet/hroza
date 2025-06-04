module rewriteengine;

import glm;
import geometry;

using namespace stormkit;

auto RewriteEngine::newMatches(
  Grid<char> grid,
  std::span<const Change<char>> changes
) const noexcept -> std::vector<Match> {
  auto&& g_area = grid.area();

  if (std::ranges::empty(changes)) {
    return rules
      | std::views::transform([&](const RewriteRule& rule) noexcept {
          auto&& neigh = rule.backward_neighborhood();
          auto&& zone = (static_cast<Area3I>(g_area) + neigh.u).umeet(g_area);
          return mdiota(zone)
            | std::views::transform([&rule](auto&& u) noexcept {
                return Match{u, rule};
            });
      })
      | std::views::join
      | std::ranges::to<std::vector>();
  }

  return rules
    | std::views::transform([&](const RewriteRule& rule) noexcept {
        auto&& neigh = rule.backward_neighborhood();
        auto&& g_zone = (static_cast<Area3I>(g_area) + neigh.u).umeet(g_area);
        return changes
          | std::views::transform([&](const Change<char>& change) noexcept {
              auto&& zone = (neigh + change.u).meet(g_zone);
              return mdiota(zone);
          })
          | std::views::join
          | std::ranges::to<std::unordered_set>()
          | std::views::transform([&](auto&& u) noexcept {
              return Match{u, rule};
          });
    })
    | std::views::join
    | std::ranges::to<std::vector>();
}
