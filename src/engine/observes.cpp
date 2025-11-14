module engine.observes;

import glm;
import engine.match;

auto Observe::future(std::vector<Change<char>>& changes, Future& future, const Grid<char>& grid, const Observes& observes) noexcept -> void {
  auto values = std::unordered_set<char>{};

  future = mdiota(grid.area())
    | std::views::transform([&](auto u) noexcept {
        const auto value = grid[u];
        if (observes.contains(value)) {
          values.insert(value);
          const auto& obs = observes.at(value);
          if (obs.from) changes.emplace_back(u, *obs.from);
          return obs.to;
        }
        else {
          return std::unordered_set{ value };
        }
    })
    | std::ranges::to<Future>(grid.extents);

  if (const auto expected = observes | std::views::keys | std::ranges::to<std::unordered_set>();
                 expected != values
  ) {
    future = {};
  }
}

auto Observe::backward_potentials(Potentials& potentials, const Future& future, std::span<const RewriteRule> rules) noexcept -> void {
  propagate(
    potentials
      | std::views::transform([&future](auto& p) noexcept {
          auto& [c, potential] = p;
          return mdiota(potential.area())
            | std::views::filter([&future, c](auto u) noexcept {
                return future[u].contains(c);
            })
            | std::views::transform([&potential, c](auto u) noexcept {
                potential[u] = 0.0;
                return std::tuple{ u, c };
            });
      })
      | std::views::join,
    [&potentials, &rules](auto&& front) noexcept {
      auto [u, c] = front;
      auto p = potentials.at(c)[u];
      return std::views::iota(0u, std::ranges::size(rules))
        | std::views::transform([&rules, u](auto r) noexcept {
            return Match{ rules, u, r };
        })
        | std::views::filter(std::bind_back(&Match::backward_match, potentials, p))
        | std::views::transform(std::bind_back(&Match::backward_changes, potentials, p + 1))
        | std::views::join
        | std::views::transform([&potentials](auto&& ch) noexcept {
            auto [c, p] = ch.value;
            potentials.at(c)[ch.u] = p;
            return std::tuple{ ch.u, c };
        });
    }
  );
}
