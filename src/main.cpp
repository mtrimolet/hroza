#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
import stormkit.Core;
import pugixml;
import model;
import interpreter;
import graphics;
import utils;

using namespace std::literals;
using namespace stormkit;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");

  // load color palette
  auto palette_doc = pugi::xml_document{};
  auto load_result = palette_doc.load_file("resources/palette.xml");
  ensures(load_result, load_result.description());

  const auto palette = palette_doc.child("colors").children("color")
      | std::views::transform([](const auto &c) {
          const auto symbol_str = std::string{c.attribute("symbol").as_string()};
          ensures(!std::ranges::empty(symbol_str),
                  std::format("missing '{}' attribute [:{}]", "symbol", c.offset_debug()));
          const auto value = fromBase<UInt32>(c.attribute("value").as_string(), 16);
          return std::make_pair(symbol_str[0], (255u << 24u) + value);
        })
      | std::ranges::to<Palette>();

  auto models_doc = pugi::xml_document{};
  load_result = models_doc.load_file("models.xml");
  ensures(load_result, load_result.description());

  const auto xmodels = models_doc.child("models");
  ensures(xmodels, std::format("models not found [:{}]", xmodels.offset_debug()));

  const auto models = xmodels 
      | std::views::filter([args{args.subspan(1)}](const auto &m) {
        return std::ranges::contains(args, m.attribute("name").as_string());
      })
      | std::views::transform(bindBack<Model::parse>(palette))
      | std::views::filter([](const auto &e) {
          if (!e) {
            std::println("parse_result: {}", e.error().description());
            return false;
          }
          return true;
        })
      | std::views::transform([](const auto &e) { return e.value(); })
      | std::ranges::to<std::vector>();

  auto random = std::random_device{};
  std::ranges::for_each(models, [](const auto &model) {
    std::println("{}[{}x{}x{}]:", model.name, model.MX, model.MY, model.MZ);

    auto interpreter = Interpreter{model};

    std::ranges::for_each(std::views::iota(0u, model.amount), [&](auto k) {
      const auto seed = std::rand();

      auto ticks = interpreter.run(seed)
          | std::views::take(model.steps)
          // mandatory to get real size
          | std::ranges::to<std::vector>();

      auto frames = ticks
          | std::views::drop(model.gif ? 0 : std::ranges::size(ticks) - 1);

      std::println("generation: {}, ticks: {}", k, std::ranges::size(ticks));

      std::ranges::for_each(frames, [&model](const auto &grid) {
        // if (grid.MZ == 1 || model.iso) {
        auto [bitmap, s] = render(grid, model);
        //   if (model.gui > 0)
        //     GUI.Draw(model.name, interpreter.root, interpreter.current,
        //              bitmap, width, height, model.palette);
        draw(bitmap, get<0>(s), get<1>(s));
        std::println();
        //   Graphics.SaveBitmap(bitmap, width, height, std::format("{}.png", outputname);
        // } else
        //   VoxHelper.SaveVox(grid, std::format("{}.vox", outputname);
        // }
      });
    });
  });

  // std::println("time = {}", sw.ElapsedMilliseconds);

  return 0;
}