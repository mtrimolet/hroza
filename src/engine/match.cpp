module engine.match;

import log;

auto Match::conflict(const Match& other) const noexcept -> bool {
  auto overlap = mdiota(area().meet(other.area()))
    | std::views::transform([&a = *this, &b = other](auto u) noexcept {
        return std::tie(
          a.rules[a.r].output.at(u - a.u),
          b.rules[b.r].output.at(u - b.u)
        );
      });
  return std::any_of(
    // std::execution::par,
    std::ranges::begin(overlap),
    std::ranges::end(overlap),
    [](const auto& p) static noexcept {
      return std::get<0>(p)
         and std::get<1>(p);
    }
  );
}

auto Match::match(const Grid<char>& grid) const noexcept -> bool {
  auto zone = std::views::zip(mdiota(area()), rules[r].input);
  return std::all_of(
    // std::execution::par,
    std::ranges::begin(zone),
    std::ranges::end(zone),
    [&grid](const auto& input) noexcept {
      auto [u, i] = input;
      return not i
          or i->contains(grid[u]);
    }
  );
}

auto Match::backward_match(const Potentials& potentials, double p) const noexcept -> bool {
  auto zone = std::views::zip(mdiota(area()), rules[r].output)
    | std::views::filter([](const auto& output) static noexcept {
        return std::get<1>(output) != std::nullopt;
    })
    | std::views::transform([&potentials](const auto& output) noexcept {
        auto [u, o] = output;
        return potentials.contains(*o) ? potentials.at(*o)[u]
               : std::numeric_limits<double>::quiet_NaN();
    });
  return std::all_of(
    // std::execution::par,
    std::ranges::begin(zone),
    std::ranges::end(zone),
    [p](auto current) noexcept {
      return is_normal(current)
         and current <= p;
    }
  );
}

auto Match::forward_match(const Potentials& potentials, double p) const noexcept -> bool {
  auto zone = std::views::zip(mdiota(area()), rules[r].input)
    | std::views::filter([](const auto& input) static noexcept {
        return std::get<1>(input) != std::nullopt;
    })
    | std::views::transform([&potentials](const auto& input) noexcept {
        auto [u, i] = input;
        auto im = std::ranges::max(*i, {}, [&potentials, u] (auto i) {
          return potentials.contains(i) ? potentials.at(i)[u]
            : std::numeric_limits<double>::quiet_NaN();
        });
        return potentials.contains(im) ? potentials.at(im)[u]
          : std::numeric_limits<double>::quiet_NaN();
    });
  return std::all_of(
    // std::execution::par,
    std::ranges::begin(zone),
    std::ranges::end(zone),
    [p](auto current) noexcept {
      return is_normal(current)
         and current <= p;
    }
  );
}

auto Match::changes(const Grid<char>& grid) const noexcept -> std::vector<Change<char>> {
  return std::views::zip(mdiota(area()), rules[r].output)
    | std::views::filter([&grid](const auto& output) noexcept {
        auto [u, o] = output;
        return  o
           and *o != grid[u];
    })
    | std::views::transform([](auto&& output) static noexcept {
        return Change{std::get<0>(output), *std::get<1>(output)};
    })
    | std::ranges::to<std::vector>();
}

auto Match::backward_changes(const Potentials& potentials, double p) const noexcept -> std::vector<Change<std::tuple<char, double>>> {
  return std::views::zip(mdiota(area()), rules[r].input)
    | std::views::filter([&potentials](const auto& input) noexcept {
        auto [u, i] = input;
        if (not i) return false;
        auto im = std::ranges::max(*i, {}, [&potentials, u] (auto i) {
          return potentials.contains(i) ? potentials.at(i)[u]
            : std::numeric_limits<double>::quiet_NaN();
        });
        return is_normal(potentials.at(im)[u]);
    })
    | std::views::transform([&potentials, p](auto&& input) noexcept {
        auto im = std::ranges::max(*std::get<1>(input), {}, [&potentials, u = std::get<0>(input)] (auto i) {
          return potentials.contains(i) ? potentials.at(i)[u]
            : std::numeric_limits<double>::quiet_NaN();
        });
        return Change{ std::get<0>(input), std::tuple{ im, p }};
    })
    | std::ranges::to<std::vector>();
}

auto Match::forward_changes(const Potentials& potentials, double p) const noexcept -> std::vector<Change<std::tuple<char, double>>> {
  return std::views::zip(mdiota(area()), rules[r].output)
    | std::views::filter([&potentials](const auto& output) noexcept {
        auto [u, o] = output;
        return o
           and is_normal(potentials.at(*o)[u]);
    })
    | std::views::transform([p](auto&& output) noexcept {
        return Change{ std::get<0>(output), std::tuple{ *std::get<1>(output), p }};
    })
    | std::ranges::to<std::vector>();
}

auto Match::delta(const Grid<char>& grid, const Potentials& potentials) const noexcept -> double {
  auto vals = std::views::zip(mdiota(area()), rules[r].output)
    | std::views::filter([&grid](auto&& _o) noexcept {
        auto [u, o] = _o;
        return  o
           and *o != grid[u];
    })
    | std::views::transform([&grid, &potentials] (const auto& _o) noexcept {
        auto [u, o] = _o;

        auto new_value = *o;
        auto old_value = grid[u];

        auto new_p = potentials.contains(new_value) ? potentials.at(new_value)[u] : 0.0;
        auto old_p = potentials.contains(old_value) ? potentials.at(old_value)[u] : 0.0;

        if (not is_normal(old_p))
          old_p = -1.0;

        return new_p - old_p;
    });
  // ilog("delta vals: {}", vals | std::ranges::to<std::vector>());
  return std::reduce(
    // std::execution::par,
    std::ranges::begin(vals),
    std::ranges::end(vals)
  );
}
