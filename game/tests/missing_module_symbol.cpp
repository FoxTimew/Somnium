// Provides a loadable image without the required query entry point.
// Repeated rejection also proves the retained-image budget without copying large symbols.
#if defined(_WIN32)
#define SOMNIUM_PROOF_EXPORT __declspec(dllexport)
#else
#define SOMNIUM_PROOF_EXPORT __attribute__((visibility("default")))
#endif
extern "C" SOMNIUM_PROOF_EXPORT int somniumUnrelatedQuery() {
    return 7;
}
