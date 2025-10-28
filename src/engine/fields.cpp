module engine.fields;

import glm;
import stormkit.core;
import geometry;

using namespace stormkit;

auto Field::potential(const Grid<char>& grid, Potential& potential) const noexcept -> void {
  propagate(
    mdiota(potential.area())
      | std::views::filter([this, &grid](auto u) noexcept {
          return zero.contains(grid[u]);
      })
      | std::views::transform([&potential](auto u) noexcept {
          potential[u] = 0.0;
          return std::tuple{ u, 0.0 };
      }),
    [this, &grid, &potential](auto&& front) noexcept {
      static constexpr auto neigh_size = 3u * glm::vec<3, u32>{1, 1, 1};
      static constexpr auto neigh_shift = -static_cast<glm::vec<3, i32>>(neigh_size) / 2;
      static constexpr auto neigh = Area3I{neigh_shift, neigh_size};

      auto [u, p] = front;
      auto new_us = (neigh + u).umeet(potential.area());
      auto new_p  = inversed ? p - 1.0 : p + 1.0;
      return mdiota(new_us)
        | std::views::filter([this, &grid, &potential](auto n) noexcept {
            return not (potential[n] == 0.0 or std::isnormal(potential[n]))
               and substrate.contains(grid[n]);
        })
        | std::views::transform([&potential, new_p](auto n) noexcept {
            potential[n] = new_p;
            return std::tuple{ n, new_p };
        });
    }
  );
}

auto Field::potentials(const Fields& fields, const Grid<char>& grid, Potentials& potentials) noexcept -> void {
  for (auto& [c, f] : fields) {
    if (potentials.contains(c) and not f.recompute) {
      continue;
    }

    potentials.insert_or_assign(c, Potential{ grid.extents, std::numeric_limits<double>::quiet_NaN() });

    f.potential(grid, potentials.at(c));

    if (std::none_of(
      // std::execution::par,
      std::ranges::begin(potentials.at(c)),
      std::ranges::end(potentials.at(c)),
      is_normal
    )) {
      potentials.erase(potentials.find(c));
      break;
    }
  }
}
