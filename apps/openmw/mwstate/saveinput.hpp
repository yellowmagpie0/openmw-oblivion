#ifndef OPENMW_MWSTATE_SAVEINPUT_H
#define OPENMW_MWSTATE_SAVEINPUT_H

#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <components/files/openfile.hpp>

namespace MWState
{
    // Own the bytes used by both admission and restoration. Keeping only an
    // open file descriptor does not protect against in-place writes/truncation.
    // Reading/allocation failures occur before any outgoing World is cleared.
    inline std::unique_ptr<std::istream> openSaveSnapshot(const std::filesystem::path& path)
    {
        auto source = Files::openBinaryInputFileStream(path);
        source->seekg(0, std::ios::end);
        const auto size = static_cast<std::streamoff>(source->tellg());
        if (size < 0 || static_cast<std::uintmax_t>(size) > std::string{}.max_size()
            || size > std::numeric_limits<std::streamsize>::max())
            throw std::runtime_error("Saved game input size cannot be represented");
        source->seekg(0, std::ios::beg);
        std::string bytes(static_cast<std::size_t>(size), '\0');
        if (size != 0)
            source->read(bytes.data(), static_cast<std::streamsize>(size));
        if (!*source)
            throw std::runtime_error("Saved game input could not be read completely");
        return std::make_unique<std::istringstream>(std::move(bytes), std::ios::in | std::ios::binary);
    }
}

#endif
