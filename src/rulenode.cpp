module rulenode;

import sort;
import geometry;

import log;
import match;

using namespace stormkit;

auto InferenceEngine::infer(const TracedGrid<char>& grid) noexcept -> std::vector<Change<char>> {
  auto changes = std::vector<Change<char>>{};

  if (    not std::ranges::empty(search.observations)
      and     std::ranges::empty(search.future)
  ) {
    changes.append_range(search.updateFuture(grid, rules));
  }

  else if (    not std::ranges::empty(observe.observations)
           and     std::ranges::empty(observe.future)
  ) {
    changes.append_range(observe.updateFuture(grid, rules));
  }

  if (not std::ranges::empty(search.trajectory)) {
    return search.followTrajectory();
  }

  if (not std::ranges::empty(dijkstra.fields)) {
    dijkstra.updatePotentials(grid);
    if (dijkstra.essential_missing()) {
      matches.clear();
      return changes;
    }
  }

  if (not std::ranges::empty(observe.observations)) {
    ::sort(matches, {}, observe.score_projection(grid, matches));
  }
  else if (not std::ranges::empty(dijkstra.fields)) {
    // ilog("(dijkstra) matches: {}", std::ranges::size(matches));
    auto&& proj = dijkstra.score_projection(grid, matches);
    // auto&& erased = std::ranges::remove_if(matches, [](auto&& p) static noexcept { return p == std::numeric_limits<double>::signaling_NaN(); }, proj);
    // matches.erase(std::ranges::begin(erased), std::ranges::end(erased));
    // ilog("(dijkstra) remaining matches: {}", std::ranges::size(matches));
    ::sort(matches, {}, proj);
  }
  else {
    std::ranges::shuffle(matches, std::mt19937{});
  }

  return changes;
}

auto One::operator()(const TracedGrid<char>& grid) noexcept -> std::vector<Change<char>> {
  updateMatches(grid, grid.history);

  auto changes = infer(grid);

  auto&& triggered = std::ranges::find_last_if(matches, bindBack(&Match::match, grid, unions));
  changes.append_range(triggered
    | std::views::take(1)
    | std::views::transform(bindBack(&Match::changes, grid))
    | std::views::join);

  if (std::ranges::empty(triggered)) matches.clear();
  else matches.erase(
    std::ranges::begin(triggered), 
    std::ranges::end(triggered)
  );

  return changes;
}

auto Prl::operator()(const TracedGrid<char>& grid) noexcept -> std::vector<Change<char>> {
  updateMatches(grid, grid.history);

  static auto rg = std::mt19937{std::random_device{}()};
  auto&& triggered = std::ranges::partition(matches, std::not_fn([](auto&& match) static noexcept {
    static auto prob = std::uniform_real_distribution<>{};
    return match.rule.p == 1.0 or prob(rg) <= match.rule.p;
  }));

  auto changes = triggered
    | std::views::filter(bindBack(&Match::match, grid, unions))
    | std::views::transform(bindBack(&Match::changes, grid))
    | std::views::join
    | std::ranges::to<std::vector>();

  if (std::ranges::empty(triggered)) matches.clear();
  else matches.erase(
    std::ranges::begin(triggered),
    std::ranges::end(triggered)
  );

  return changes;
}

inline constexpr auto removeOverlaps(std::ranges::input_range auto&& matches) noexcept -> decltype(auto) {
  return std::ranges::fold_left(std::move(matches), std::vector<Match>{},
    [](auto&& triggered, auto&& match) static noexcept {
      if (std::ranges::none_of(triggered, [&match](auto&& visited) noexcept {
          auto&& overlap = visited.area().meet(match.area());
          return std::ranges::any_of(
            mdiota(overlap),
            [](auto&& p) static noexcept {
              return std::get<0>(p) != Match::IGNORED_SYMBOL
                 and std::get<1>(p) != Match::IGNORED_SYMBOL;
            },
            [&visited, &match](auto&& u) noexcept {
              return std::make_tuple(
                visited.rule.output.at(u - visited.u),
                match.rule.output.at(u - match.u)
              );
            }
          );
        }
      ))
        triggered.push_back(std::move(match));
      return triggered;
    }
  );
}

auto All::operator()(const TracedGrid<char>& grid) noexcept -> std::vector<Change<char>> {
  updateMatches(grid, grid.history);

  auto changes = infer(grid);

  auto&& triggered = removeOverlaps(
    std::views::reverse(std::move(matches))
      | std::views::filter(bindBack(&Match::match, grid, unions))
  );
  changes.append_range(std::move(triggered)
    | std::views::transform(bindBack(&Match::changes, grid))
    | std::views::join);

  matches.clear();

  return changes;
}
