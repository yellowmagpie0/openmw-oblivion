#include "formidfields.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

#include <components/esm/fourcc.hpp>

namespace ESM4
{
    namespace
    {
        using ESM::fourCC;

        std::uint16_t uint16(std::span<const std::uint8_t> data, std::size_t offset)
        {
            if (offset + 2 > data.size())
                throw std::runtime_error("Truncated TES4 FormID-bearing subrecord");
            return static_cast<std::uint16_t>(data[offset])
                | static_cast<std::uint16_t>(data[offset + 1]) << 8;
        }

        std::uint32_t uint32(std::span<const std::uint8_t> data, std::size_t offset)
        {
            if (offset + 4 > data.size())
                throw std::runtime_error("Truncated TES4 FormID-bearing subrecord");
            return static_cast<std::uint32_t>(data[offset])
                | static_cast<std::uint32_t>(data[offset + 1]) << 8
                | static_cast<std::uint32_t>(data[offset + 2]) << 16
                | static_cast<std::uint32_t>(data[offset + 3]) << 24;
        }

        std::vector<std::size_t> repeated(
            std::span<const std::uint8_t> data, std::size_t stride, std::size_t offset, const char* field)
        {
            if (data.empty())
                return {};
            if (data.size() % stride != 0 || offset + 4 > stride)
                throw std::runtime_error(std::string("Invalid TES4 ") + field + " payload size "
                    + std::to_string(data.size()));
            std::vector<std::size_t> result;
            result.reserve(data.size() / stride);
            for (std::size_t base = 0; base < data.size(); base += stride)
                result.push_back(base + offset);
            return result;
        }

        std::uint8_t conditionParameterMask(std::uint16_t function)
        {
            // Generated from xEdit's TES4 wbConditionFunctions definitions.
            // Bit 0 is parameter 1 and bit 1 is parameter 2.
            static constexpr std::pair<std::uint16_t, std::uint8_t> entries[]{
                { 1, 1 }, { 27, 1 }, { 32, 1 }, { 42, 1 }, { 43, 1 },
                { 44, 1 }, { 45, 1 }, { 47, 1 }, { 53, 1 }, { 56, 1 }, { 58, 1 }, { 59, 1 }, { 60, 3 },
                { 66, 1 }, { 67, 1 }, { 68, 1 }, { 69, 1 }, { 71, 1 }, { 72, 1 }, { 73, 1 }, { 74, 1 },
                { 76, 1 }, { 79, 1 }, { 84, 1 }, { 99, 1 }, { 122, 1 }, { 129, 1 }, { 130, 1 }, { 132, 1 },
                { 136, 1 }, { 149, 1 }, { 161, 1 }, { 162, 1 }, { 163, 1 }, { 172, 1 }, { 180, 1 },
                { 182, 1 }, { 193, 1 }, { 195, 1 }, { 197, 1 }, { 199, 1 }, { 201, 1 }, { 214, 1 },
                { 223, 1 }, { 224, 1 }, { 228, 1 }, { 230, 3 }, { 246, 1 }, { 278, 1 }, { 280, 3 },
                { 288, 1 }, { 1107, 1 }, { 1122, 1 }, { 1124, 2 }, { 2213, 1 }, { 2214, 1 }, { 2215, 1 },
                { 2216, 1 }, { 2217, 1 }, { 2218, 1 }, { 2219, 1 }, { 2220, 1 }, { 2221, 1 }, { 2222, 1 },
                { 2223, 1 }, { 2224, 1 }, { 2225, 1 }, { 2226, 1 }, { 2227, 1 }, { 2228, 1 }, { 2229, 1 },
                { 2230, 1 }, { 2231, 1 }, { 2232, 1 }, { 2233, 1 },
            };
            const auto it = std::lower_bound(std::begin(entries), std::end(entries), function,
                [](const auto& entry, std::uint16_t value) { return entry.first < value; });
            return it != std::end(entries) && it->first == function ? it->second : 0;
        }

        std::vector<std::size_t> conditionOffsets(std::span<const std::uint8_t> data)
        {
            if (data.size() != 20 && data.size() != 24 && data.size() != 28 && data.size() != 32
                && data.size() != 36)
                throw std::runtime_error("Invalid TES4 condition payload size");
            std::vector<std::size_t> result;
            if ((data[0] & 4) != 0)
                result.push_back(4); // comparison value is a GLOB FormID
            const std::uint8_t mask = conditionParameterMask(uint16(data, 8));
            if ((mask & 1) != 0)
                result.push_back(12);
            if ((mask & 2) != 0)
                result.push_back(16);
            if (data.size() >= 28 && uint32(data, 24) != 0)
                result.push_back(24); // later-format run-on reference
            return result;
        }

