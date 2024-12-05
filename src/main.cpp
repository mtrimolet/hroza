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

  // const auto seq_snake = Node{bindFront(sequence, makeStaticArray(
  //   Node{bindFront(no_limit, Action{bindFront(one, makeStaticArray(
  //     Rule{{{{'W'}},{{'B'}},{{'B'}}}, {'P','E','R'}}
  //   ))})},
  //   Node{bindFront(limit, 10, Action{bindFront(one, makeStaticArray(
  //     Rule{{{{'R'}},{{'B'}},{{'B'}}}, {'E','E','R'}}
  //   ))})},
  //   Node{bindFront(markov, makeStaticArray(
  //     Node{bindFront(no_limit, Action{bindFront(one, makeStaticArray(
  //       Rule{{{{'R'}},{{'B'}},{{'B'}}}, {'G','G','U'}},
  //       Rule{{{{'E'}},{{'E'}},{{'G'}}}, {'G','G','G'}},
  //       Rule{{{{'P'}},{{'E'}},{{'G'}}}, {'B','B','P'}},
  //     ))})},
  //     Node{bindFront(no_limit, Action{bindFront(all, makeStaticArray(
  //       Rule{{{{'G'}}}, {'E'}},
  //       Rule{{{{'U'}}}, {'R'}},
  //     ))})},
  //     Node{bindFront(no_limit, Action{bindFront(all, makeStaticArray(
  //       Rule{{{{'R'}}}, {'P'}},
  //       Rule{{{{'P'}}}, {'R'}},
  //     ))})},
  //   ))},
  // ))};
  const auto seq_snake = Sequence{{
    NoLimit{One{{
      {{{{'W'}},{{'B'}},{{'B'}}}, {'P','E','R'}}
    }}},
    Limit{10, One{{
      {{{{'R'}},{{'B'}},{{'B'}}}, {'E','E','R'}}
    }}},
    Markov{{
      NoLimit{One{{
        {{{{'R'}},{{'B'}},{{'B'}}}, {'G','G','U'}},
        {{{{'E'}},{{'E'}},{{'G'}}}, {'G','G','G'}},
        {{{{'P'}},{{'E'}},{{'G'}}}, {'B','B','P'}},
      }}},
      NoLimit{All{{
        {{{{'G'}}}, {'E'}},
        {{{{'U'}}}, {'R'}},
      }}},
      // NoLimit{All{{
      //   {{{{'R'}}}, {'P'}},
      //   {{{{'P'}}}, {'R'}},
      // }}},
    }},
  }};

  auto grid = Grid{'B','B','B','B','B','B','W','B','B','B','B','B','B'};
  for (auto current_grid : seq_snake(grid)) {
    std::println("{}", current_grid);
  }

  return 0;
}