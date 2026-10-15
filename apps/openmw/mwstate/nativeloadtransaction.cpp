#include "nativeloadtransaction.hpp"
#include "statemanagerimp.hpp"

#include <optional>
#include <stdexcept>
#include <osgViewer/Viewer>

#include "../mwbase/environment.hpp"
#include "../mwdialogue/journalimp.hpp"
#include "../mwdialogue/dialoguemanagerimp.hpp"
#include "../mwgui/windowmanagerimp.hpp"
#include "../mwinput/inputmanagerimp.hpp"
#include "../mwlua/luamanagerimp.hpp"
#include "../mwmechanics/mechanicsmanagerimp.hpp"
#include "../mwscript/scriptmanagerimp.hpp"
#include "../mwsound/soundmanagerimp.hpp"
#include "../mwworld/worldimp.hpp"

namespace MWState
{
    namespace
    {
        template<class T, class U> T& nativeService(U value)
        {
            auto* result = dynamic_cast<T*>(&*value);
            if (!result) throw std::logic_error("Native load requires native runtime services");
            return *result;
        }
    }

    struct NativeLoadTransaction::Data
    {
        MWWorld::World& world;
        osg::ref_ptr<osgViewer::Viewer> viewer;
        bool threadsRunning;
        bool worldDetached = false;
        bool finished = false;
        unsigned originalMask = 0;
        enum Domain : unsigned { World = 1, Mechanics = 2, Lua = 4, Windows = 8, State = 16,
            Journal = 32, Dialogue = 64, Legacy = 128, Input = 256, Sound = 512 };
        std::optional<MWWorld::World::RetainedSimulation> simulation;
        std::optional<MWDialogue::Journal::RetainedRuntime> journal;
        std::optional<MWDialogue::DialogueManager::RetainedRuntime> dialogue;
        std::optional<MWScript::ScriptManager::RetainedRuntime> legacy;
        std::optional<StateManager::RetainedRuntime> state;
        std::optional<MWMechanics::MechanicsManager::RetainedRuntime> mechanics;
        std::optional<MWLua::LuaManager::RetainedRuntime> lua;
        std::optional<MWGui::WindowManager::RetainedRuntime> windows;
        std::optional<MWInput::InputManager::RetainedRuntime> input;
        std::optional<MWSound::SoundManager::RetainedRuntime> sound;

        explicit Data(MWWorld::World& value)
            : world(value), viewer(value.getRuntimeViewer()), threadsRunning(viewer && viewer->areThreadsRunning())
        {
            if (!viewer) throw std::logic_error("Native load requires an initialized runtime viewer");
            viewer->stopThreading();
        }

        ~Data()
        {
            if (!finished)
            {
                try { rollback(); }
                catch (...) { std::terminate(); }
            }
        }

        void resumeViewer()
        {
            if (threadsRunning) viewer->startThreading();
        }

        void prepare(StateManager& manager)
        {
            auto& env = MWBase::Environment::get();
            simulation.emplace(world.retainSimulation());
            journal.emplace(nativeService<MWDialogue::Journal>(env.getJournal()).retainRuntime());
            dialogue.emplace(nativeService<MWDialogue::DialogueManager>(env.getDialogueManager()).retainRuntime());
            legacy.emplace(nativeService<MWScript::ScriptManager>(env.getScriptManager()).retainRuntime());
            state.emplace(manager.retainRuntime());
            mechanics.emplace(nativeService<MWMechanics::MechanicsManager>(env.getMechanicsManager()).retainRuntime());
            lua.emplace(nativeService<MWLua::LuaManager>(env.getLuaManager()).retainRuntime(false));
            windows.emplace(nativeService<MWGui::WindowManager>(env.getWindowManager()).retainRuntime());
            input.emplace(nativeService<MWInput::InputManager>(env.getInputManager()).retainRuntime());
            sound.emplace(nativeService<MWSound::SoundManager>(env.getSoundManager()).retainRuntime());
            if (!journal->detach() || !dialogue->detach() || !legacy->detach() || !state->detach()
                || !input->detach() || !sound->detach() || !mechanics->detach() || !lua->detach()
                || !windows->detach())
                throw std::runtime_error("Could not detach native runtime services");
            if (!simulation->detach())
            {
                simulation.reset();
                throw std::runtime_error("Could not detach native World");
            }
            worldDetached = true;
            windows->initialiseIncoming();
        }

