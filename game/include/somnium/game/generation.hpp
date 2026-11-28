// Gives each reloadable build distinct EnTT type identities while codecs stay stable.
// Static consumers use a fixed namespace and exactly the same gameplay sources.
#pragma once
#ifndef SOMNIUM_GAME_GENERATION
#define SOMNIUM_GAME_GENERATION static_game
#endif
namespace somnium::game {
namespace SOMNIUM_GAME_GENERATION {}
// Clang omits inline namespaces from __PRETTY_FUNCTION__, which EnTT hashes.
using namespace SOMNIUM_GAME_GENERATION;
} // namespace somnium::game
