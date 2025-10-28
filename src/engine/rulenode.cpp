module engine.rulenode;

import sort;
import geometry;

import log;

using namespace stormkit;

RuleNode::RuleNode(RuleNode::Mode _mode, std::vector<RewriteRule>&& _rules, RewriteRule::Unions&& _unions) noexcept 
: mode{_mode}, rules{std::move(_rules)}, unions{std::move(_unions)}
{}

RuleNode::RuleNode(RuleNode::Mode _mode, std::vector<RewriteRule>&& _rules, RewriteRule::Unions&& _unions, Fields&& _fields, double _temperature) noexcept 
: mode{_mode}, rules{std::move(_rules)}, unions{std::move(_unions)},
  inference{Inference::DISTANCE}, temperature{_temperature}, fields{std::move(_fields)}
{}

RuleNode::RuleNode(RuleNode::Mode _mode, std::vector<RewriteRule>&& _rules, RewriteRule::Unions&& _unions, Observes&& _observes, double _temperature) noexcept 
: mode{_mode}, rules{std::move(_rules)}, unions{std::move(_unions)},
  inference{Inference::OBSERVE}, temperature{_temperature}, observes{std::move(_observes)}
{}

RuleNode::RuleNode(RuleNode::Mode _mode, std::vector<RewriteRule>&& _rules, RewriteRule::Unions&& _unions, Observes&& _observes, cpp::UInt _limit, double _depthCoefficient) noexcept 
: mode{_mode}, rules{std::move(_rules)}, unions{std::move(_unions)},
  inference{Inference::SEARCH}, limit{_limit}, depthCoefficient{_depthCoefficient}, observes{std::move(_observes)}
{}

auto RuleNode::operator()(const TracedGrid<char>& grid, std::vector<Change<char>>& changes) noexcept -> void {
  predict(grid, changes);
  scan(grid);
  select(grid);
  apply(grid, changes);
}

template <>
struct std::hash<std::tuple<glm::vec<3, u32>, u32>> {
  inline constexpr auto operator()(std::tuple<glm::vec<3, u32>, u32> t) const noexcept -> std::size_t {
    return std::hash<glm::vec<3, u32>>{}(std::get<0>(t))
         ^ std::hash<u32>{}(std::get<1>(t));
  }
};

auto RuleNode::scan(const TracedGrid<char>& grid) noexcept -> void {
  auto now = std::ranges::cend(grid.history);
  auto since = prev
    .transform(std::bind_front(std::ranges::next, std::ranges::cbegin(grid.history)))
    .value_or(now);

  matches.erase(
    std::remove_if(
      // std::execution::par,
      std::ranges::begin(matches),
      std::ranges::end(matches),
      std::not_fn(std::bind_back(&Match::match, grid))
    ),
    std::ranges::end(matches)
  );

  auto targets =
    since == now ? mdiota(grid.area()) | std::ranges::to<std::vector>()
    : std::ranges::subrange(since, now) | std::views::transform(&Change<char>::u) | std::ranges::to<std::vector>();

  matches.append_range(
    std::views::zip(rules, std::views::iota(0u))
      | std::views::transform([&grid, &targets](const auto& v) noexcept {
          const auto& [rule, r] = v;
          const auto zone = grid.area();
          const auto valid_zone = zone - Area3U{ {}, rule.output.area().shiftmax() };
          return targets
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
      | std::views::transform([this](auto&& ur) noexcept {
          return Match{ rules, std::get<0>(ur), std::get<1>(ur) };
      })
      | std::views::filter(std::bind_back(&Match::match, grid))
  );

  active = std::ranges::begin(matches);
}

auto RuleNode::apply(const TracedGrid<char>& grid, std::vector<Change<char>>& changes) -> void {
  if (active != std::ranges::end(matches))
    prev = std::ranges::size(grid.history);

  changes.append_range(
    std::ranges::subrange(active, std::ranges::cend(matches))
      | std::views::transform(std::bind_back(&Match::changes, grid))
      | std::views::join
  );

  matches.erase(active, std::ranges::end(matches));
}

auto RuleNode::predict(const Grid<char>& grid, std::vector<Change<char>>& changes) noexcept -> void {
  switch (inference) {
    case Inference::RANDOM:
      break;

    case Inference::DISTANCE:
      Field::potentials(fields, grid, potentials);
      if (std::ranges::any_of(
        fields,
        [&potentials = potentials](const auto& p) noexcept {
          const auto& [c, f] = p;
          return f.essential and not potentials.contains(c);
        }
      )) {
        active = std::ranges::end(matches);
        return;
      }
      break;

    case Inference::OBSERVE:
      if (not std::ranges::empty(future)) {
        return;
      }

      Observe::future(changes, future, grid, observes);
      if (std::ranges::empty(future)) {
        active = std::ranges::end(matches);
        return;
      }

      Observe::potentials(potentials, future, rules);

      break;

    case Inference::SEARCH:
      if (not std::ranges::empty(future)) {
        return;
      }

      Observe::future(changes, future, grid, observes);
      if (std::ranges::empty(future)) {
        active = std::ranges::end(matches);
        return;
      }

      // trajectory = Search::trajectory(future, grid, rules, limit);

      break;
  }
}

auto RuleNode::select(const Grid<char>& grid) noexcept -> void {
  switch (mode) {
    case Mode::ONE:
      infer(grid);
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
      infer(grid);
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

  return std::ranges::next(begin, picked);
}

auto RuleNode::infer(const Grid<char>& grid) noexcept -> void {
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

  if (active == std::ranges::end(matches)) return;

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
}
