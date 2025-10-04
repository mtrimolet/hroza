module rulenode;

import sort;
import geometry;

import log;

using namespace stormkit;

auto RuleNode::operator()(const TracedGrid<char>& grid) noexcept -> std::vector<Change<char>> {
  matches.erase(
    std::remove_if(
      // std::execution::par,
      std::ranges::begin(matches),
      std::ranges::end(matches),
      std::not_fn(std::bind_back(&Match::match, grid))
    ),
    std::ranges::end(matches)
  );

  matches.append_range(scan(grid)
    | std::views::filter(std::bind_back(&Match::match, grid)));
  active = std::ranges::begin(matches);
  // ilog("working on {}", std::ranges::distance(active, std::ranges::end(matches)));

  auto changes = std::vector<Change<char>>{};

  switch (mode) {
    case Mode::ONE:
      changes.append_range(infer(grid));
      if (auto picked = pick(active, std::ranges::end(matches));
               picked != std::ranges::end(matches)
      ) {
        active = std::ranges::prev(std::ranges::end(matches));
        std::iter_swap(picked, active);
      }
      else {
        active = std::ranges::end(matches);
      }
      break;

    case Mode::ALL:
      changes.append_range(infer(grid));
      // ilog("weights: {}", std::ranges::subrange(active, std::ranges::end(matches))
      //                       | std::views::transform(&Match::w)
      //                       | std::ranges::to<std::vector>()
      // );
      for (auto selection = std::ranges::end(matches);
                selection != active;
      ) {
        if (auto picked = pick(active, selection);
                 picked != selection
        ) {
          auto conflict = std::any_of(
            // std::execution::par,
            selection, std::ranges::end(matches),
            std::bind_back(&Match::conflict, *picked)
          );
          std::iter_swap(
            picked,
            conflict ? active++ : --selection
          );
        }
        else {
          active = selection;
        }
      }
      break;

    case Mode::PRL:
      active = std::partition(
        // std::execution::par,
        active, std::ranges::end(matches),
        std::not_fn([this](const auto& match) noexcept {
          return rules[match.r].draw(rng);
        })
      );
      break;
  }

  if (active != std::ranges::end(matches))
    prev = std::ranges::size(grid.history);

  changes.append_range(std::ranges::subrange(active, std::ranges::cend(matches))
    | std::views::transform(std::bind_back(&Match::changes, grid))
    | std::views::join);
  matches.erase(active, std::ranges::end(matches));

  return changes;
}

template <>
struct std::hash<std::tuple<glm::vec<3, u32>, u32>> {
  inline constexpr auto operator()(std::tuple<glm::vec<3, u32>, u32> t) const noexcept -> std::size_t {
    return std::hash<glm::vec<3, u32>>{}(std::get<0>(t))
         ^ std::hash<u32>{}(std::get<1>(t));
  }
};

