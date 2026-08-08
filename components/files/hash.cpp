#include "hash.hpp"

#include <smhasher/MurmurHash3.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Files
{
    namespace
    {
        constexpr std::array<std::uint32_t, 64> sSha256Constants{
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
            0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
            0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
            0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
            0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
            0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
            0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
            0xc67178f2,
        };

        class Sha256
        {
        public:
            void update(const std::uint8_t* data, std::size_t size)
            {
                if (size > (std::numeric_limits<std::uint64_t>::max() - mSize) / 8)
                    throw std::runtime_error("SHA-256 input is too large");
                mSize += static_cast<std::uint64_t>(size) * 8;
                while (size != 0)
                {
                    const std::size_t copied = std::min(size, mBuffer.size() - mBufferSize);
                    std::copy_n(data, copied, mBuffer.data() + mBufferSize);
                    data += copied;
                    size -= copied;
                    mBufferSize += copied;
                    if (mBufferSize == mBuffer.size())
                    {
                        transform(mBuffer.data());
                        mBufferSize = 0;
                    }
                }
            }

            std::array<std::uint8_t, 32> finish()
            {
                const std::uint64_t bitSize = mSize;
                const std::uint8_t marker = 0x80;
                update(&marker, 1);
                const std::uint8_t zero = 0;
                while (mBufferSize != 56)
                    update(&zero, 1);
                std::array<std::uint8_t, 8> length{};
                for (std::size_t i = 0; i < length.size(); ++i)
                    length[length.size() - i - 1] = static_cast<std::uint8_t>(bitSize >> (i * 8));
                update(length.data(), length.size());

                std::array<std::uint8_t, 32> result{};
                for (std::size_t i = 0; i < mState.size(); ++i)
                    for (std::size_t j = 0; j < 4; ++j)
                        result[i * 4 + j] = static_cast<std::uint8_t>(mState[i] >> ((3 - j) * 8));
                return result;
            }

        private:
            void transform(const std::uint8_t* block)
            {
                std::array<std::uint32_t, 64> words{};
                for (std::size_t i = 0; i < 16; ++i)
                    words[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24)
                        | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16)
                        | (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8)
                        | static_cast<std::uint32_t>(block[i * 4 + 3]);
                for (std::size_t i = 16; i < words.size(); ++i)
                {
                    const std::uint32_t s0
                        = std::rotr(words[i - 15], 7) ^ std::rotr(words[i - 15], 18) ^ (words[i - 15] >> 3);
                    const std::uint32_t s1
                        = std::rotr(words[i - 2], 17) ^ std::rotr(words[i - 2], 19) ^ (words[i - 2] >> 10);
                    words[i] = words[i - 16] + s0 + words[i - 7] + s1;
                }

                auto [a, b, c, d, e, f, g, h] = mState;
                for (std::size_t i = 0; i < words.size(); ++i)
                {
                    const std::uint32_t sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
                    const std::uint32_t choice = (e & f) ^ (~e & g);
                    const std::uint32_t temp1 = h + sum1 + choice + sSha256Constants[i] + words[i];
                    const std::uint32_t sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
                    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                    const std::uint32_t temp2 = sum0 + majority;
                    h = g;
                    g = f;
                    f = e;
                    e = d + temp1;
                    d = c;
                    c = b;
                    b = a;
                    a = temp1 + temp2;
                }
                mState[0] += a;
                mState[1] += b;
                mState[2] += c;
                mState[3] += d;
                mState[4] += e;
                mState[5] += f;
                mState[6] += g;
                mState[7] += h;
            }

            std::array<std::uint32_t, 8> mState{ 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
            std::array<std::uint8_t, 64> mBuffer{};
            std::size_t mBufferSize = 0;
            std::uint64_t mSize = 0;
        };
    }

    std::array<std::uint64_t, 2> getHash(std::string_view fileName, std::istream& stream)
    {
        std::array<std::uint64_t, 2> hash{ 0, 0 };
        try
        {
            const auto start = stream.tellg();
            const auto exceptions = stream.exceptions();
            stream.exceptions(std::ios_base::badbit);
            while (stream)
            {
                std::array<char, 4096> value;
                stream.read(value.data(), value.size());
                const std::streamsize read = stream.gcount();
                if (read == 0)
                    break;
                std::array<std::uint64_t, 2> blockHash{ 0, 0 };
                MurmurHash3_x64_128(value.data(), static_cast<int>(read), hash.data(), blockHash.data());
                hash = blockHash;
            }
            stream.clear();
            stream.exceptions(exceptions);
            stream.seekg(start);
        }
        catch (const std::exception& e)
        {
            std::string message = "Error while reading \"";
            message += fileName;
            message += "\" to get hash: ";
            message += e.what();
            throw std::runtime_error(message);
        }
        return hash;
    }

    std::string getSha256(std::string_view fileName, std::istream& stream)
    {
        try
        {
            const auto start = stream.tellg();
            const auto exceptions = stream.exceptions();
            stream.exceptions(std::ios_base::badbit);
            Sha256 hash;
            while (stream)
            {
                std::array<std::uint8_t, 64 * 1024> value{};
                stream.read(reinterpret_cast<char*>(value.data()), value.size());
                const std::streamsize read = stream.gcount();
                if (read == 0)
                    break;
                hash.update(value.data(), static_cast<std::size_t>(read));
            }
            const auto digest = hash.finish();
            stream.clear();
            stream.exceptions(exceptions);
            stream.seekg(start);

            std::ostringstream result;
            result << std::hex << std::setfill('0');
            for (const std::uint8_t value : digest)
                result << std::setw(2) << static_cast<unsigned>(value);
            return result.str();
        }
        catch (const std::exception& e)
        {
            std::string message = "Error while reading \"";
            message += fileName;
            message += "\" to get SHA-256: ";
            message += e.what();
            throw std::runtime_error(message);
        }
    }
}
