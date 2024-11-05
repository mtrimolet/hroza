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

  const auto palette =
      palette_doc.child("colors").children("color")
      | std::views::transform([](const auto &c) {
          const auto symbol_str =
              std::string{c.attribute("symbol").as_string()};
          ensures(!std::ranges::empty(symbol_str),
                  std::format("missing '{}' attribute [:{}]", "symbol",
                              c.offset_debug()));
          const auto value =
              fromBase<UInt32>(c.attribute("value").as_string(), 16);
          return std::make_pair(symbol_str[0], (255u << 24u) + value);
        })
      | std::ranges::to<Palette>();

  auto models_doc = pugi::xml_document{};
  load_result = models_doc.load_file("models.xml");
  ensures(load_result, load_result.description());

  const auto xmodels = models_doc.child("models");
  ensures(xmodels, std::format("models not found [:{}]", xmodels.offset_debug()));

  const auto models =
      xmodels | std::views::filter([args{args.subspan(1)}](const auto &m) {
        return std::ranges::contains(args, m.attribute("name").as_string());
      })
      | std::views::transform(bindBack<parseModel>(palette))
      | std::views::filter([](const auto &e) { return static_cast<bool>(e); })
      | std::ranges::to<std::vector>();

  auto random = std::random_device{};
  std::ranges::for_each(models, [&random](const auto &e) {
    const auto &model = e.value();
    std::println("[[{}]]", model.name);

    const auto interpreter = Interpreter{model};

    std::ranges::for_each(std::views::iota(0, model.amount), [&](auto k) {
      const auto seed =
          // !std::empty(model.seeds) && k < std::ranges::size(model.seeds)
          //     ? model.seeds.at(k) :
          random();

      const auto runtime = interpreter.run(seed, model.steps);

      const auto frames =
          runtime | std::views::filter([&model](const auto &f) {
            return model.gif || f.counter == 0;
          })
          | std::views::transform([&model, &seed](const auto &f) {
              const auto [grid, counter] = f;
              const auto colors = grid.characters
                  | std::views::transform([&model](auto ch) {
                                    return model.palette.at(ch);
                                  })
                  | std::ranges::to<std::vector>();
              const auto outputname = model.gif
                  ? std::format("output/{}", model.steps - counter)
                  : std::format("output/{}_{}", model.name, seed);

              return std::make_tuple(grid, std::move(colors),
                                     std::move(outputname));
            })
          | std::ranges::to<std::vector>();
      
      std::println("frames: {}", std::ranges::size(frames));

      std::ranges::for_each(frames, [&model](const auto &f) {
        const auto [grid, colors, outputname] = f;
        // if (grid.MZ == 1 || model.iso) {
        const auto [bitmap, WIDTH, HEIGHT] =
            render(grid.states, grid.MX, grid.MY, grid.MZ, colors,
                   model.pixelsize, model.gui);
        std::println("{}[{}x{}]: {}", outputname, WIDTH, HEIGHT, bitmap);
        //   if (model.gui > 0)
        //     GUI.Draw(model.name, interpreter.root, interpreter.current,
        //              bitmap, WIDTH, HEIGHT, model.palette);
        //   Graphics.SaveBitmap(bitmap, WIDTH, HEIGHT,
        //   std::format("{}.png", outputname);
        // } else
        //   VoxHelper.SaveVox(grid.states, grid.MX, grid.MY, grid.MZ,
        //   colors, std::format("{}.vox", outputname);
        // }
      });
    });
  });

  // std::println("time = {}", sw.ElapsedMilliseconds);

  return 0;
}