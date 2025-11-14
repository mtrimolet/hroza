module engine.search;

import glm;
import geometry;

import engine.match;

template <>
struct std::hash<Grid<char>::Extents> {
  inline constexpr auto operator()(Grid<char>::Extents t) const noexcept -> std::size_t {
    return std::hash<decltype(t.extent(0))>{}(t.extent(0))
       and std::hash<decltype(t.extent(1))>{}(t.extent(1))
       and std::hash<decltype(t.extent(2))>{}(t.extent(2));
  }
};

template <>
struct std::hash<std::vector<char>> {
  inline constexpr auto operator()(const std::vector<char>& t) const noexcept -> std::size_t {
    auto s = std::size_t{ 0 };
    for (auto c : t)
      s ^= std::hash<char>{}(c);
    return s;
  }
};

template <>
struct std::hash<Grid<char>> {
  inline constexpr auto operator()(const Grid<char>& grid) const noexcept -> std::size_t {
    return std::hash<decltype(grid.extents)>{}(grid.extents)
         ^ std::hash<decltype(grid.values)>{}(grid.values);
  }
};

auto Search::trajectory(
  Trajectory& traj,
  const Future& future,
  const Grid<char>& grid,
  std::span<const RewriteRule> rules,
  bool all, u32 limit, double depthCoefficient
) -> void {
  // traj = {};
  auto candidates = std::vector<Candidate>{};

  Potentials backward, forward;

  Observe::backward_potentials(backward, future, rules);
  Search::forward_potentials(forward, grid, rules);
  
  candidates.emplace_back(
    grid, -1, 0,
    Search::backward_delta(backward, grid),
    Search::forward_delta(forward, future) 
  );

  if (candidates[0].backward < 0.0 or candidates[0].forward < 0.0) {
    // traj = {};
    return;
  }
  if (candidates[0].backward == 0.0) {
    // traj = {};
    return;
  }

  auto visited = std::views::zip(
    candidates | std::views::transform(&Candidate::state),
    std::views::iota(std::size_t{ 0u })
  )
    | std::ranges::to<std::unordered_map>();

  for (
    auto q = std::views::zip(
      candidates | std::views::transform(std::bind_back(&Candidate::weight, depthCoefficient)),
      std::views::iota(0u)
    )
      | std::ranges::to<std::priority_queue>([](const auto& a, const auto& b){
          return std::get<0>(a) - std::get<0>(b);
      });
    not std::ranges::empty(q) and (limit == 0 or std::ranges::size(candidates) < limit);
    q.pop()
  ) {
    auto [score, parentIndex]  = q.top();
    const auto& parent = candidates[parentIndex];

    for (auto childState : parent.children(rules, all)) {
      if (visited.contains(childState)) {
        auto childIndex = visited.at(childState);

        auto& child = candidates[childIndex];
        if (child.depth <= parent.depth + 1) {
          continue;
        }
        
        child.depth = parent.depth + 1;
        child.parentIndex = parentIndex;

        if (child.backward < 0.0 or child.forward < 0.0) {
          continue;
        }

        q.emplace(child.weight(depthCoefficient), childIndex);
      }
      else {
        auto backward_estimate = Search::backward_delta(backward, childState);
        Search::forward_potentials(forward, childState, rules);
        auto forward_estimate  = Search::forward_delta(forward, future);
        if (backward_estimate < 0.0 or forward_estimate < 0.0) {
          continue;
        }

        auto childIndex = std::ranges::size(candidates);
        visited.emplace(childState, childIndex);
        candidates.emplace_back(
          childState, parentIndex, parent.depth + 1,
          backward_estimate,
          forward_estimate
        );

        auto& child = candidates[childIndex];

        if (child.forward == 0.0) {
          q = {}; // end the propagation
          break;
        }

        // if (limit == 0 and backward_estimate + forward_estimate <= record) {
        //   record = backward_estimate + forward_estimate;
        // }

        q.emplace(child.weight(depthCoefficient), childIndex);
      }
    }
  }

  if (std::ranges::empty(candidates)
   or std::ranges::prev(std::ranges::end(candidates))->forward != 0.0
  ) {
    // traj = {};
    return;
  }

  // TODO use child.depth to resize traj
  for (auto candidate = std::ranges::prev(std::ranges::cend(candidates));
       candidate->parentIndex >= 0;
       candidate = std::ranges::next(std::ranges::cbegin(candidates), candidate->parentIndex)
  ) {
    traj.emplace(std::ranges::begin(traj), candidate->state);
  }
}

auto Search::forward_potentials(Potentials& potentials, const Grid<char>& grid, std::span<const RewriteRule> rules) noexcept -> void {
  propagate(
    std::views::zip(mdiota(grid.area()), grid)
      | std::views::transform([&potentials](const auto& p) noexcept {
          auto [u, c] = p;
          potentials.at(c)[u] = 0.0;
          return std::tuple{ u, c };
      }),
    [&potentials, &rules](auto&& front) noexcept {
      auto [u, c] = front;
      auto p = potentials.at(c)[u];
      return std::views::iota(0u, std::ranges::size(rules))
        | std::views::transform([&rules, u](auto r) noexcept {
            return Match{ rules, u, r };
        })
        | std::views::filter(std::bind_back(&Match::forward_match, potentials, p))
        | std::views::transform(std::bind_back(&Match::forward_changes, potentials, p + 1))
        | std::views::join
        | std::views::transform([&potentials](auto&& ch) noexcept {
            auto [c, p] = ch.value;
            potentials.at(c)[ch.u] = p;
            return std::tuple{ ch.u, c };
        });
    }
  );
}

