module engine.match;

import log;

namespace stdr = std::ranges;
namespace stdv = std::views;

auto Match::conflict(const Match& other) const noexcept -> bool {
  return stdr::any_of(
    mdiota(area().meet(other.area())),
    [&a = *this, &b = other](auto u) noexcept {
      return a.rules[a.r].output.at(u - a.u)
         and b.rules[b.r].output.at(u - b.u);
    }
  );
}

auto Match::match(const Grid<char>& grid) const noexcept -> bool {
  return stdr::all_of(
    stdv::zip(mdiota(area()), rules[r].input),
    [&grid](const auto& input) noexcept {
      auto [u, i] = input;
      return not i
          or i->contains(grid[u]);
    }
  );
}

auto Match::backward_match(const Potentials& potentials, double p) const noexcept -> bool {
  return stdr::all_of(
    stdv::zip(mdiota(area()), rules[r].output)
      | stdv::filter([](const auto& output) static noexcept {
          return std::get<1>(output) != std::nullopt;
      }),
    [p](auto current) noexcept {
      return is_normal(current)
         and current <= p;
    },
    [&potentials](const auto& output) noexcept {
      auto [u, o] = output;
      return potentials.contains(*o) ? potentials.at(*o)[u]
             : std::numeric_limits<double>::quiet_NaN();
    }
  );
}

auto Match::forward_match(const Potentials& potentials, double p) const noexcept -> bool {
  return stdr::all_of(
    stdv::zip(mdiota(area()), rules[r].input)
      | stdv::filter([](const auto& input) static noexcept {
          return std::get<1>(input) != std::nullopt;
      }),
    [p](auto current) noexcept {
      return is_normal(current)
         and current <= p;
    },
    [&potentials](const auto& input) noexcept {
      auto [u, i] = input;
      auto im = stdr::max(*i, {}, [&potentials, u] (auto i) {
        return potentials.contains(i) ? potentials.at(i)[u]
          : std::numeric_limits<double>::quiet_NaN();
      });
      return potentials.contains(im) ? potentials.at(im)[u]
        : std::numeric_limits<double>::quiet_NaN();
    }
  );
}

auto Match::changes(const Grid<char>& grid) const noexcept -> std::vector<Change<char>> {
  return stdv::zip(mdiota(area()), rules[r].output)
    | stdv::filter([&grid](const auto& output) noexcept {
        auto [u, o] = output;
        return  o
           and *o != grid[u];
    })
    | stdv::transform([](auto&& output) static noexcept {
        return Change{std::get<0>(output), *std::get<1>(output)};
    })
    | stdr::to<std::vector>();
}

auto Match::backward_changes(const Potentials& potentials, double p) const noexcept -> std::vector<Change<std::tuple<char, double>>> {
  return stdv::zip(mdiota(area()), rules[r].input)
    | stdv::filter([&potentials](const auto& input) noexcept {
        auto [u, i] = input;
        if (not i) return false;
        auto im = stdr::max(*i, {}, [&potentials, u] (auto i) {
          return potentials.contains(i) ? potentials.at(i)[u]
            : std::numeric_limits<double>::quiet_NaN();
        });
        return is_normal(potentials.at(im)[u]);
    })
    | stdv::transform([&potentials, p](auto&& input) noexcept {
        auto im = stdr::max(*std::get<1>(input), {}, [&potentials, u = std::get<0>(input)] (auto i) {
          return potentials.contains(i) ? potentials.at(i)[u]
            : std::numeric_limits<double>::quiet_NaN();
        });
        return Change{ std::get<0>(input), std::tuple{ im, p }};
    })
    | stdr::to<std::vector>();
}

auto Match::forward_changes(const Potentials& potentials, double p) const noexcept -> std::vector<Change<std::tuple<char, double>>> {
  return stdv::zip(mdiota(area()), rules[r].output)
    | stdv::filter([&potentials](const auto& output) noexcept {
        auto [u, o] = output;
        return o
           and is_normal(potentials.at(*o)[u]);
    })
    | stdv::transform([p](auto&& output) noexcept {
        return Change{ std::get<0>(output), std::tuple{ *std::get<1>(output), p }};
    })
    | stdr::to<std::vector>();
}

auto Match::delta(const Grid<char>& grid, const Potentials& potentials) const noexcept -> double {
  return stdr::fold_left(
    stdv::zip(mdiota(area()), rules[r].output)
      | stdv::filter([&grid](auto&& _o) noexcept {
          auto [u, o] = _o;
          return  o
             and *o != grid[u];
      })
      | stdv::transform([&grid, &potentials] (const auto& _o) noexcept {
          auto [u, o] = _o;

          auto new_value = *o;
          auto old_value = grid[u];

          auto new_p = potentials.contains(new_value) ? potentials.at(new_value)[u] : 0.0;
          auto old_p = potentials.contains(old_value) ? potentials.at(old_value)[u] : 0.0;

          if (not is_normal(old_p))
            old_p = -1.0;

          return new_p - old_p;
      }),
    0, std::plus{}
  );
}
