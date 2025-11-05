#include <stormkit/main/main_macro.hpp>
#include <unistd.h>

import std;
import stormkit.log;
import tui.consoleapp;
import gui.windowapp;

using namespace stormkit;

inline constexpr auto run_consoleapp(std::span<const std::string_view> args) noexcept -> int {
  auto app = ConsoleApp{};
  return app(args);
}

inline constexpr auto run_windowapp(std::span<const std::string_view> args) noexcept -> int {
  auto app = WindowApp{};
  return app(args);
}

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when xmake target properly handles workdir on macos
  chdir("/Users/mtrimolet/Desktop/mtrimolet/hroza");

  auto&& gui = std::ranges::find(args, "--gui") != std::ranges::end(args);
  
  auto _ = log::Logger::create_logger_instance<log::FileLogger>(".");
  if (gui) return run_windowapp(args);
  else     return run_consoleapp(args);
}