auto Search::backward_delta(const Potentials& potentials, const Grid<char>& grid) noexcept -> double {
  auto vals = std::views::zip(mdiota(grid.area()), grid)
    | std::views::transform([&potentials] (const auto& locus) noexcept {
        auto [u, value] = locus;
        return potentials.contains(value) ? potentials.at(value)[u]
          : 0.0;
    });
  
  return std::reduce(
    // std::execution::par,
    std::ranges::begin(vals),
    std::ranges::end(vals)
  );
}

auto Search::forward_delta(const Potentials& potentials, const Future& future) noexcept -> double {
  auto vals = std::views::zip(mdiota(future.area()), future)
    | std::views::transform([&potentials] (const auto& locus) noexcept {
        auto [u, value] = locus;
        if (std::ranges::empty(value)) {
          return std::numeric_limits<double>::quiet_NaN();
        }

        auto candidates = potentials
          | std::views::transform([u] (const auto& pot){
              const auto& [c, potential] = pot;
              return potential[u];
          })
          | std::views::filter(is_normal);
        return std::ranges::empty(candidates) ? std::numeric_limits<double>::quiet_NaN()
          : std::ranges::min(candidates);
    });
  
  return std::reduce(
    // std::execution::par,
    std::ranges::begin(vals),
    std::ranges::end(vals)
  );
}

auto Candidate::weight(double depthCoefficient) const -> double {
  return depthCoefficient < 0.0 ? 1000.0 - static_cast<double>(depth)
    : forward + backward + 2.0 * depthCoefficient * static_cast<double>(depth);
}

// TODO maybe avoid duplication of rulenode logic ?
auto Candidate::children(std::span<const RewriteRule> rules, bool all) const -> std::vector<Grid<char>> {
  // simulate rulenode (one | all)
  auto result = std::vector<Grid<char>>{};
  // scan matches
  auto matches = std::views::zip(rules, std::views::iota(0u))
    | std::views::transform([&grid = state](const auto& v) noexcept {
        const auto& [rule, r] = v;
        const auto zone = grid.area();
        const auto valid_zone = zone - Area3U{ {}, rule.output.area().shiftmax() };
        return mdiota(grid.area())
          // | std::views::transform([r_area = rule.output.area(), zone](auto u) noexcept {
          //     return glm::min(
          //       u - (u % r_area.size) + r_area.shiftmax(),
          //       zone.shiftmax()
          //     );
          // })
          // | std::ranges::to<std::unordered_set>()
          // TODO group changes according to rule size
          // currently this is highly redundant on adjacent changes (which happens a lot..)
          // This is the old way, it is not a grouping but a filter so it only applies to full grid scan
          // | std::views::filter([r_area = rule.output.area(), zone](auto u) noexcept {
          //     return glm::all(
          //          glm::equal(u, zone.shiftmax())
          //       or glm::equal(u % r_area.size, r_area.shiftmax())
          //     );
          // })
          | std::views::transform([&grid, &rule](auto u) noexcept {
              return rule.get_ishifts(grid[u])
                | std::views::transform([u](const auto &shift) noexcept {
                      return u - shift;
                });
          })
          | std::views::join
          | std::views::filter(std::bind_front(&Area3U::contains, valid_zone))
          | std::ranges::to<std::unordered_set>()
          | std::views::transform([r](auto u) noexcept {
              return std::tuple{ u, r };
          });
    })
    | std::views::join
    | std::views::transform([&rules](auto&& ur) noexcept {
        return Match{ rules, std::get<0>(ur), std::get<1>(ur) };
    })
    | std::views::filter(std::bind_back(&Match::match, state));
  if (all) {
    // all :
    //   non overlaping matches induce a common substate when applied simultaneously
    //   overlaping matches induce a combinatoric of substates when applied concurrently
    //     cartesian product of the overlaping rules grouped by joined overlapping area
    // mock:
    auto common_substate = state;
    std::ranges::for_each(
      matches
        | std::views::transform(std::bind_back(&Match::changes, common_substate))
        | std::views::join,
      [&common_substate](auto&& c) {
        common_substate[c.u] = c.value;
    });
    // real :
    // fill hitgrid (Grid<u32>)
    // recursively enumerate while decrementing hitgrid :
    //   find u : location of hitgrid with highest nonzero value
    //   if none, we're done with this recursion :
    //       apply current sequence and push result
    result.push_back(common_substate);
    //   for each match m hitting u :
    //     recurse enumeration with :
    //       hitgrid decremented on m.area
    //       matches without those intersecting m
    //       current sequence appended with m
    //   
  }
  else {
    // one :
    //   each match gives an induced state when applied individually
    result.append_range(
      matches
        | std::views::transform([&state = state](auto&& m) {
          auto newstate = state;
          std::ranges::for_each(
            m.changes(newstate),
            [&newstate](auto&& c) {
              newstate[c.u] = c.value;
          });
          return newstate;
        })
    );
  }

  return result;
}

