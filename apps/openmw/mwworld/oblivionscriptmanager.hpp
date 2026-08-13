#ifndef GAME_MWWORLD_OBLIVIONSCRIPTMANAGER_H
#define GAME_MWWORLD_OBLIVIONSCRIPTMANAGER_H

#include <components/esm4/runtimestate.hpp>
#include <components/obscript/compiler.hpp>
#include <components/obscript/vm.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "ptr.hpp"

namespace MWWorld
{
    class ESMStore;
    class World;

    // Main-thread host for the portable M6 ObScript Program. Commands mutate
    // the native TES4 world immediately; dispatch is synchronous and bounded.
    class OblivionScriptManager final : public ObScript::RuntimeHost
    {
    public:
        OblivionScriptManager(World& world, ESMStore& store, const std::vector<std::string>& contentFiles);

        void clear();
        void update(double secondsPassed);
        bool dispatchObjectEvent(const Ptr& self, std::string_view event, const Ptr& actionReference = {});
        bool dispatchObjectEvent(const ESM::FormKey& self, std::string_view event,
            const ESM::FormKey& actionReference = {});
        bool dispatchActivation(const Ptr& self, const Ptr& actionReference);
        void setStage(const ESM::FormKey& quest, std::int32_t stage);
        void dispatchDialogueResult(const ESM::FormKey& info, std::uint32_t ordinal, const Ptr& actor = {});
        void dispatchEffect(
            const ESM::FormKey& script, std::string_view event, const Ptr& target, const Ptr& caster = {});

        void capture(ESM4::RuntimeState& state) const;
        void restore(const ESM4::RuntimeState& state);
        void writeRuntimeReport() const;

        ObScript::Value resolveName(std::string_view name, const ObScript::RuntimeContext& context) override;
        ObScript::Value loadMember(const ObScript::Value& target, std::string_view name,
            const ObScript::RuntimeContext& context) override;
        void storeExternal(std::string_view name, const ObScript::Value& value,
            const ObScript::RuntimeContext& context) override;
        void storeMember(const ObScript::Value& target, std::string_view name, const ObScript::Value& value,
            const ObScript::RuntimeContext& context) override;
        ObScript::Value call(std::string_view name, const std::optional<ObScript::Value>& target,
            const std::vector<ObScript::Value>& arguments, const ObScript::RuntimeContext& context,
            const ObScript::SourceLocation& location) override;

    private:
        struct InstanceKey
        {
            std::string mUnit;
            ESM::FormKey mContext;
            friend bool operator<(const InstanceKey& left, const InstanceKey& right)
            {
                return std::tie(left.mUnit, left.mContext) < std::tie(right.mUnit, right.mContext);
            }
        };

        struct Instance
        {
            std::vector<ObScript::Value> mLocals;
            bool mOnLoadFired = false;
        };

        struct ScheduledEvent
        {
            double mAt = 0;
            std::vector<std::string> mWords;
            bool mExecuted = false;
        };

        World& mWorld;
        ESMStore& mStore;
        ESM::FormKeyResolver mResolver;
        ObScript::CoverageRegistry mCoverage;
        ObScript::CompilationCache mCache;
        ObScript::VirtualMachine mVm;
        std::map<std::string, std::shared_ptr<const ObScript::Program>, std::less<>> mProgramsByUnit;
        std::map<ESM::FormKey, std::shared_ptr<const ObScript::Program>> mScripts;
        std::map<ESM::FormKey, std::shared_ptr<const ObScript::Program>> mBaseScripts;
        std::map<ESM::FormKey, ESM::FormKey> mReferenceBases;
        std::map<ESM::FormKey, std::vector<std::shared_ptr<const ObScript::Program>>> mDialogueResults;
        std::map<ESM::FormKey, std::vector<ESM::FormKey>> mTopicInfos;
        std::map<ESM::FormKey, std::vector<std::string>> mNativeVoiceFiles;
        std::map<ESM::FormKey, std::vector<std::shared_ptr<const ObScript::Program>>> mQuestResults;
        std::map<ESM::FormKey, ESM::FormKey> mQuestScripts;
        std::map<InstanceKey, Instance> mInstances;
        std::map<ESM::FormKey, ESM4::RuntimeQuestState> mQuests;
        std::vector<ObScript::RuntimeDiagnostic> mDiagnostics;
        std::vector<std::string> mTrace;
        std::vector<ScheduledEvent> mScheduledEvents;
        std::filesystem::path mReportPath;
        std::uint64_t mSequence = 0;
        std::uint32_t mDepth = 0;
        double mElapsed = 0;
        std::size_t mCompiledUnits = 0;
        std::size_t mCompilationFailures = 0;
        std::map<std::string, std::uint64_t, std::less<>> mCommandCounts;
        std::set<ESM::FormKey> mSuppressedActivations;

        void compileCorpus();
        void indexNativeVoices();
        std::optional<std::string> findNativeVoice(
            const ESM::FormKey& topic, const Ptr& actor, const ESM::FormKey& voiceType) const;
        void loadScheduledEvents();
        void runScheduledEvents();
        void executeScheduledEvent(const ScheduledEvent& event);
        bool execute(const std::shared_ptr<const ObScript::Program>& program, const ESM::FormKey& instance,
            const ESM::FormKey& self, std::string_view event, const ESM::FormKey& actionReference = {});
        Instance& instanceFor(const ObScript::Program& program, const ESM::FormKey& context);
        std::shared_ptr<const ObScript::Program> scriptFor(const Ptr& ptr) const;
        std::shared_ptr<const ObScript::Program> scriptFor(const ESM::FormKey& key);
        bool dispatchBaseEvent(const ESM::FormKey& base, std::string_view event,
            const ESM::FormKey& actionReference);
        Ptr ptrFor(const ESM::FormKey& key);
        ESM::FormKey keyFor(const Ptr& ptr) const;
        std::optional<ESM::FormKey> keyFromValue(const ObScript::Value& value) const;
        ESM::FormKey keyArgument(const std::optional<ObScript::Value>& target,
            const std::vector<ObScript::Value>& arguments, std::size_t argument,
            const ObScript::RuntimeContext& context) const;
        ESM4::RuntimeReferenceState* referenceState(const ESM::FormKey& key);
        const ESM4::RuntimeReferenceState* referenceState(const ESM::FormKey& key) const;
        ESM4::RuntimeQuestState& questState(const ESM::FormKey& key);
        ObScript::Value loadScriptLocal(const ESM::FormKey& target, std::string_view name);
        bool storeScriptLocal(const ESM::FormKey& target, std::string_view name, const ObScript::Value& value);
        void trace(std::string value);
        void recordDiagnostic(const ObScript::RuntimeDiagnostic& diagnostic);
    };
}

#endif
