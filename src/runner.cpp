module runner;

import log;

using namespace stormkit;

auto RuleRunner::operator()(TracedGrid<char>& grid) noexcept -> std::generator<std::vector<Change<char>>> {
  for (
    ;
    steps == 0 or step < steps;
    step++
  ) {
    auto changes = rulenode(grid);
    std::ranges::for_each(changes, bindFront(&TracedGrid<char>::apply, &grid));
    if (std::ranges::empty(changes)) co_return;
    co_yield std::move(changes);
  }
  co_return;
}

auto TreeRunner::operator()(TracedGrid<char>& grid) noexcept -> std::generator<std::vector<Change<char>>> {
  auto found = false;
  for (
    current_node = std::ranges::begin(nodes);
    current_node != std::ranges::end(nodes);
    current_node =
        not found ? std::ranges::next(current_node)
      : mode == Mode::MARKOV ? std::ranges::begin(nodes)
      : current_node,
    found = false
  ) {
    for (auto&& changes : (*current_node)(grid)) {
      if (not found) found = true;
      co_yield std::move(changes);
    }
  }
}
