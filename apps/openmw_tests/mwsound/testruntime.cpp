#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>

#include "apps/openmw/mwsound/soundmanagerimp.hpp"
#include "apps/openmw/mwsound/openaloutput.hpp"
#include "apps/openmw/mwsound/sound.hpp"
#include "apps/openmw/mwsound/sounddecoder.hpp"

namespace
{
    class ContinuousDecoder final : public MWSound::SoundDecoder
    {
    public:
        std::atomic<size_t> bytesRead{0};
        std::atomic<int> closes{0};
        std::function<void()> infoCallback;
        std::function<void()> offsetCallback;
        ContinuousDecoder() : SoundDecoder(nullptr) {}
        void open(VFS::Path::NormalizedView) override {}
        void close() override { ++closes; }
        std::string getName() override { return "retention-test"; }
        void getInfo(int* rate, MWSound::ChannelConfig* channels, MWSound::SampleType* type) override
        {
            if (infoCallback) infoCallback();
            *rate = 44100; *channels = MWSound::ChannelConfig_Mono; *type = MWSound::SampleType_Int16;
        }
        size_t read(char* output, size_t bytes) override
        {
            std::memset(output, 0, bytes);
            bytesRead += bytes;
            return bytes;
        }
        size_t getSampleOffset() override
        {
            if (offsetCallback) offsetCallback();
            return bytesRead / 2;
        }
    };

    struct ObservableSound : MWSound::Sound
    {
        ALuint source() const { return static_cast<ALuint>(reinterpret_cast<uintptr_t>(mHandle)); }
    };

    ALint state(ALuint source)
    {
        ALint result = 0; alGetSourcei(source, AL_SOURCE_STATE, &result); return result;
    }

    struct SoundRuntime : testing::Test
    {
        std::unique_ptr<MWSound::SoundManager> manager;
        void SetUp() override
        {
            manager = std::make_unique<MWSound::SoundManager>(nullptr, true);
            ASSERT_TRUE(manager->isEnabled());
        }
        void TearDown() override { manager.reset(); }
    };
}

TEST_F(SoundRuntime, DecoderAndVoiceIdentitySurviveSelectionWithNoParkedReads)
{
    auto original = std::make_shared<ContinuousDecoder>();
    auto* track = manager->playTrack(original, MWSound::Type::Voice);
    ASSERT_NE(track, nullptr);
    track->setVolume(0.375f);
    track->setFade(5.f, 0.125f);
    const float volume = track->getRealVolume();
    manager->setSimulationTimeScale(0.75f);
    auto retained = manager->retainRuntime();
    EXPECT_THROW(manager->clear(), std::logic_error);
    EXPECT_THROW(manager->retainRuntime(), std::logic_error);
    ASSERT_TRUE(retained.detach());
    const auto parkedReads = original->bytesRead.load();
    const auto parkedDelay = manager->getTrackTimeDelay(track);
    EXPECT_FLOAT_EQ(manager->getSimulationTimeScale(), 1.f);
    auto incoming = std::make_shared<ContinuousDecoder>();
    auto* candidate = manager->playTrack(incoming, MWSound::Type::Music);
    ASSERT_NE(candidate, nullptr);
    ASSERT_NE(candidate, track);
    manager->pauseSounds(MWSound::BlockerType::VideoPlayback);
    manager->resumeSounds(MWSound::BlockerType::VideoPlayback);
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    EXPECT_EQ(original->bytesRead.load(), parkedReads);
    EXPECT_DOUBLE_EQ(manager->getTrackTimeDelay(track), parkedDelay);
    ASSERT_TRUE(retained.exchange());
    EXPECT_FLOAT_EQ(manager->getSimulationTimeScale(), 0.75f);
    EXPECT_FLOAT_EQ(track->getRealVolume(), volume);
    EXPECT_TRUE(track->getInFade());
    const auto candidateReads = incoming->bytesRead.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    EXPECT_EQ(incoming->bytesRead.load(), candidateReads);
    ASSERT_TRUE(retained.exchange());
    ASSERT_TRUE(retained.restore());
    EXPECT_EQ(original->closes, 0);
    EXPECT_EQ(incoming->closes, 1);
    manager->stopTrack(track);
    EXPECT_EQ(original->closes, 1);
}

TEST_F(SoundRuntime, ScopeMoveAndCommitRetireOnlyUnwantedDecoder)
{
    auto original = std::make_shared<ContinuousDecoder>();
    auto* track = manager->playTrack(original, MWSound::Type::Voice);
    ASSERT_NE(track, nullptr);
    {
        auto retained = manager->retainRuntime();
        ASSERT_TRUE(retained.detach());
        auto moved = std::move(retained);
        EXPECT_FALSE(retained.restore());
    }
    EXPECT_EQ(original->closes, 0);
    auto retained = manager->retainRuntime();
    ASSERT_TRUE(retained.detach());
    auto incoming = std::make_shared<ContinuousDecoder>();
    auto* candidate = manager->playTrack(incoming, MWSound::Type::Music);
    ASSERT_NE(candidate, nullptr);
    ASSERT_TRUE(retained.exchange()); // Commit can start with the original selected.
    retained.discard();
    EXPECT_EQ(original->closes, 1);
    EXPECT_EQ(incoming->closes, 0);
    manager->stopTrack(candidate);
    EXPECT_EQ(incoming->closes, 1);
}

