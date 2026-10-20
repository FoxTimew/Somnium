// Tests time accounting without wall-clock sleeps or SDL devices.
// Conservation checks detect both duplicated ticks and silently lost fractional time.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <somnium/core/conventions.hpp>
#include <somnium/runtime/fixed_step.hpp>

#include <cmath>
#include <limits>

TEST_CASE("Fractional time persists across frames without a fixed update") {
    somnium::runtime::FixedStep loop;
    auto zero = loop.advance(0.0);
    REQUIRE(zero);
    CHECK(zero->steps == 0);
    auto half = loop.advance(somnium::core::kFixedStepSeconds / 2.0);
    REQUIRE(half);
    CHECK(half->steps == 0);
    CHECK(half->alpha == doctest::Approx(0.5));
    auto full = loop.advance(somnium::core::kFixedStepSeconds / 2.0);
    REQUIRE(full);
    CHECK(full->steps == 1);
    CHECK(full->alpha == doctest::Approx(0.0));
}

TEST_CASE("Long pauses cap work, report dropped time and preserve the remainder") {
    somnium::runtime::FixedStep loop;
    REQUIRE(loop.advance(somnium::core::kFixedStepSeconds / 2.0));
    auto pause = loop.advance(10.0);
    REQUIRE(pause);
    CHECK(pause->steps == somnium::core::kMaxFixedSteps);
    CHECK(pause->alpha == doctest::Approx(0.5));
    CHECK(pause->droppedSeconds == doctest::Approx(10.0 - 8.0 / 60.0));
    loop.reset();
    auto reset = loop.advance(0.0);
    REQUIRE(reset);
    CHECK(reset->alpha == 0.0);
}

TEST_CASE("A variable frame sequence conserves time and never leaves alpha outside its interval") {
    somnium::runtime::FixedStep loop;
    double elapsed = 0.0;
    double dropped = 0.0;
    std::uint64_t steps = 0;
    double alpha = 0.0;
    for (int frame = 0; frame < 10000; ++frame) {
        const auto delta = static_cast<double>((frame * 37) % 400) / 1000.0;
        auto batch = loop.advance(delta);
        REQUIRE(batch);
        CHECK(batch->steps <= somnium::core::kMaxFixedSteps);
        CHECK(batch->alpha >= 0.0);
        CHECK(batch->alpha < 1.0);
        elapsed += delta;
        dropped += batch->droppedSeconds;
        steps += batch->steps;
        alpha = batch->alpha;
    }
    CHECK(elapsed ==
          doctest::Approx((static_cast<double>(steps) + alpha) * somnium::core::kFixedStepSeconds +
                          dropped));
}

TEST_CASE("Invalid elapsed time is rejected without corrupting the accumulator") {
    somnium::runtime::FixedStep loop;
    REQUIRE(loop.advance(somnium::core::kFixedStepSeconds / 2.0));
    CHECK_FALSE(loop.advance(-1.0));
    CHECK_FALSE(loop.advance(std::numeric_limits<double>::infinity()));
    CHECK_FALSE(loop.advance(std::numeric_limits<double>::quiet_NaN()));
    auto remaining = loop.advance(0.0);
    REQUIRE(remaining);
    CHECK(remaining->alpha == doctest::Approx(0.5));
}
