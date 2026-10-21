// Checks the real overlay's timing window independently of a graphics device.
// Startup, suspension and eviction must not produce misleading FPS values.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "frame_times.hpp"
#include <doctest/doctest.h>
#include <limits>

TEST_CASE("overlay uses a bounded mean and rejects suspension samples") {
    somnium::debug_ui::detail::FrameTimes timing;
    CHECK(timing.fps() == 0);
    CHECK(timing.milliseconds() == 0);
    timing.sample(0.01);
    timing.sample(0.03);
    CHECK(timing.fps() == doctest::Approx(50));
    CHECK(timing.milliseconds() == doctest::Approx(20));
    for (double invalid : {0.0, -1.0, 2.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
        timing.sample(invalid);
    CHECK(timing.fps() == doctest::Approx(50));
    for (int index = 0; index < 120; ++index)
        timing.sample(0.005);
    CHECK(timing.fps() == doctest::Approx(200));
    CHECK(timing.milliseconds() == doctest::Approx(5));
}
