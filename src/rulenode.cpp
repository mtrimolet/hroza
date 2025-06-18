module rulenode;

import sort;
import geometry;

import log;

using namespace stormkit;

auto RuleNode::operator()(const TracedGrid<char>& grid) noexcept -> std::vector<Change<char>> {
  {
    auto obsolete = std::ranges::remove_if(matches, std::not_fn(bindBack(&Match::match, grid, unions)));
    matches.erase(std::ranges::begin(obsolete), std::ranges::end(obsolete));
  }

  auto n = newMatches(grid);
  // ilog("{} new matches", std::ranges::size(n));
  matches.append_range(n
    | std::views::filter(bindBack(&Match::match, grid, unions)));
  active = std::ranges::begin(matches);

  // ilog("found {} matches", std::ranges::distance(active, std::ranges::cend(matches)));

  auto changes = std::vector<Change<char>>{};

  switch (mode) {
    case Mode::ONE:
      {
        changes.append_range(infer(grid));
        if (active != std::ranges::end(matches)) {
          active = std::ranges::prev(std::ranges::end(matches));
        }
      }
      break;

    case Mode::ALL:
      {
        changes.append_range(infer(grid));
        auto a = std::ranges::distance(active, std::ranges::end(matches));
        auto conflicts = std::ranges::remove_if(
          active, std::ranges::end(matches),
          [this](auto&& m) noexcept {
            return std::ranges::any_of(
              active, std::ranges::end(matches),
              bindBack(&Match::conflict, m)
            );
          }
        );
        auto c = std::ranges::size(conflicts);
        matches.erase(std::ranges::begin(conflicts), std::ranges::end(conflicts));
        active = std::ranges::prev(std::ranges::end(matches), a - c);
      }
      break;

    case Mode::PRL:
      {
        auto _active = std::ranges::partition(
          active, std::ranges::end(matches),
          std::not_fn([this](auto&& match) noexcept {
            return rules[match.r].draw(rng);
          })
        );
        active = std::ranges::begin(_active);
      }
      break;
  }

  // ilog("trigger {} matches", std::ranges::distance(active, std::ranges::cend(matches)));

  changes.append_range(std::ranges::subrange(active, std::ranges::cend(matches))
    | std::views::transform(bindBack(&Match::changes, grid))
    | std::views::join);
  matches.erase(active, std::ranges::end(matches));

  return changes;
}

template <>
struct std::hash<std::tuple<math::Vector3U, UInt>> {
  inline constexpr auto operator()(std::tuple<math::Vector3U, UInt> t) const noexcept -> std::size_t {
    return std::hash<math::Vector3U>{}(std::get<0>(t))
         ^ std::hash<UInt>{}(std::get<1>(t));
  }
};

