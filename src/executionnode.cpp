module executionnode;

import std;
import stormkit.core;

import log;
import grid;

using namespace stormkit;

auto ActionNodeBase::operator()(TracedGrid<char>& grid) noexcept -> std::vector<Change<char>> {
  auto&& changes = action(grid);
  std::ranges::for_each(changes, bindFront(&TracedGrid<char>::apply, &grid));
  return changes;
}

auto NoLimit::operator()(TracedGrid<char>& grid) noexcept -> std::generator<std::vector<Change<char>>> {
  auto&& changes = ActionNodeBase::operator()(grid);
  if (std::ranges::empty(changes)) co_return;
  co_yield std::move(changes);
}

auto Limit::operator()(TracedGrid<char>& grid) noexcept -> std::generator<std::vector<Change<char>>> {
  if (count == 0) co_return;
  
  auto&& changes = ActionNodeBase::operator()(grid);
  if (std::ranges::empty(changes)) co_return;

  count--;
  co_yield std::move(changes);
}

auto Sequence::operator()(TracedGrid<char>& grid) noexcept -> std::generator<std::vector<Change<char>>> {
  current_node = std::ranges::begin(nodes);
  while (current_node != std::ranges::end(nodes)) {
    auto&& found = false;
    for (auto&& changes : (*current_node)(grid)) {
      if (not found) found = true;
      co_yield changes;
    }
    if (not found)
      current_node++;
  }
}

auto Markov::operator()(TracedGrid<char>& grid) noexcept -> std::generator<std::vector<Change<char>>> {
  current_node = std::ranges::begin(nodes);
  while (current_node != std::ranges::end(nodes)) {
    auto&& found = false;
    for (auto&& changes : (*current_node)(grid)) {
      if (not found) found = true;
      co_yield changes;
    }
    if (found)
      current_node = std::ranges::begin(nodes);
    else
      current_node++;
  }
}
