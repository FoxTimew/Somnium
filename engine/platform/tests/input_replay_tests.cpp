// Tests the replay source without initializing video or issuing OS input.
// Recorded edges retain order when render frames consume several simulation steps.
#include <doctest/doctest.h>
#include <somnium/platform/input_replay.hpp>
TEST_CASE("Replay delivers each recorded event at exactly its tick") {
    auto replay = somnium::platform::InputReplay::parse(R"({"version":1,"events":[
        {"step":2,"kind":"key","control":"F24","down":true},
        {"step":2,"kind":"key","control":"F24","down":false},
        {"step":3,"kind":"mouseMotion","x":4,"y":-5}]})");
    REQUIRE(replay);
    REQUIRE(replay->next(0));
    CHECK_FALSE(replay->next(2));
    REQUIRE(replay->next(1));
    auto events = replay->next(2);
    REQUIRE(events);
    REQUIRE(events->size() == 2);
    CHECK((*events)[0].down);
    CHECK_FALSE((*events)[1].down);
    CHECK((*events)[0].key == somnium::core::Key::F24);
    events = replay->next(3);
    REQUIRE(events);
    REQUIRE(events->size() == 1);
    CHECK((*events)[0].y == -5);
    events = replay->next(4);
    REQUIRE(events);
    CHECK(events->empty());
    CHECK_FALSE(replay->next(4));
}
TEST_CASE("Replay rejects unknown controls, fields, raw ranges and step order") {
    for (
        auto text :
        {R"({"version":2,"events":[]})",
         R"({"version":1,"events":[{"step":0,"kind":"key","control":"Unknown","down":true}]})",
         R"({"version":1,"events":[{"step":1,"kind":"focusLost"},{"step":0,"kind":"focusLost"}]})",
         R"({"version":1,"events":[{"step":0,"kind":"gamepadAxis","device":1,"control":"LeftX","value":32768}]})",
         R"({"version":1,"events":[{"step":0,"kind":"mouseMotion","x":1,"y":2,"extra":3}]})"})
        CHECK_FALSE(somnium::platform::InputReplay::parse(text));
}