        template<class T> static void restore(std::optional<T>& value)
        {
            if (value)
            {
                if (!value->restore()) throw std::logic_error("Native runtime rollback could not select original owner");
                value.reset();
            }
        }

        template<class T> void selectOriginal(std::optional<T>& value, unsigned bit)
        {
            if (!value->exchange()) throw std::logic_error("Native commit could not select outgoing owners");
            originalMask |= bit;
        }
        template<class T> void selectIncoming(std::optional<T>& value, unsigned bit)
        {
            if (!(originalMask & bit)) return;
            if (!value->exchange()) throw std::logic_error("Native rollback could not select incoming owners");
            originalMask &= ~bit;
        }

        void rollback()
        {
            if (finished) return;
            viewer->stopThreading();
            // A commit guard may reject before retirement. Put every already
            // exchanged domain back on the candidate before clearing its scene.
            selectIncoming(sound, Sound); selectIncoming(input, Input);
            selectIncoming(legacy, Legacy); selectIncoming(dialogue, Dialogue);
            selectIncoming(journal, Journal); selectIncoming(state, State);
            selectIncoming(windows, Windows); selectIncoming(lua, Lua);
            selectIncoming(mechanics, Mechanics); selectIncoming(simulation, World);
            if (worldDetached && !simulation->clearSelectedScene())
                throw std::logic_error("Native rollback could not retire candidate scene");
            restore(lua);
            restore(windows);
            restore(input);
            restore(sound);
            restore(mechanics);
            restore(legacy);
            restore(dialogue);
            restore(journal);
            restore(simulation);
            restore(state);
            finished = true;
            resumeViewer();
        }

        void commit()
        {
            viewer->stopThreading();
            simulation->prepareExchange();
            selectOriginal(simulation, World); selectOriginal(mechanics, Mechanics);
            selectOriginal(lua, Lua); selectOriginal(windows, Windows);
            selectOriginal(state, State); selectOriginal(journal, Journal);
            selectOriginal(dialogue, Dialogue); selectOriginal(legacy, Legacy);
            selectOriginal(input, Input); selectOriginal(sound, Sound);
            if (!simulation->clearSelectedScene())
                throw std::logic_error("Native commit could not retire outgoing scene");
            // These operations are irreversible: all fallible candidate work
            // has completed. Retire users before backend/model/base ownership.
            lua->discard(); lua.reset();
            windows->discard(); windows.reset();
            input->discard(); input.reset();
            sound->discard(); sound.reset();
            mechanics->discard(); mechanics.reset();
            legacy->discard(); legacy.reset();
            dialogue->discard(); dialogue.reset();
            journal->discard(); journal.reset();
            simulation->discard(); simulation.reset();
            state->discard(); state.reset();
            finished = true;
            resumeViewer();
        }
    };

    NativeLoadTransaction::NativeLoadTransaction(StateManager& state, MWWorld::World& world)
        : mData(std::make_unique<Data>(world))
    {
        mData->prepare(state);
    }
    NativeLoadTransaction::~NativeLoadTransaction() = default;
    void NativeLoadTransaction::startIncomingScripts()
    {
        nativeService<MWLua::LuaManager>(MWBase::Environment::get().getLuaManager()).startPreparedRuntime();
    }
    void NativeLoadTransaction::rollback() { mData->rollback(); }
    void NativeLoadTransaction::commit() { mData->commit(); }
}
