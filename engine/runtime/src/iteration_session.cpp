// Keeps the editing snapshot separate from the disposable simulated World.
// Candidate Worlds acquire assets before the old World releases its requests.
#include "iteration_session.hpp"
namespace somnium::runtime {
IterationSession::~IterationSession() {
    if (m_playing)
        m_game.stop(*m_world);
}
Result<void> IterationSession::play(std::uint64_t tick) {
    if (m_playing)
        return tl::unexpected(Error{ErrorCode::InvalidState, "Already playing"});
    auto saved = scene::snapshot(*m_world, m_codecs);
    if (!saved)
        return tl::unexpected(saved.error());
    auto candidate = scene::restore(*saved, m_codecs, m_assets);
    if (!candidate)
        return tl::unexpected(candidate.error());
    auto started = m_game.start(**candidate, {m_mapping, tick});
    if (!started) {
        m_game.stop(**candidate);
        return started;
    }
    auto valid = (*candidate)->propagate();
    if (!valid) {
        m_game.stop(**candidate);
        return valid;
    }
    m_editing = std::move(*saved);
    m_world = std::move(*candidate);
    m_playing = true;
    return {};
}
Result<void> IterationSession::stop() {
    if (!m_playing)
        return tl::unexpected(Error{ErrorCode::InvalidState, "Already editing"});
    auto candidate = scene::restore(m_editing, m_codecs, m_assets);
    if (!candidate)
        return tl::unexpected(candidate.error());
    m_game.stop(*m_world);
    m_world = std::move(*candidate);
    m_playing = false;
    return {};
}
} // namespace somnium::runtime
