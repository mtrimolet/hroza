module runner;

import log;

using namespace stormkit;

auto RuleRunner::operator()(TracedGrid<char>& grid) noexcept -> std::generator<std::vector<Change<char>>> {
  if (steps != 0 and step >= steps) co_return;

  auto&& changes = rulenode(grid);
  std::ranges::for_each(changes, bindFront(&TracedGrid<char>::apply, &grid));
  step++;

  if (std::ranges::empty(changes)) co_return;

  co_yield std::move(changes);
}

auto TreeRunner::operator()(TracedGrid<char>& grid) noexcept -> std::generator<std::vector<Change<char>>> {
  current_node = std::ranges::begin(nodes);
  while (current_node != std::ranges::end(nodes)) {
    auto&& found = false;
    for (auto&& changes : (*current_node)(grid)) {
      if (not found) found = true;
      co_yield changes;
    }
    if (not found)
      current_node++;
    else if (mode == Mode::MARKOV)
      current_node = std::ranges::begin(nodes);
  }
}