auto RuleNode::newMatches(const TracedGrid<char>& grid) noexcept -> std::vector<Match> {
  auto now = std::ranges::cend(grid.history);
  auto since = prev
    .transform(bindFront(std::ranges::next, std::ranges::cbegin(grid.history)))
    .value_or(now);
  prev = std::ranges::size(grid.history);

  const auto g_area = grid.area();

  if (since != now) {
    return std::views::zip(rules, std::views::iota(0u))
      | std::views::transform([g_area, changes = std::ranges::subrange(since, now)](const auto& v) noexcept {
          const auto& [rule, r] = v;
          auto r_area = rule.area();
          auto zone = Area3U{g_area.u, g_area.size - r_area.shiftmax()};
          return changes
            | std::views::transform([neigh = rule.backward_neighborhood(), zone, r](const auto& c) noexcept {
                return std::views::zip(mdiota((neigh + c.u).umeet(zone)), std::views::repeat(r))
                  | std::ranges::to<std::unordered_set>();
            })
            // | std::views::transform([&size = r_area.size](const auto& c) noexcept {
            //     return c.u - (c.u % size);
            // })
            // | std::ranges::to<std::unordered_set>()
            // | std::views::transform([&grid, &ishifts = rule.ishifts, zone, r](auto u) noexcept {
            //     auto c = grid[u];
            //     auto bucket = ishifts.bucket(c);
            //     return std::views::zip(
            //       std::ranges::subrange(ishifts.cbegin(bucket), ishifts.cend(bucket))
            //         | std::views::transform([u] (const auto& v) noexcept {
            //             return u + std::get<1>(v);
            //         })
            //         | std::views::filter([zone](auto v) noexcept {
            //             return zone.contains(v);
            //         }),
            //       std::views::repeat(r)
            //     );
            // })
            | std::views::join;
      })
      | std::views::join
      | std::views::transform([this](auto&& ur) noexcept {
          return Match{rules, std::get<0>(ur), std::get<1>(ur)};
      })
      | std::ranges::to<std::vector>();
  }

  return std::views::zip(rules, std::views::iota(0u))
    | std::views::transform([&grid, g_area](const auto& v) noexcept {
        const auto& [rule, r] = v;
        auto r_area = rule.area();
        auto zone = Area3U{g_area.u, g_area.size - r_area.shiftmax()};
        return mdiota(zone)
          | std::views::filter([size = r_area.size](auto&& u) noexcept {
              return u % size == math::Vector3U{};
          })
          | std::views::transform([&grid, &ishifts = rule.ishifts, zone, r](auto u) noexcept {
              auto c = grid[u];
              auto bucket = ishifts.bucket(c);
              return std::views::zip(
                std::ranges::subrange(ishifts.cbegin(bucket), ishifts.cend(bucket))
                  | std::views::transform([u] (const auto& v) noexcept {
                      return u + std::get<1>(v);
                  })
                  | std::views::filter([zone](auto&& v) noexcept {
                      return zone.contains(v);
                  }),
                std::views::repeat(r)
              );
          })
          | std::views::join;
    })
    | std::views::join
    | std::views::transform([this](auto&& ur) noexcept {
        return Match{rules, std::get<0>(ur), std::get<1>(ur)};
    })
    | std::ranges::to<std::vector>();
}

// auto score_projection(const Grid<char>& grid, std::span<const Match> matches) noexcept -> std::function<double(const Match&)> {
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

auto RuleNode::infer(const Grid<char>& grid) noexcept -> std::vector<Change<char>> {
  auto changes = std::vector<Change<char>>{};

  switch (inference) {
    case Inference::RANDOM:
      {
        // shuffle matches
        const auto a = std::ranges::distance(active, std::ranges::end(matches));
        std::ranges::shuffle(std::ranges::subrange(active, std::ranges::end(matches)), rng);
        active = std::ranges::prev(std::ranges::end(matches), a);
      }
      break;
    case Inference::DISTANCE:
      {
        // update potentials
        std::ranges::for_each(
          fields
            | std::views::filter([&](auto&& _tuple) noexcept {
              auto&& [c, f] = _tuple;
              return not potentials.contains(c)
                  or f.recompute;
            }),
          [this, &grid](auto&& _tuple) noexcept {
            auto&& [c, f] = _tuple;
            auto&& p = f.potential(grid);
            if (std::ranges::none_of(p, [](auto&& p) static noexcept {
              return p == 0.0 or std::isnormal(p);
            })) {
              potentials.erase(c);
              return;
            }
            potentials.insert_or_assign(c, std::move(p));
          }
        );
        // if essential missing : invalidate all matches
        if (std::ranges::any_of(fields, [this](auto&& _f) noexcept {
          auto&& [c, f] = _f;
          return f.essential and not potentials.contains(c);
        }))
          active = std::ranges::begin(matches);
        // compute score projection
        // auto proj = score_projection(grid, matches);
        auto P = std::uniform_real_distribution{};
        auto proj = [&rng = rng, &P](auto) noexcept { return P(rng); };
        // sort matches using score projection
        ::sort(matches, {}, proj);
      }
      break;
    case Inference::OBSERVE:
      break;
    case Inference::SEARCH:
      break;
  }

  return changes;
}
