#ifndef ESMLOADER_HPP
#define ESMLOADER_HPP

#include <map>
#include <optional>
#include <vector>

#include <components/esm/gameprofile.hpp>

#include "contentloader.hpp"

namespace ToUTF8
{
    class Utf8Encoder;
}

namespace ESM
{
    class ReadersCache;
    struct Dialogue;
}

namespace MWWorld
{

    class ESMStore;

    struct EsmLoader : public ContentLoader
    {
        explicit EsmLoader(MWWorld::ESMStore& store, ESM::ReadersCache& readers, ToUTF8::Utf8Encoder* encoder,
            std::vector<int>& esmVersions, ESM::GameProfile requestedProfile);

        std::optional<int> getMasterFileFormat() const { return mMasterFileFormat; }
        ESM::GameProfile getGameProfile() const { return mGameProfileSelector.selected(); }

        void load(const std::filesystem::path& filepath, int& index, Loading::Listener* listener) override;

    private:
        ESM::ReadersCache& mReaders;
        MWWorld::ESMStore& mStore;
        ToUTF8::Utf8Encoder* mEncoder;
        ESM::Dialogue* mDialogue;
        std::optional<int> mMasterFileFormat;
        std::vector<int>& mESMVersions;
        std::map<std::string, int> mNameToIndex;
        ESM::GameProfileSelector mGameProfileSelector;
    };

} /* namespace MWWorld */

#endif // ESMLOADER_HPP
