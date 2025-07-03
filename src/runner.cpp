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

// TODO this is problematically not extensible, but the other options I think of are about inheritance and I would prefer to avoid that
auto reset(NodeRunner& n) noexcept -> void {
  if (auto p = n.target<RuleRunner>(); p != nullptr and p->steps != 0) {
    p->step = 0;
  }

  if (auto p = n.target<TreeRunner>(); p != nullptr) {
    std::ranges::for_each(p->nodes, reset);
  }
}

auto TreeRunner::operator()(TracedGrid<char>& grid) noexcept -> bool {
  if (mode == Mode::SEQUENCE and current_node != std::ranges::end(nodes)) {
    current_node = std::ranges::find_if(
      current_node, std::ranges::end(nodes),
      [&grid](NodeRunner& n) noexcept { return n(grid); }
    );
    if (current_node != std::ranges::end(nodes)) {
      return true;
    }
    std::ranges::for_each(nodes, reset);
  }

  current_node = std::ranges::find_if(
    nodes,
    [&grid](NodeRunner& n) noexcept { return n(grid); }
  );
  return current_node != std::ranges::end(nodes);
}
