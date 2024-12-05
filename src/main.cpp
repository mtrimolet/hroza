#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
// import glm;
import stormkit.Core;
import ranges;

import grid;
import rule;
import node;

// using namespace std::literals;
using namespace stormkit;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");

  std::println("hello world!");

  const auto seq_snake = Sequence{{
    NoLimit{One{{
      {{{{{{'W'}},{{'B'}},{{'B'}}}}}, {{{'P','E','R'}}}},
      {{{{{{'B'}},{{'B'}},{{'W'}}}}}, {{{'R','E','P'}}}},
      {{{{{{'W'}}},{{{'B'}}},{{{'B'}}}}}, {{{'P'},{'E'},{'R'}}}},
      {{{{{{'B'}}},{{{'B'}}},{{{'W'}}}}}, {{{'R'},{'E'},{'P'}}}},
    }}},
    Limit{10, One{{
      {{{{{{'R'}},{{'B'}},{{'B'}}}}}, {{{'E','E','R'}}}},
      {{{{{{'B'}},{{'B'}},{{'R'}}}}}, {{{'R','E','E'}}}},
      {{{{{{'R'}}},{{{'B'}}},{{{'B'}}}}}, {{{'E'},{'E'},{'R'}}}},
      {{{{{{'B'}}},{{{'B'}}},{{{'R'}}}}}, {{{'R'},{'E'},{'E'}}}},
    }}},
    Markov{{
      NoLimit{One{{
        {{{{{{'R'}},{{'B'}},{{'B'}}}}}, {{{'G','G','U'}}}},
        {{{{{{'B'}},{{'B'}},{{'R'}}}}}, {{{'U','G','G'}}}},
        {{{{{{'R'}}},{{{'B'}}},{{{'B'}}}}}, {{{'G'},{'G'},{'U'}}}},
        {{{{{{'B'}}},{{{'B'}}},{{{'R'}}}}}, {{{'U'},{'G'},{'G'}}}},
        {{{{{{'E'}},{{'E'}},{{'G'}}}}}, {{{'G','G','G'}}}},
        {{{{{{'G'}},{{'E'}},{{'E'}}}}}, {{{'G','G','G'}}}},
        {{{{{{'E'}}},{{{'E'}}},{{{'G'}}}}}, {{{'G'},{'G'},{'G'}}}},
        {{{{{{'G'}}},{{{'E'}}},{{{'E'}}}}}, {{{'G'},{'G'},{'G'}}}},
        {{{{{{'P'}},{{'E'}},{{'G'}}}}}, {{{'B','B','P'}}}},
        {{{{{{'G'}},{{'E'}},{{'P'}}}}}, {{{'P','B','B'}}}},
        {{{{{{'P'}}},{{{'E'}}},{{{'G'}}}}}, {{{'B'},{'B'},{'P'}}}},
        {{{{{{'G'}}},{{{'E'}}},{{{'P'}}}}}, {{{'P'},{'B'},{'B'}}}},
      }}},
      NoLimit{All{{
        {{{{{{'G'}}}}}, {{{'E'}}}},
        {{{{{{'U'}}}}}, {{{'R'}}}},
      }}},
      NoLimit{All{{
        {{{{{{'R'}}}}}, {{{'P'}}}},
        {{{{{{'P'}}}}}, {{{'R'}}}},
      }}},
    }},
  }};
  
  const auto growth = Markov{{
    NoLimit{One{{
      {{{{{{'W'}}, {{'B'}}}}}, {{{'W', 'W'}}}},
      {{{{{{'B'}}, {{'W'}}}}}, {{{'W', 'W'}}}},
      {{{{{{'B'}}}, {{{'W'}}}}}, {{{'W'}, {'W'}}}},
      {{{{{{'W'}}}, {{{'B'}}}}}, {{{'W'}, {'W'}}}},
    }}}
  }};

  auto grid = Grid<>{{
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','W','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
    {'B','B','B','B','B','B','B','B','B','B','B','B','B'},
  }};
  for (auto y : std::views::iota(0u, grid.size.y))
    std::println("{}", std::ranges::subrange(
      std::begin(grid.values) + y * grid.size.x,
      std::begin(grid.values) + (y + 1) * grid.size.x
    ));
  std::println();
  for (auto current_grid : seq_snake(grid)) {
  // for (auto current_grid : growth(grid)) {
    for (auto y : std::views::iota(0u, current_grid.size.y))
      std::println("{}", std::ranges::subrange(
        std::begin(current_grid.values) + y * current_grid.size.x,
        std::begin(current_grid.values) + (y + 1) * current_grid.size.x
      ));
    std::println();
  }

  return 0;
}