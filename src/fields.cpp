module fields;

import glm;
import stormkit.core;
import geometry;

using namespace stormkit;

auto Field::potential(const Grid<char>& grid) const noexcept -> Potential {
  auto unit = inversed ? -1.0 : +1.0;
  auto potential = Potential{grid.extents, std::numeric_limits<double>::quiet_NaN()};
  auto&& p_area = potential.area();
  propagate(
    mdiota(p_area)
      | std::views::filter([&](auto&& u) noexcept {
          return zero.contains(grid[u]);
      })
      | std::views::transform([&](auto&& u) noexcept {
          potential[u] = 0.0;
          return std::make_pair(u, 0.0);
      }),
    [&](auto&& front) noexcept {
      static constexpr auto neigh_size = 3u * math::Vector3U{1, 1, 1};
      static constexpr auto neigh_shift = -static_cast<math::Vector3I>(neigh_size) / 2;
      static constexpr auto neigh = Area3I{neigh_shift, neigh_size};

      auto&& [u, p] = front;
      auto&& new_us = (neigh + u).umeet(p_area);
      auto&& new_p = p + unit;
      return mdiota(new_us)
        | std::views::filter([&](auto&& n) noexcept {
            return not (potential[n] == 0.0 or std::isnormal(potential[n]))
               and substrate.contains(grid[n]);
        })
        | std::views::transform([&, new_p](auto&& n) noexcept {
            potential[n] = new_p;
            return std::make_pair(n, new_p);
        });
    }
  );

  return potential;
}
