// Violates each structural boundary in a child process so debug assertions are observable.
// Release builds must return an error without changing the traversed World.
#include <cstdlib>
#include <somnium/scene/world.hpp>
#include <string_view>

int main(int argc, char** argv) {
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (argc != 2)
        return 2;
    somnium::scene::World world;
    auto root = world.create("root");
    auto child = world.create("child");
    if (!root || !child)
        return 2;
    const auto traversal = world.traverse();
    const std::string_view operation(argv[1]);
    somnium::Result<void> result;
    if (operation == "create") {
        auto created = world.create("forbidden");
        if (!created)
            result = tl::unexpected(created.error());
    } else if (operation == "destroy")
        result = world.destroy(*child);
    else if (operation == "reparent")
        result = world.reparent(*child, *root);
    else
        return 2;
    return !result && result.error().code == somnium::ErrorCode::InvalidState &&
                   world.size() == 2 &&
                   world.registry().get<somnium::Hierarchy>(*child).parent == entt::null
               ? 0
               : 3;
}