        bool isOneOf(std::uint32_t value, std::initializer_list<std::uint32_t> values)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }
    }

    bool mayContainFormIds(std::uint32_t recordType, std::uint32_t subRecordType)
    {
        if (subRecordType == fourCC("CTDA") || subRecordType == fourCC("CTDT"))
            return true;
        if (recordType == fourCC("BSGN"))
            return subRecordType == fourCC("SPLO");
        if (recordType == fourCC("CLMT"))
            return subRecordType == fourCC("WLST");
        if (recordType == fourCC("ENCH") || recordType == fourCC("SPEL"))
            return subRecordType == fourCC("SCIT");
        if (recordType == fourCC("FACT"))
            return subRecordType == fourCC("XNAM");
        if (recordType == fourCC("LSCR"))
            return subRecordType == fourCC("LNAM");
        if (recordType == fourCC("LVSP"))
            return subRecordType == fourCC("LVLO");
        if (recordType == fourCC("MGEF"))
            return subRecordType == fourCC("DATA");
        if (recordType == fourCC("WATR"))
            return subRecordType == fourCC("SNAM") || subRecordType == fourCC("GNAM");
        if (recordType == fourCC("WTHR"))
            return subRecordType == fourCC("SNAM");
        if (recordType == fourCC("ACHR"))
            return isOneOf(subRecordType, { fourCC("XHRS"), fourCC("XMRC"), fourCC("XPCI") });
        if (recordType == fourCC("INFO"))
            return isOneOf(subRecordType,
                { fourCC("TPIC"), fourCC("PNAM"), fourCC("NAME"), fourCC("TCLT"), fourCC("TCLF") });
        if (recordType == fourCC("QUST"))
            return subRecordType == fourCC("QSTA");
        if (recordType == fourCC("REFR"))
            return subRecordType == fourCC("XPCI");
        if (recordType == fourCC("REGN"))
            return isOneOf(subRecordType, { fourCC("RDOT"), fourCC("RDGS"), fourCC("RDWT") });
        return false;
    }

    std::vector<std::size_t> findFormIdOffsets(
        std::uint32_t recordType, std::uint32_t subRecordType, std::span<const std::uint8_t> data)
    {
        if (subRecordType == fourCC("CTDA") || subRecordType == fourCC("CTDT"))
            return conditionOffsets(data);

        if ((recordType == fourCC("BSGN") && subRecordType == fourCC("SPLO"))
            || ((recordType == fourCC("ENCH") || recordType == fourCC("SPEL"))
                && subRecordType == fourCC("SCIT"))
            || (recordType == fourCC("FACT") && subRecordType == fourCC("XNAM"))
            || (recordType == fourCC("WATR") && subRecordType == fourCC("SNAM"))
            || (recordType == fourCC("QUST") && subRecordType == fourCC("QSTA"))
            || (recordType == fourCC("REFR") && subRecordType == fourCC("XPCI"))
            || (recordType == fourCC("ACHR")
                && isOneOf(subRecordType, { fourCC("XHRS"), fourCC("XMRC"), fourCC("XPCI") }))
            || (recordType == fourCC("INFO")
                && isOneOf(subRecordType,
                    { fourCC("TPIC"), fourCC("PNAM"), fourCC("NAME"), fourCC("TCLT"), fourCC("TCLF") })))
        {
            if (data.empty())
                return {};
            if (data.size() < 4)
                throw std::runtime_error("Truncated TES4 FormID subrecord " + std::to_string(recordType) + '/'
                    + std::to_string(subRecordType) + " size " + std::to_string(data.size()));
            return { 0 };
        }

        if (recordType == fourCC("CLMT") && subRecordType == fourCC("WLST"))
            return repeated(data, 8, 0, "CLMT/WLST");
        if (recordType == fourCC("LVSP") && subRecordType == fourCC("LVLO"))
            return repeated(data, 12, 4, "LVSP/LVLO");
        if (recordType == fourCC("WTHR") && subRecordType == fourCC("SNAM"))
            return repeated(data, 8, 0, "WTHR/SNAM");
        if (recordType == fourCC("REGN") && subRecordType == fourCC("RDGS"))
            return repeated(data, 8, 0, "REGN/RDGS");
        if (recordType == fourCC("REGN") && subRecordType == fourCC("RDWT"))
            return repeated(data, 8, 0, "REGN/RDWT");
        if (recordType == fourCC("REGN") && subRecordType == fourCC("RDOT"))
        {
            if (data.empty())
                return {};
            // Some released records use an abbreviated single-object layout;
            // all revisions keep the object FormID in the first four bytes.
            if (data.size() % 52 != 0)
            {
                if (data.size() < 4)
                    throw std::runtime_error("Invalid TES4 REGN/RDOT payload size "
                        + std::to_string(data.size()));
                return { 0 };
            }
            return repeated(data, 52, 0, "REGN/RDOT");
        }

        if (recordType == fourCC("LSCR") && subRecordType == fourCC("LNAM"))
        {
            const std::vector<std::size_t> direct = repeated(data, 12, 0, "LSCR/LNAM");
            std::vector<std::size_t> result;
            result.reserve(direct.size() * 2);
            for (const std::size_t offset : direct)
            {
                result.push_back(offset);
                result.push_back(offset + 4);
            }
            return result;
        }
        if (recordType == fourCC("WATR") && subRecordType == fourCC("GNAM"))
        {
            const std::vector<std::size_t> first = repeated(data, 12, 0, "WATR/GNAM");
            std::vector<std::size_t> result;
            result.reserve(first.size() * 3);
            for (const std::size_t offset : first)
                result.insert(result.end(), { offset, offset + 4, offset + 8 });
            return result;
        }
        if (recordType == fourCC("MGEF") && subRecordType == fourCC("DATA"))
        {
            // TES4 writes abbreviated 24-byte built-in effects as well as
            // progressively extended 28-68 byte revisions. Missing tail
            // fields have no implied references.
            if (data.size() < 24 || data.size() > 68 || data.size() % 4 != 0)
                throw std::runtime_error(
                    "Invalid TES4 MGEF/DATA payload size " + std::to_string(data.size()));
            std::vector<std::size_t> result;
            const std::uint32_t flags = uint32(data, 0);
            if ((flags & ((1u << 16) | (1u << 17) | (1u << 18))) != 0)
                result.push_back(8);
            for (const std::size_t offset : { 24u, 32u, 36u, 40u, 44u, 48u, 52u })
                if (offset + 4 <= data.size())
                    result.push_back(offset);
            return result;
        }
        return {};
    }
}
