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

  matches.append_range(scan(grid)
    | std::views::filter(bindBack(&Match::match, grid, unions)));
  active = std::ranges::begin(matches);

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
        auto sampled = std::ranges::partition(
          active, std::ranges::end(matches),
          std::not_fn([this](auto&& match) noexcept {
            return rules[match.r].draw(rng);
          })
        );
        active = std::ranges::begin(sampled);
      }
      break;
  }

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

auto RuleNode::scan(const TracedGrid<char>& grid) noexcept -> std::vector<Match> {
  auto now = std::ranges::cend(grid.history);
  auto since = prev
    .transform(bindFront(std::ranges::next, std::ranges::cbegin(grid.history)))
    .value_or(now);
  prev = std::ranges::size(grid.history);

  if (since != now) {
    return std::views::zip(rules, std::views::iota(0u))
      | std::views::transform([g_area = grid.area(), changes = std::ranges::subrange(since, now)](const auto& v) noexcept {
          const auto& [rule, r] = v;
          auto r_area = rule.area();
          auto zone = Area3U{g_area.u, g_area.size - r_area.shiftmax()};
          return changes
            | std::views::transform([neigh = rule.backward_neighborhood(), zone, r](const auto& c) noexcept {
                return std::views::zip(mdiota((neigh + c.u).umeet(zone)), std::views::repeat(r))
                  | std::ranges::to<std::unordered_set>();
            })
            // | std::views::transform([size = r_area.size](const auto& c) noexcept {
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
    | std::views::transform([&grid, &unions = unions](const auto& v) noexcept {
        const auto& [rule, r] = v;
        return mdiota(grid.area() - Area3U{{}, rule.area().shiftmax()})
          | std::views::filter([r_area = rule.area()](auto u) noexcept {
             return u % r_area.size == math::Vector3U{};
          })
          | std::views::transform([&grid, &unions, &rule](auto u) noexcept {
              return unions
                | std::views::filter([c = grid[u]](const auto& cs) noexcept {
                    return std::get<1>(cs).contains(c);
                  })
                | std::views::transform([&rule](const auto& cs) noexcept {
                    auto bucket = rule.ishifts.bucket(std::get<0>(cs));
                    return std::ranges::subrange(
                      rule.ishifts.cbegin(bucket),
                      rule.ishifts.cend(bucket)
                    ) | std::views::transform(monadic::get<1>());
                })
                | std::views::join
                | std::views::transform([u] (const auto& shift) noexcept {
                    return u + shift;
                });
          })
          | std::views::join
          | std::views::filter(bindFront(&Area3U::contains, grid.area() - Area3U{{}, rule.area().shiftmax()}))
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

auto RuleNode::infer(const Grid<char>& grid) noexcept -> std::vector<Change<char>> {
  auto changes = std::vector<Change<char>>{};

  switch (inference) {
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
        })) {
          ilog("essential missing");
          active = std::ranges::end(matches);
        }

        // compute scores
        std::ranges::for_each(
          active, std::ranges::end(matches),
          [this, &grid, U = std::uniform_real_distribution{}](auto& m) mutable noexcept {
            auto w = delta(grid, m);
            auto u = U(rng);

            m.score = 
              /** Boltzmann distribution: `p(r) ~ exp(-w(r)/t)` */
              temperature > 0.0 ? std::pow(u, std::exp(w / temperature))   
                                : -w + 0.001 * u;
          }
        );
      }
      break;
    case Inference::OBSERVE:
      // break;
    case Inference::SEARCH:
      // break;
    case Inference::RANDOM:
      {
        // shuffle matches
        std::ranges::for_each(
          active, std::ranges::end(matches),
          [this, U = std::uniform_real_distribution{}](auto& m) mutable noexcept {
            m.score = U(rng);
          }
        );
      }
      break;
  }

  static constexpr auto is_normal = [](double value) static noexcept {
    return value == 0.0 or std::isnormal(value);
  };

  auto normal = std::ranges::partition(
    active, std::ranges::end(matches),
    std::not_fn(is_normal),
    &Match::score
  );
  active = std::ranges::begin(normal);

  const auto a = std::ranges::distance(active, std::ranges::end(matches));
  std::ranges::sort(
    active, std::ranges::end(matches),
    {},
    &Match::score
  );
  active = std::ranges::prev(std::ranges::end(matches), a);

  return changes;
}

auto RuleNode::delta(const Grid<char>& grid, const Match& match) noexcept -> double {
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
