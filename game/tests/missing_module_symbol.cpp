// Provides a loadable image without the required query entry point.
// Repeated rejection also proves the retained-image budget without copying large symbols.
#include "../src/module_export.hpp"
extern "C" SOMNIUM_GAME_EXPORT int somniumUnrelatedQuery() {
    return 7;
}
