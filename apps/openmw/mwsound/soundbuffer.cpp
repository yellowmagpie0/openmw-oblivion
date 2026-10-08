#include "soundbuffer.hpp"
#include "nativeaudioutils.hpp"

#include "../mwbase/environment.hpp"
#include "../mwworld/esmstore.hpp"

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadsoun.hpp>
#include <components/esm4/loadsndr.hpp>
#include <components/esm4/loadsoun.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/rng.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace MWSound
{
    const ESM4::Sound* resolveNativeAnimationSound(const MWWorld::ESMStore& store, std::string_view editorId)
    {
        if (editorId.empty() || editorId.find('\0') != std::string_view::npos)
            return nullptr;
        const ESM4::Sound* result = nullptr;
        std::set<ESM::RefId> seen;
        for (const auto& record : store.get<ESM4::Sound>())
        {
            if (!seen.insert(ESM::RefId(record.mId)).second)
                continue;
            const auto* winner = store.get<ESM4::Sound>().search(record.mId);
            if (!winner || !Misc::StringUtils::ciEqual(winner->mEditorId, editorId))
                continue;
            if (result)
                throw std::invalid_argument("ambiguous native animation sound editor ID");
            result = winner;
        }
        return result;
    }

    namespace
    {
        constexpr VFS::Path::NormalizedView soundDir("sound");
        constexpr VFS::Path::ExtensionView mp3("mp3");

        struct AudioParams
        {
            float mAudioDefaultMinDistance;
            float mAudioDefaultMaxDistance;
            float mAudioMinDistanceMult;
            float mAudioMaxDistanceMult;
        };

        AudioParams makeAudioParams(const MWWorld::Store<ESM::GameSetting>& settings)
        {
            AudioParams params;
            params.mAudioDefaultMinDistance = settings.find("fAudioDefaultMinDistance")->mValue.getFloat();
            params.mAudioDefaultMaxDistance = settings.find("fAudioDefaultMaxDistance")->mValue.getFloat();
            params.mAudioMinDistanceMult = settings.find("fAudioMinDistanceMult")->mValue.getFloat();
            params.mAudioMaxDistanceMult = settings.find("fAudioMaxDistanceMult")->mValue.getFloat();
            return params;
        }

        VFS::Path::Normalized resolveNativeSoundPath(std::string_view value, bool isolated = false)
        {
            const auto* vfs = MWBase::Environment::get().getResourceSystem()->getVFS();
            if (!value.ends_with('\\') && !value.ends_with('/'))
                return Misc::ResourceHelpers::correctResourcePath(
                    { { soundDir } }, VFS::Path::toNormalized(value), *vfs, mp3);

            const VFS::Path::Normalized directory
                = soundDir / VFS::Path::toNormalized(value.substr(0, value.size() - 1));
            std::vector<VFS::Path::Normalized> candidates;
            for (const VFS::Path::Normalized& path
                : vfs->getRecursiveDirectoryIterator(VFS::Path::NormalizedView(directory)))
            {
                const auto extension = path.extension();
                if (extension == "mp3" || extension == "wav" || extension == "ogg" || extension == "flac")
                    candidates.push_back(path);
            }
            if (candidates.empty())
                return directory;
            std::sort(candidates.begin(), candidates.end());
            auto copiedGenerator = Misc::Rng::getGenerator();
            const auto selected = static_cast<std::size_t>(Misc::Rng::rollDice(
                static_cast<int>(candidates.size()), isolated ? copiedGenerator : Misc::Rng::getGenerator()));
            Log(Debug::Verbose) << "M10 native sound directory: " << directory << " -> " << candidates[selected];
            return candidates[selected];
        }
    }

    SoundBufferPool::SoundBufferPool(SoundOutput& output)
        : mOutput(&output)
        , mBufferCacheMax(Settings::sound().mBufferCacheMax * 1024 * 1024)
        , mBufferCacheMin(
              std::min(static_cast<std::size_t>(Settings::sound().mBufferCacheMin) * 1024 * 1024, mBufferCacheMax))
    {
    }

    SoundBufferPool::~SoundBufferPool()
    {
        clear();
    }

    SoundBuffer* SoundBufferPool::lookup(const ESM::RefId& soundId) const
    {
        const auto it = mBufferNameMap.find(soundId);
        if (it != mBufferNameMap.end())
        {
            SoundBuffer* sfx = it->second;
            if (sfx->getHandle() != nullptr)
                return sfx;
        }
        return nullptr;
    }

    SoundBuffer* SoundBufferPool::lookupPrepared(const ESM::RefId& soundId) const
    {
        const auto found = mImmutableBufferNameMap.find(soundId);
        return found != mImmutableBufferNameMap.end() && found->second->getHandle() ? found->second : nullptr;
    }

    SoundBuffer* SoundBufferPool::lookup(VFS::Path::NormalizedView fileName) const
    {
        const auto it = mBufferFileNameMap.find(fileName);
        if (it != mBufferFileNameMap.end())
        {
            SoundBuffer* sfx = it->second;
            if (sfx->getHandle() != nullptr)
                return sfx;
        }
        return nullptr;
    }

    SoundBuffer* SoundBufferPool::loadSfx(SoundBuffer* sfx)
    {
        if (sfx->getHandle() != nullptr)
            return sfx;

        auto [handle, size] = mOutput->loadSound(sfx->getResourceName());
        if (handle == nullptr)
            return {};

        sfx->mHandle = handle;

        mBufferCacheSize += size;
        if (mBufferCacheSize > mBufferCacheMax)
        {
            unloadUnused();
            if (!mUnusedBuffers.empty() && mBufferCacheSize > mBufferCacheMax)
                Log(Debug::Warning) << "No unused sound buffers to free, using " << mBufferCacheSize << " bytes!";
        }
        mUnusedBuffers.push_front(sfx);

        return sfx;
    }

    SoundBuffer* SoundBufferPool::load(const ESM::RefId& soundId)
    {
        if (mBufferNameMap.empty())
        {
            const MWWorld::ESMStore* esmstore = MWBase::Environment::get().getESMStore();
            for (const ESM::Sound& sound : esmstore->get<ESM::Sound>())
                insertSound(sound.mId, sound);
            for (const ESM4::Sound& sound : esmstore->get<ESM4::Sound>())
                if (!mBufferNameMap.contains(ESM::RefId(sound.mId)))
                    if (const auto* winner = esmstore->get<ESM4::Sound>().search(sound.mId))
                        insertSound(winner->mId, *winner);
            for (const ESM4::SoundReference& sound : esmstore->get<ESM4::SoundReference>())
                if (!mBufferNameMap.contains(ESM::RefId(sound.mId)))
                    if (const auto* winner = esmstore->get<ESM4::SoundReference>().search(sound.mId))
                        insertSound(winner->mId, *winner);
        }

        SoundBuffer* sfx;
        const auto it = mBufferNameMap.find(soundId);
        if (it != mBufferNameMap.end())
            sfx = it->second;
        else
        {
            const ESM::Sound* sound = MWBase::Environment::get().getESMStore()->get<ESM::Sound>().search(soundId);
            if (sound == nullptr)
                return {};
            sfx = insertSound(soundId, *sound);
        }

        return loadSfx(sfx);
    }

    std::shared_ptr<SoundBuffer> SoundBufferPool::prepareImmutable(const ESM::RefId& soundId)
    {
        if (!mPreparationIdentity)
            mPreparationIdentity = std::make_shared<const char>();
        SoundBuffer* buffer = nullptr;
        const auto existing = mImmutableBufferNameMap.find(soundId);
        if (existing != mImmutableBufferNameMap.end())
            buffer = existing->second;
        else
        {
            const auto& store = *MWBase::Environment::get().getESMStore();
            if (const auto* sound = store.get<ESM::Sound>().searchStatic(soundId))
                buffer = insertSound(soundId, *sound, false);
            else if (const auto* native = store.get<ESM4::Sound>().searchStatic(soundId))
                buffer = insertSound(soundId, *native, false);
            else if (const auto* reference = store.get<ESM4::SoundReference>().searchStatic(soundId))
                buffer = insertSound(soundId, *reference, false);
            else
                return {};
            mImmutableBufferNameMap.emplace(soundId, buffer);
        }
        if (!loadSfx(buffer))
            return {};
        use(*buffer);
        const std::weak_ptr<const char> identity = mPreparationIdentity;
        return std::shared_ptr<SoundBuffer>(buffer, [this, identity](SoundBuffer* sound) {
            if (!identity.expired())
                release(*sound);
        });
    }

    SoundBuffer* SoundBufferPool::load(VFS::Path::NormalizedView fileName)
    {
        SoundBuffer* sfx;
        const auto it = mBufferFileNameMap.find(fileName);
        if (it != mBufferFileNameMap.end())
            sfx = it->second;
        else
            sfx = insertSound(fileName);

        return loadSfx(sfx);
    }

    void SoundBufferPool::clear()
    {
        mPreparationIdentity.reset();
        mImmutableBufferNameMap.clear();
        for (auto& sfx : mSoundBuffers)
        {
            if (sfx.mHandle)
                mOutput->unloadSound(sfx.mHandle);
            sfx.mHandle = nullptr;
            sfx.mUses = 0;
        }

        mBufferFileNameMap.clear();
        mBufferNameMap.clear();
        mUnusedBuffers.clear();
    }

    SoundBuffer* SoundBufferPool::insertSound(VFS::Path::NormalizedView fileName)
    {
        static const AudioParams audioParams
            = makeAudioParams(MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>());

        float volume = 1.f;
        float min = std::max(audioParams.mAudioDefaultMinDistance * audioParams.mAudioMinDistanceMult, 1.f);
        float max = std::max(min, audioParams.mAudioDefaultMaxDistance * audioParams.mAudioMaxDistanceMult);

        min = std::max(min, 1.0f);
        max = std::max(min, max);

        SoundBuffer& sfx = mSoundBuffers.emplace_back(fileName, volume, min, max);

        mBufferFileNameMap.emplace(fileName, &sfx);
        return &sfx;
    }

    SoundBuffer* SoundBufferPool::insertSound(const ESM::RefId& soundId, const ESM::Sound& sound, bool index)
    {
        static const AudioParams audioParams
            = makeAudioParams(MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>());

        float volume = static_cast<float>(std::pow(10.0, (sound.mData.mVolume / 255.0 * 3348.0 - 3348.0) / 2000.0));
        float min = sound.mData.mMinRange;
        float max = sound.mData.mMaxRange;
        if (min == 0 && max == 0)
        {
            min = audioParams.mAudioDefaultMinDistance;
            max = audioParams.mAudioDefaultMaxDistance;
        }

        min *= audioParams.mAudioMinDistanceMult;
        max *= audioParams.mAudioMaxDistanceMult;
        min = std::max(min, 1.0f);
        max = std::max(min, max);

        SoundBuffer& sfx = mSoundBuffers.emplace_back(
            Misc::ResourceHelpers::correctSoundPath(VFS::Path::toNormalized(sound.mSound)), volume, min, max);

        if (index)
            mBufferNameMap.emplace(soundId, &sfx);
        return &sfx;
    }

    SoundBuffer* SoundBufferPool::insertSound(const ESM::RefId& soundId, const ESM4::Sound& sound, bool index)
    {
        static const AudioParams audioParams
            = makeAudioParams(MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>());
        VFS::Path::Normalized path = resolveNativeSoundPath(sound.mSoundFile, !index);
        const NativeSoundParams params = makeNativeSoundParams(sound.mData.staticAttenuation,
            sound.mData.minAttenuation, sound.mData.maxAttenuation, audioParams.mAudioDefaultMinDistance,
            audioParams.mAudioDefaultMaxDistance, audioParams.mAudioMinDistanceMult,
            audioParams.mAudioMaxDistanceMult);
        SoundBuffer& sfx
            = mSoundBuffers.emplace_back(std::move(path), params.mVolume, params.mMinDistance, params.mMaxDistance);
        if (index)
            mBufferNameMap.emplace(soundId, &sfx);
        return &sfx;
    }

    SoundBuffer* SoundBufferPool::insertSound(const ESM::RefId& soundId, const ESM4::SoundReference& sound, bool index)
    {
        VFS::Path::Normalized path = resolveNativeSoundPath(sound.mSoundFile, !index);
        const float volume = std::pow(10.f, -static_cast<float>(sound.mData.staticAttenuation) / 2000.f);
        const float min = 1.f;
        const float max = 255.f;
        // TODO: sound.mSoundId can link to another SoundReference, probably we will need to add additional lookups to
        // ESMStore.
        SoundBuffer& sfx = mSoundBuffers.emplace_back(std::move(path), volume, min, max);
        if (index)
            mBufferNameMap.emplace(soundId, &sfx);
        return &sfx;
    }

    void SoundBufferPool::unloadUnused()
    {
        while (!mUnusedBuffers.empty() && mBufferCacheSize > mBufferCacheMin)
        {
            SoundBuffer* const unused = mUnusedBuffers.back();

            mBufferCacheSize -= mOutput->unloadSound(unused->getHandle());
            unused->mHandle = nullptr;

            mUnusedBuffers.pop_back();
        }
    }
}
