#include <stormkit/Main/MainMacro.hpp>
#include <unistd.h>

import std;
import pugixml;

import model;
import interpreter;
import utils;

import stormkit.Core;

using namespace std::literals;
using namespace stormkit;

auto main(std::span<const std::string_view> args) noexcept -> int {
  // [tmpfix] remove when stormkit userMain allows for non-packaged build
  chdir("/Users/mtrimolet/Desktop/mtrimolet/markovjunior/hroza");
  
  // load color palette
  auto palette_doc = pugi::xml_document{};
  auto load_result = palette_doc.load_file("resources/palette.xml");
  ensures(load_result, load_result.description());

  const auto palette =
      palette_doc.child("colors").children("color") |
      std::views::transform([](const auto &c) {
        const auto symbol_str = std::string{c.attribute("symbol").as_string()};
        ensures(!std::ranges::empty(symbol_str));
        return std::make_pair(
            symbol_str[0],
            (255u << 24u) +
                fromBase<UInt32>(c.attribute("value").as_string(), 16));
      }) |
      std::ranges::to<Palette>();
  std::println("palette: {}", palette);

  // load models registry
  auto models_doc = pugi::xml_document{};
  load_result = models_doc.load_file("models.xml");
  ensures(load_result, load_result.description());

  const auto xmodels = models_doc.child("models");
  ensures(xmodels, "models not found");

  // load and run models
  std::ranges::for_each(
      xmodels | std::views::filter([_args = args.subspan(1)](const auto &m) {
        return std::ranges::contains(_args, m.attribute("name").as_string());
      }) | std::views::transform(bindBack<parseModel>(palette)) |
          std::views::filter(
              [](const auto &e) { return static_cast<bool>(e); }),
      [](const auto &r) {
        const auto &model = r.value();
        std::println("[[{}]]", model.name);

        auto interpreter = Interpreter{model};

        for (auto k = 0; k < model.amount; k++) {
          const auto seed =
              // !std::empty(model.seeds) && k < std::ranges::size(model.seeds)
              //     ? model.seeds.at(k) :
              std::rand();

          // for (auto [result, legend, FX, FY, FZ] : interpreter.Run(seed,
          // model.steps, model.gif)) {
          auto [result, legend, FX, FY, FZ] =
              interpreter.run(seed, model.steps, model.gif);

          const auto colors = legend | std::views::transform([&model](auto ch) {
                                return model.palette.at(ch);
                              }) |
                              std::ranges::to<std::vector>();

          const auto outputname =
              model.gif ? std::format("output/{}", interpreter.counter)
                        : std::format("output/{}_{}", model.name, seed);

          std::println("{} {} {} {}:{}:{} {} {}", seed,
                       std::ranges::size(result), legend, FX, FY, FZ, colors,
                       outputname);

          // if (FZ == 1 || model.iso) {
          //   const auto [bitmap, WIDTH, HEIGHT] = Graphics.Render(
          //       result, FX, FY, FZ, colors, model.pixelsize, model.gui);
          //   if (model.gui > 0)
          //     GUI.Draw(model.name, interpreter.root, interpreter.current,
          //              bitmap, WIDTH, HEIGHT, customPalette);
          //   Graphics.SaveBitmap(bitmap, WIDTH, HEIGHT, outputname + ".png");
          // } else
          //   VoxHelper.SaveVox(result, FX, FY, FZ, colors, outputname +
          //   ".vox");
          // }
          std::println("DONE");
        }
      });

  // std::println("time = {}", sw.ElapsedMilliseconds);
  std::println("finished");

  return 0;
}