TEST_F(SoundRuntime, OwnerDeathRetiresBothDomainsBeforeOutputAndPools)
{
    auto original = std::make_shared<ContinuousDecoder>();
    ASSERT_NE(manager->playTrack(original, MWSound::Type::Voice), nullptr);
    auto retained = manager->retainRuntime();
    ASSERT_TRUE(retained.detach());
    auto incoming = std::make_shared<ContinuousDecoder>();
    ASSERT_NE(manager->playTrack(incoming, MWSound::Type::Music), nullptr);
    manager.reset();
    EXPECT_EQ(original->closes, 1);
    EXPECT_EQ(incoming->closes, 1);
    EXPECT_FALSE(retained.restore());
    EXPECT_FALSE(retained.exchange());
    retained.discard();
}

TEST_F(SoundRuntime, DecoderInitializationRejectsRetentionInsideActiveDispatch)
{
    auto decoder = std::make_shared<ContinuousDecoder>();
    int rejected = 0;
    decoder->infoCallback = [&] {
        EXPECT_THROW(manager->retainRuntime(), std::logic_error);
        ++rejected;
    };
    ASSERT_NE(manager->playTrack(decoder, MWSound::Type::Voice), nullptr);
    EXPECT_EQ(rejected, 1);
    auto retained = manager->retainRuntime();
    ASSERT_TRUE(retained.detach());
    ASSERT_TRUE(retained.restore());
}

TEST(SoundOutputRuntime, ActualSourcesAndAlreadyPausedStateRemainUnchanged)
{
    MWSound::SoundManager owner(nullptr, false);
    MWSound::OpenALOutput output(owner);
    ASSERT_TRUE(output.init("", "", MWSound::HrtfMode::Disable));
    ALuint buffer = 0; alGenBuffers(1, &buffer);
    const short pcm[44100] = {};
    alBufferData(buffer, AL_FORMAT_MONO16, pcm, sizeof(pcm), 44100);
    ObservableSound playing, paused, candidate;
    MWSound::SoundParams params;
    params.mFlags = int(MWSound::Type::Sfx) | int(MWSound::PlayMode::LoopNoEnvNoScaling);
    playing.init(params); paused.init(params); candidate.init(params);
    const auto handle = reinterpret_cast<void*>(static_cast<uintptr_t>(buffer));
    ASSERT_TRUE(output.playSound(&playing, handle, 0.25f));
    ASSERT_TRUE(output.playSound(&paused, handle, 0.5f));
    alSourcePause(paused.source());
    const ALuint originalSource = playing.source();
    auto retained = output.prepareRuntime();
    ASSERT_TRUE(retained->exchange());
    EXPECT_EQ(state(originalSource), AL_PAUSED);
    EXPECT_EQ(state(paused.source()), AL_PAUSED);
    ALfloat offset = 0; alGetSourcef(originalSource, AL_SEC_OFFSET, &offset);
    ASSERT_TRUE(output.playSound(&candidate, handle, 0));
    output.pauseSounds(int(MWSound::Type::Mask));
    output.resumeSounds(int(MWSound::Type::Mask));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ALfloat still = 0; alGetSourcef(originalSource, AL_SEC_OFFSET, &still);
    EXPECT_FLOAT_EQ(still, offset);
    ASSERT_TRUE(retained->exchange());
    EXPECT_EQ(playing.source(), originalSource);
    EXPECT_EQ(state(originalSource), AL_PLAYING);
    EXPECT_EQ(state(paused.source()), AL_PAUSED);
    output.finishSound(&playing); output.finishSound(&paused);
    ASSERT_TRUE(retained->exchange());
    output.finishSound(&candidate);
    retained.reset();
    alDeleteBuffers(1, &buffer);
    EXPECT_EQ(alGetError(), AL_NO_ERROR);
    output.deinit();
}

TEST(SoundOutputRuntime, DecoderOffsetCallbackCannotReenterSelection)
{
    MWSound::SoundManager owner(nullptr, false);
    MWSound::OpenALOutput output(owner);
    ASSERT_TRUE(output.init("", "", MWSound::HrtfMode::Disable));
    auto decoder = std::make_shared<ContinuousDecoder>();
    MWSound::Stream stream;
    MWSound::SoundParams params;
    params.mFlags = int(MWSound::Type::Voice) | int(MWSound::PlayMode::NoEnvNoScaling);
    stream.init(params);
    ASSERT_TRUE(output.streamSound(decoder, &stream));
    auto retained = output.prepareRuntime();
    int calls = 0;
    decoder->offsetCallback = [&] {
        ++calls;
        EXPECT_FALSE(retained->exchange());
        EXPECT_THROW(output.prepareRuntime(), std::logic_error);
    };
    output.getStreamOffset(&stream);
    EXPECT_EQ(calls, 1);
    decoder->offsetCallback = {};
    EXPECT_TRUE(retained->exchange());
    EXPECT_TRUE(retained->exchange());
    output.finishStream(&stream);
    retained.reset();
    output.deinit();
}
