module engine.runner;

import log;

using namespace stormkit;

auto RuleRunner::operator()(TracedGrid<char>& grid) noexcept -> std::generator<bool> {
  if (steps > 0 and step >= steps) co_return;

  auto changes = rulenode(grid);
  if (std::ranges::empty(changes)) co_return;

  std::for_each(
    // std::execution::par,
    std::ranges::begin(changes),
    std::ranges::end(changes),
    std::bind_front(&TracedGrid<char>::apply, &grid)
  );
  step++;
  co_yield true;
}

// TODO this is problematically not extensible, but the other options I think of are about inheritance and I would prefer to avoid that
auto reset(NodeRunner& n) noexcept -> void {
  if (auto p = n.target<RuleRunner>(); p != nullptr) {
    p->step = 0;
  }

  if (auto p = n.target<TreeRunner>(); p != nullptr) {
    std::ranges::for_each(p->nodes, reset);
  }
}

auto current(const NodeRunner& n) noexcept -> const RuleRunner::RuleNode* {
  if (auto p = n.target<RuleRunner>(); p != nullptr) {
    return &p->rulenode;
  }

  if (auto p = n.target<TreeRunner>(); p != nullptr) {
    if (p->current_node == std::ranges::end(p->nodes))
      return nullptr;
    return current(*p->current_node);
  }

  std::unreachable();
}

auto TreeRunner::operator()(TracedGrid<char>& grid) noexcept -> std::generator<bool> {
  for (current_node  = std::ranges::begin(nodes);
       current_node != std::ranges::end(nodes);
  ) {
    auto found = false;
    for (auto s : (*current_node)(grid)) {
      found = true;
      co_yield s;
    }

    if (not found) current_node++;

    else if (mode == Mode::MARKOV) current_node = std::ranges::begin(nodes);
  }

  std::ranges::for_each(nodes, reset);
}
