module searchengine;

import match;

using namespace stormkit;

auto forward(const Grid<char>& grid, RewriteRule::Unions unions, std::span<const RewriteRule> rules) noexcept -> Potentials {
  auto potentials = Potentials{};

  propagate(
    mdiota(grid.area())
      | std::views::transform([&](auto&& u) noexcept {
          auto&& c = grid[u];
          potentials.try_emplace(c, grid.extents, std::numeric_limits<double>::quiet_NaN());
          potentials.at(c)[u] = 0.0;
          return std::make_pair(c, u);
      }),
    [&](auto&& front) noexcept {
      auto&& [c, u] = front;
      auto&& potential = potentials.at(c);
      auto&& p_area = potential.area();

      auto&& p = potential[u];
      auto&& new_p = p + 1.0;
  
      return rules 
        | std::views::transform([&, p_area](auto&& rule) noexcept {
            auto&& neigh = rule.backward_neighborhood();
            auto&& zone = (neigh + u).umeet(p_area);
            return mdiota(zone)
              | std::views::transform([&](auto&& u) noexcept {
                  return std::views::zip(
                    mdiota(rule.area() + u),
                    rule.input,
                    rule.output
                  );
              })
              | std::views::filter([&, p](auto&& _uios) noexcept {
                  return std::ranges::none_of(
                    _uios | std::views::filter([](auto&& _uio) static noexcept {
                      auto&& [u, i, o] = _uio;
                      return i != Match::IGNORED_SYMBOL;
                    }),
                    bindBack(std::greater{}, p),
                    [&](auto&& _uio) noexcept {
                      auto&& [u, i, o] = _uio;
                      // return std::optional{0.0};
  
                      // In original MarkovJunior code, we use int-coded bitset for rule input (binput),
                      // and the following type-mismatch is solved using a mysterious "least index of active bit"
                      // which is very dependant to the specific fact that we use an _ordered_ repr. of a bitset...
                      // That's magic ! 
                      // .. unless that bit is expected to be the only one active
                      //
                      // Original code (our 'i' is the 'w'): `System.Numerics.BitOperations.TrailingZeroCount(w)`
                      // Or is the provided order of the chars so important that we need to move rule.input to an ordered set ?
                      // return potentials.contains(i)
                      //   ? potentials.at(i)[u]
                      //   : std::optional{0u};

                      auto&& e = unions.at(i)
                        | std::views::filter([&](auto&& i) noexcept {
                            return potentials.contains(i)
                               and potentials.at(i)[u] != std::numeric_limits<double>::signaling_NaN();
                        })
                        | std::views::transform([&](auto&& i) noexcept {
                            return potentials.at(i)[u];
                        })
                        | std::ranges::to<std::unordered_set>();
                      if (std::ranges::empty(e)) return std::numeric_limits<double>::signaling_NaN();
  
                      return *std::ranges::max_element(e);
                    }
                  );
              })
              | std::views::join;
          }
        )
        | std::views::join
        | std::views::filter([&potentials](auto&& _uio) noexcept {
            auto&& [u, i, o] = _uio;
            return o != Match::IGNORED_SYMBOL
               and potentials.contains(o)
               and potentials.at(o)[u] == std::numeric_limits<double>::signaling_NaN();
        })
        | std::views::transform([&, new_p](auto&& _uio) noexcept {
            auto&& [u, i, o] = _uio;
            potentials.at(o)[u] = new_p;
            return std::make_pair(o, u);
        });
    }
  );

  return potentials;
}

auto SearchEngine::updateFuture(const Grid<char>& grid, std::span<const RewriteRule>) noexcept -> std::vector<Change<char>> {
  if (not std::ranges::empty(future)) {
    return std::vector<Change<char>>{};
  }

  auto&& [changes, _future] = ::future(grid, observations);
  future = _future;

  return changes;
}
