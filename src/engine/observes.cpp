module engine.observes;

import glm;
import engine.match;

namespace stdr = std::ranges;
namespace stdv = std::views;

auto Observe::future(std::vector<Change<char>>& changes, Future& future, const Grid<char>& grid, const Observes& observes) noexcept -> void {
  auto values = std::unordered_set<char>{};

  future = {
    std::from_range,
    mdiota(grid.area()) | stdv::transform([&](auto u) noexcept {
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
    }),
    grid.extents
  };

  if (const auto expected = observes | stdv::keys | stdr::to<std::unordered_set>();
                 expected != values
  ) {
    future = {};
  }
}

auto Observe::backward_potentials(Potentials& potentials, const Future& future, std::span<const RewriteRule> rules) noexcept -> void {
  propagate(
    potentials
      | stdv::transform([&future](auto& p) noexcept {
          auto& [c, potential] = p;
          return mdiota(potential.area())
            | stdv::filter([&future, c](auto u) noexcept {
                return future[u].contains(c);
            })
            | stdv::transform([&potential, c](auto u) noexcept {
                potential[u] = 0.0;
                return std::tuple{ u, c };
            });
      })
      | stdv::join,
    [&potentials, &rules](auto&& front) noexcept {
      auto [u, c] = front;
      auto p = potentials.at(c)[u];
      return stdv::iota(0u, stdr::size(rules))
        | stdv::transform([&rules, u](auto r) noexcept {
            return Match{ rules, u, r };
        })
        | stdv::filter(std::bind_back(&Match::backward_match, potentials, p))
        | stdv::transform(std::bind_back(&Match::backward_changes, potentials, p + 1))
        | stdv::join
        | stdv::transform([&potentials](auto&& ch) noexcept {
            auto [c, p] = ch.value;
            potentials.at(c)[ch.u] = p;
            return std::tuple{ ch.u, c };
        });
    }
  );
}
