module controls;

using namespace std::chrono_literals;

void Controls::play_pause() {
  {
    auto l = std::lock_guard{ pause_m };
    model_paused ^= true;
  }
  pause_cv.notify_one();
}

void Controls::reset() {
  {
    auto l = std::lock_guard{ pause_m };
    model_paused = true;
  }
  pause_cv.notify_one();

  onReset();
}

void Controls::maybe_pause() {
  auto l = std::unique_lock{ pause_m };
  pause_cv.wait(l, [&paused = model_paused]{ return not paused; });
}

void Controls::sleep_missing(Controls::clock::time_point last_time) {
  if (tickrate_enabled and tickrate != 0) {
    const auto tickperiod = std::chrono::duration_cast<clock::duration>( 1000ms / tickrate );
    const auto elapsed = clock::now() - last_time;
    const auto missing = tickperiod - std::min(elapsed, tickperiod);
    std::this_thread::sleep_for(missing);
  }
}
