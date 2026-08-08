#ifndef COMPONENTS_FILES_HASH_H
#define COMPONENTS_FILES_HASH_H

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace Files
{
    std::array<std::uint64_t, 2> getHash(std::string_view fileName, std::istream& stream);

    // Returns the lowercase hexadecimal SHA-256 digest of the stream while
    // preserving its position and exception mask. This is deliberately kept
    // beside getHash so content baselines and saved-game identities use the
    // same streaming file boundary.
    std::string getSha256(std::string_view fileName, std::istream& stream);
}

#endif
