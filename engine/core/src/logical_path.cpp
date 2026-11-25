// Shares path identity rules between the asset cache and platform readers.
// Native paths, NUL and root traversal are rejected before any I/O.
#include <somnium/core/logical_path.hpp>
#include <vector>

namespace somnium::core {
bool validUtf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first < 0x80) {
            ++i;
            continue;
        }
        const unsigned count = first >= 0xc2 && first <= 0xdf   ? 2
                               : first >= 0xe0 && first <= 0xef ? 3
                               : first >= 0xf0 && first <= 0xf4 ? 4
                                                                : 0;
        if (!count || count > text.size() - i)
            return false;
        const auto second = static_cast<unsigned char>(text[i + 1]);
        if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
            (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90))
            return false;
        for (unsigned j = 1; j < count; ++j) {
            const auto value = static_cast<unsigned char>(text[i + j]);
            if (value < 0x80 || value > 0xbf)
                return false;
        }
        i += count;
    }
    return true;
}
Result<std::string> logicalPath(std::string_view path) {
    const auto invalid = [] {
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid logical resource path"});
    };
    if (!validUtf8(path) || path.empty() || path.front() == '/' || path.back() == '/' ||
        path.find_first_of("\\:") != std::string_view::npos ||
        path.find('\0') != std::string_view::npos)
        return invalid();
    std::vector<std::string_view> parts;
    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto part = path.substr(0, slash);
        if (part.empty())
            return invalid();
        if (part == "..") {
            if (parts.empty())
                return invalid();
            parts.pop_back();
        } else if (part != ".") {
            parts.push_back(part);
        }
        if (slash == std::string_view::npos)
            break;
        path.remove_prefix(slash + 1);
    }
    if (parts.empty())
        return invalid();
    std::string result;
    for (const auto part : parts) {
        if (!result.empty())
            result += '/';
        result += part;
    }
    return result;
}
} // namespace somnium::core
