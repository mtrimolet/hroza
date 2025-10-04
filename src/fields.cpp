module fields;

import glm;
import stormkit.core;
import geometry;

using namespace stormkit;

auto Field::potential(const Grid<char>& grid, Potential& potential) const noexcept -> void {
  // auto potential = Potential{grid.extents, std::numeric_limits<double>::quiet_NaN()};
  propagate(
    mdiota(potential.area())
      | std::views::filter([this, &grid](auto u) noexcept {
          return zero.contains(grid[u]);
      })
      | std::views::transform([&potential](auto u) noexcept {
          potential[u] = 0.0;
          return std::make_pair(u, 0.0);
      }),
    [this, &grid, &potential](auto&& front) noexcept {
      static constexpr auto neigh_size = 3u * glm::vec<3, u32>{1, 1, 1};
      static constexpr auto neigh_shift = -static_cast<glm::vec<3, i32>>(neigh_size) / 2;
      static constexpr auto neigh = Area3I{neigh_shift, neigh_size};

      auto [u, p] = front;
      auto new_us = (neigh + u).umeet(potential.area());
      auto new_p  = inversed ? p - 1 : p + 1;
      return mdiota(new_us)
        | std::views::filter([this, &grid, &potential](auto n) noexcept {
            return not (potential[n] == 0.0 or std::isnormal(potential[n]))
               and substrate.contains(grid[n]);
        })
        | std::views::transform([&potential, new_p](auto n) noexcept {
            potential[n] = new_p;
            return std::make_pair(n, new_p);
        });
    }
  );

  // return potential;
}
