#include <stormkit/main/main_macro.hpp>
#include <unistd.h>

import std;
import stormkit.log;
import consoleapp;
import windowapp;

using namespace stormkit;

inline constexpr auto run_consoleapp(std::span<const std::string_view> args) noexcept -> int {
  return ConsoleApp{}.run(args);
}

inline constexpr auto run_windowapp(std::span<const std::string_view> args) noexcept -> int {
  auto _ = log::Logger::create_logger_instance<log::ConsoleLogger>();
  return WindowApp{}.run(args);
}

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when xmake target properly handles workdir on macos
  chdir("/Users/mtrimolet/Desktop/mtrimolet/hroza");

  auto&& tui = std::ranges::find(args, "--tui") != std::ranges::end(args);
  
  if (tui) return run_consoleapp(args);
  else     return run_windowapp(args);
}