auto RuleNode::scan(const TracedGrid<char>& grid) noexcept -> std::vector<Match> {
  auto now = std::ranges::cend(grid.history);
  auto since = prev
    .transform(std::bind_front(std::ranges::next, std::ranges::cbegin(grid.history)))
    .value_or(now);

  if (since != now) {
    return std::views::zip(rules, std::views::iota(0u))
      | std::views::transform([&grid, changes = std::ranges::subrange(since, now)](const auto& v) noexcept {
          const auto& [rule, r] = v;
          auto zone = grid.area();
          return changes
            | std::views::transform([&rule](auto change) noexcept {
                return rule.get_ishifts(change.value)
                  | std::views::transform([u = change.u] (const auto& shift) noexcept {
                      return u - shift;
                  });
            })
            | std::views::join
            | std::views::filter(std::bind_front(&Area3U::contains, zone - Area3U{ {}, rule.area().shiftmax() }))
            | std::ranges::to<std::unordered_set>()
            | std::views::transform([r](auto u) noexcept {
                return std::tuple{ u, r };
            });
      })
      | std::views::join
      | std::views::transform([this](auto&& ur) noexcept {
          return Match{ rules, std::get<0>(ur), std::get<1>(ur) };
      })
      | std::ranges::to<std::vector>();
  }

  return std::views::zip(rules, std::views::iota(0u))
    | std::views::transform([&grid](const auto& v) noexcept {
        const auto& [rule, r] = v;
        auto zone = grid.area();
        return mdiota(zone)
          | std::views::filter([r_area = rule.area(), z_m = zone.shiftmax()](auto u) noexcept {
              auto r_m = r_area.shiftmax();
              auto umod = u % r_area.size;
              return (umod.x == r_m.x or u.x == z_m.x)
                 and (umod.y == r_m.y or u.y == z_m.y);
          })
          | std::views::transform([&grid, &rule](auto u) noexcept {
              return rule.get_ishifts(grid[u])
                | std::views::transform([u](const auto& shift) noexcept {
                    return u - shift;
                });
          })
          | std::views::join
          | std::views::filter(std::bind_front(&Area3U::contains, zone - Area3U{ {}, rule.area().shiftmax() }))
          | std::views::transform([r](auto u) noexcept {
              return std::tuple{ u, r };
          });
    })
    | std::views::join
    | std::views::transform([&rules = rules](auto&& ur) noexcept {
        return Match{ rules, std::get<0>(ur), std::get<1>(ur) };
    })
    | std::ranges::to<std::vector>();
}

auto RuleNode::pick(MatchIterator begin, MatchIterator end) noexcept -> MatchIterator {
  auto weights = std::ranges::subrange(begin, end)
               | std::views::transform(&Match::w);

  if (std::reduce(
    // std::execution::par,
    std::ranges::begin(weights),
    std::ranges::end(weights)
  ) == 0.0) {
    return end;
  }
  auto picker = std::discrete_distribution{std::ranges::begin(weights), std::ranges::end(weights)};
  auto picked = picker(rng);
  // ilog("{} picked {}", picker.probabilities(), picked);
  return std::ranges::next(begin, picked);
}

static constexpr auto is_normal = [](double value) static noexcept {
  return value == 0.0 or std::isnormal(value);
};

auto RuleNode::infer(const Grid<char>& grid) noexcept -> std::vector<Change<char>> {
  auto changes = std::vector<Change<char>>{};

  switch (inference) {
    case Inference::DISTANCE:
      // update potentials
      for (auto& [c, f] : fields) {
        if (potentials.contains(c) and not f.recompute) {
          continue;
        }

        if (not potentials.contains(c))
          potentials.emplace(c, Potential{ grid.extents, std::numeric_limits<double>::quiet_NaN() });
        f.potential(grid, potentials.at(c));

        if (f.essential and std::none_of(
          // std::execution::par,
          std::ranges::begin(potentials.at(c)),
          std::ranges::end(potentials.at(c)),
          is_normal
        )) {
          active = std::ranges::end(matches);
          break;
        }
      }

      // compute sum of pointwise differences in potential
      std::for_each(
        // std::execution::par,
        active, std::ranges::end(matches),
        [&potentials = potentials, &grid](auto& m) noexcept {
          m.w = m.delta(grid, potentials);
      });

      active = std::partition(
        // std::execution::par,
        active, std::ranges::end(matches),
        [](auto& m) noexcept {
          return not is_normal(m.w);
        }
      );

      if (active == std::ranges::end(matches)) break;

      if (temperature > 0.0) std::for_each(
        // std::execution::par,
        active, std::ranges::end(matches),
        [temperature = temperature, first_w = active->w](auto& m) noexcept {
          /** pre-Boltzmann distribution */
          m.w = std::exp(-m.w / temperature);
        }
      );
      else std::for_each(
        // std::execution::par,
        active, std::ranges::end(matches),
        [](auto& m) noexcept {
          /** pre-Softmax */
          m.w = std::exp(-m.w);
        }
      );

      break;
    case Inference::OBSERVE:
      // break;
    case Inference::SEARCH:
      // break;
    case Inference::RANDOM:
      // nothing to do, by default all weights are equal (= 1.0)
      break;
  }

  return changes;
}
