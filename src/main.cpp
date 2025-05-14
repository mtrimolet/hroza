#include <stormkit/main/main_macro.hpp>
#include <unistd.h>

import std;
import consoleapp;

using App = ConsoleApp;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when xmake target properly handles workdir on macos
  chdir("/Users/mtrimolet/Desktop/mtrimolet/hroza");

  auto app = App{};
  return app.run(args);
}
