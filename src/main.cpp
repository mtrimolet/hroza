#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
import stormkit.Core;
import pugixml;
import model;
import interpreter;
import graphics;
import utils;
import stride_view;

using namespace std::literals;
using namespace stormkit;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");

  auto palette_doc = pugi::xml_document{};
  auto load_result = palette_doc.load_file("resources/palette.xml");
  ensures(load_result, load_result.description());

  const auto palette = parsePalette(palette_doc.child("colors").children("color"));

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

  std::ranges::for_each(models, [](const auto &model) {
    std::println("{}[{}x{}x{}]:", model.name, std::get<0>(model.size), std::get<1>(model.size), std::get<2>(model.size));

    if (std::get<0>(model.size) != 1) {
      std::println("cannot render 3d grid in console mode");
      return;
    }

    auto interpreter = Interpreter::parse(model.doc.first_child(), auto{model.size});

    std::ranges::for_each(std::views::iota(0u, model.amount), [&](auto k) {
      std::println("generation: {}", k);

      const auto seed = std::rand();

      auto ticks = interpreter.run(seed)
          | std::views::take(model.steps);
          // mandatory to get real size

      auto frames = std::move(ticks)
          // | std::views::drop(model.gif ? 0 : std::ranges::size(ticks) - 1);
          | std::views::stride(model.gif ? 0 : 10);

      std::ranges::for_each(frames, [&model](const auto &grid) {
        // if (std::get<0>(grid.size) == 1 or model.iso) {
          auto [bitmap, s] = render(grid, model);
          //   if (model.gui > 0)
          //     GUI.Draw(model.name, interpreter.root, interpreter.current,
          //              bitmap, width, height, model.palette);
          draw(bitmap, std::get<2>(s), std::get<1>(s));
          std::println();
          //   Graphics.SaveBitmap(bitmap, width, height, std::format("{}.png", outputname);
        // } else
            // VoxHelper.SaveVox(grid, std::format("{}.vox", outputname));
      });
    });
  });

  // std::println("time = {}", sw.ElapsedMilliseconds);

  return 0;
}