module runner;

import log;

using namespace stormkit;

auto RuleRunner::operator()(TracedGrid<char>& grid) noexcept -> bool {
  if (steps != 0 and step >= steps) return false;
  // ilog("step {}", step);
  auto changes = rulenode(grid);
  if (std::ranges::empty(changes)) return false;

  std::ranges::for_each(changes, std::bind_front(&TracedGrid<char>::apply, &grid));
  step++;
  return true;
}

auto TreeRunner::operator()(TracedGrid<char>& grid) noexcept -> bool {
  current_node = std::ranges::find_if(
    mode == Mode::MARKOV or current_node == std::ranges::end(nodes) ? std::ranges::begin(nodes) : current_node,
    std::ranges::end(nodes),
    [&grid](auto& node) noexcept { return node(grid); }
  );
  return current_node != std::ranges::end(nodes);
}
