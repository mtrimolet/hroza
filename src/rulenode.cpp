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
      std::not_fn(std::bind_back(&Match::match, grid, unions))
    ),
    std::ranges::end(matches)
  );

  matches.append_range(scan(grid)
    | std::views::filter(std::bind_back(&Match::match, grid, unions)));
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
      for (auto __i = std::ranges::end(matches);
                __i != active;
      ) {
        if (auto picked = pick(active, __i);
                 picked != __i
        ) {
          auto conflict = std::any_of(
            // std::execution::par,
            __i, std::ranges::end(matches),
            std::bind_back(&Match::conflict, *picked)
          );
          std::iter_swap(
            picked,
            conflict ? active++ : --__i
          );
        }
        else {
          active = __i;
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
struct std::hash<std::tuple<math::Vector3U, cpp::UInt>> {
  inline constexpr auto operator()(std::tuple<math::Vector3U, cpp::UInt> t) const noexcept -> std::size_t {
    return std::hash<math::Vector3U>{}(std::get<0>(t))
         ^ std::hash<cpp::UInt>{}(std::get<1>(t));
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
          auto zone = grid.area() - Area3U{{}, rule.area().shiftmax()};
          return changes
            | std::views::transform([neigh = rule.backward_neighborhood(), zone](const auto& c) noexcept {
                return mdiota((neigh + c.u).umeet(zone));
            })
            | std::views::join
            // | std::views::filter([r_area = rule.area()](auto u) noexcept {
            //    return u % r_area.size == math::Vector3U{};
            // })
            // // | std::views::transform([r_area = rule.area()](const auto& c) noexcept {
            // //     // ilog("cu [{}] - (c.u % r_area.size [{}]) = {}", c.u, r_area.size, c.u - (c.u % r_area.size));
            // //     return c.u - (c.u % r_area.size);
            // // })
            // | std::views::transform([&grid, &unions, &rule](auto u) noexcept {
            //     return unions
            //       | std::views::filter([c = grid[u]](const auto& cs) noexcept {
            //           return std::get<1>(cs).contains(c);
            //         })
            //       | std::views::transform([&rule](const auto& cs) noexcept {
            //           auto bucket = rule.ishifts.bucket(std::get<0>(cs));
            //           return std::ranges::subrange(
            //             rule.ishifts.cbegin(bucket),
            //             rule.ishifts.cend(bucket)
            //           ) | std::views::transform(monadic::get<1>());
            //       })
            //       | std::views::join
            //       | std::views::transform([u] (const auto& shift) noexcept {
            //           return u + shift;
            //       });
            // })
            // | std::views::join
            // | std::views::filter(std::bind_front(&Area3U::contains, zone))
            | std::ranges::to<std::unordered_set>()
            | std::views::transform([r](auto u) noexcept {
                return std::make_pair(u, r);
            });
      })
      | std::views::join
      | std::views::transform([this](auto&& ur) noexcept {
          return Match{rules, std::get<0>(ur), std::get<1>(ur)};
      })
      | std::ranges::to<std::vector>();
  }

  return std::views::zip(rules, std::views::iota(0u))
    | std::views::transform([&grid](const auto& v) noexcept {
        const auto& [rule, r] = v;
        auto zone = grid.area() - Area3U{{}, rule.area().shiftmax()};
        return mdiota(zone)
          // | std::views::filter([r_area = rule.area()](auto u) noexcept {
          //    return u % r_area.size == math::Vector3U{};
          // })
          // | std::views::transform([&grid, &unions, &rule](auto u) noexcept {
          //     return unions
          //       | std::views::filter([c = grid[u]](const auto& cs) noexcept {
          //           return std::get<1>(cs).contains(c);
          //         })
          //       | std::views::transform([&rule](const auto& cs) noexcept {
          //           auto bucket = rule.ishifts.bucket(std::get<0>(cs));
          //           return std::ranges::subrange(
          //             rule.ishifts.cbegin(bucket),
          //             rule.ishifts.cend(bucket)
          //           ) | std::views::transform(monadic::get<1>());
          //       })
          //       | std::views::join
          //       | std::views::transform([u] (const auto& shift) noexcept {
          //           return u + shift;
          //       });
          // })
          // | std::views::join
          // | std::views::filter(std::bind_front(&Area3U::contains, zone))
          | std::views::transform([r](auto u) noexcept {
              return std::make_pair(u, r);
          });
    })
    | std::views::join
    | std::views::transform([this](auto&& ur) noexcept {
        return Match{rules, std::get<0>(ur), std::get<1>(ur)};
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

        if (auto p = f.potential(grid); std::any_of(
              // std::execution::par,
              std::ranges::begin(p),
              std::ranges::end(p),
              is_normal)
        ) {
          potentials.insert_or_assign(c, std::move(p));
        }

        if (not potentials.contains(c) and f.essential) {
          active = std::ranges::end(matches);
          break;
        }
      }
      // compute weights
      std::for_each(
        // std::execution::par,
        active, std::ranges::end(matches),
        [this, &grid](auto& m) noexcept {
          auto d = m.delta(grid, potentials);
          m.w = std::isnormal(d) ? -d : 0.0;
          /** Boltzmann distribution: `p(r) ~ exp(-w(r)/t)` */
          if (temperature > 0.0) m.w = std::exp(m.w / temperature);
          // exp(0) == 1 ... this might be a problem, or this might be of interest :
          // heating makes you consider discarded matches, like quantum teleportation y'know
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
