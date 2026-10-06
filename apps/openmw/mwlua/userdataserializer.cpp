#include "userdataserializer.hpp"

#include <cstring>
#include <cmath>
#include <set>

#include <components/lua/serialization.hpp>
#include <components/lua/luastate.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/misc/endianness.hpp>

#include "object.hpp"

namespace MWLua
{

    class Serializer final : public LuaUtil::UserdataSerializer
    {
    public:
        explicit Serializer(bool localSerializer, std::map<int, int>* contentFileMapping)
            : mLocalSerializer(localSerializer)
            , mContentFileMapping(contentFileMapping)
        {
        }

    private:
        // Appends serialized sol::userdata to the end of BinaryData.
        // Returns false if this type of userdata is not supported by this serializer.
        bool serialize(LuaUtil::BinaryData& out, const sol::userdata& data) const override
        {
            if (data.is<GObject>() || data.is<LObject>())
            {
                appendRefNum(out, data.as<Object>().id());
                return true;
            }
            if (data.is<GObjectList>())
            {
                appendObjectIdList(out, data.as<GObjectList>().mIds);
                return true;
            }
            if (data.is<LObjectList>())
            {
                appendObjectIdList(out, data.as<LObjectList>().mIds);
                return true;
            }
            return false;
        }

        constexpr static std::string_view sObjListTypeName = "objlist";
        void appendObjectIdList(LuaUtil::BinaryData& out, const ObjectIdList& objList) const
        {
            static_assert(sizeof(ESM::RefNum) == 8);
            if constexpr (Misc::IS_LITTLE_ENDIAN)
                append(out, sObjListTypeName, objList->data(), objList->size() * sizeof(ESM::RefNum));
            else
            {
                std::vector<ESM::RefNum> buf;
                buf.reserve(objList->size());
                for (ESM::RefNum v : *objList)
                    buf.push_back({ Misc::toLittleEndian(v.mIndex), Misc::toLittleEndian(v.mContentFile) });
                append(out, sObjListTypeName, buf.data(), buf.size() * sizeof(ESM::RefNum));
            }
        }

        void adjustRefNum(ESM::RefNum& refNum) const
        {
            if (refNum.hasContentFile() && mContentFileMapping)
            {
                auto iter = mContentFileMapping->find(refNum.mContentFile);
                if (iter != mContentFileMapping->end())
                    refNum.mContentFile = iter->second;
            }
        }

        // Deserializes userdata of type "typeName" from binaryData. Should push the result on stack using
        // sol::stack::push. Returns false if this type is not supported by this serializer.
        bool deserialize(std::string_view typeName, std::string_view binaryData, lua_State* lua) const override
        {
            if (typeName == sRefNumTypeName)
            {
                ObjectId id = loadRefNum(binaryData);
                adjustRefNum(id);
                if (mLocalSerializer)
                    sol::stack::push<LObject>(lua, LObject(id));
                else
                    sol::stack::push<GObject>(lua, GObject(id));
                return true;
            }
            if (typeName == sObjListTypeName)
            {
                if (binaryData.size() % sizeof(ESM::RefNum) != 0)
                    throw std::runtime_error("Invalid size of ObjectIdList in MWLua::Serializer");
                ObjectIdList objList = std::make_shared<std::vector<ESM::RefNum>>();
                objList->resize(binaryData.size() / sizeof(ESM::RefNum));
                std::memcpy(objList->data(), binaryData.data(), binaryData.size());
                for (ESM::RefNum& id : *objList)
                {
                    id.mIndex = Misc::fromLittleEndian(id.mIndex);
                    id.mContentFile = Misc::fromLittleEndian(id.mContentFile);
                    adjustRefNum(id);
                }
                if (mLocalSerializer)
                    sol::stack::push<LObjectList>(lua, LObjectList{ std::move(objList) });
                else
                    sol::stack::push<GObjectList>(lua, GObjectList{ std::move(objList) });
                return true;
            }
            return false;
        }

        bool mLocalSerializer;
        std::map<int, int>* mContentFileMapping;
    };

    std::unique_ptr<LuaUtil::UserdataSerializer> createUserdataSerializer(
        bool local, std::map<int, int>* contentFileMapping)
    {
        return std::make_unique<Serializer>(local, contentFileMapping);
    }

    ESM4::LocalLuaScripts validateSavedLuaRecord(ESM::ESMReader& reader, const ESM::LuaScriptsCfg& scripts)
    {
        double simulationTime;
        reader.getHNT(simulationTime, "LUAW");
        if (!std::isfinite(simulationTime) || simulationTime < 0)
            throw std::runtime_error("Invalid saved Lua simulation time");
        if (reader.getFormId(true).hasContentFile())
            throw std::runtime_error("Last generated RefNum is invalid");

        LuaUtil::ScriptsConfiguration configuration;
        configuration.init(scripts, false);
        struct RestoreConfiguration
        {
            ESM::ESMReader& mReader;
            const LuaUtil::ScriptsConfiguration* mPrevious;
            ~RestoreConfiguration() { mReader.mScriptsConfiguration = mPrevious; }
        } restore{ reader, reader.mScriptsConfiguration };
        configuration.read(reader);
        ESM::LuaScripts globals;
        globals.load(reader);
        sol::state lua;
        const auto globalSerializer = createUserdataSerializer(false);
        const auto localSerializer = createUserdataSerializer(true);
        ESM4::LocalLuaScripts native;
        LuaUtil::protectedCall(lua.lua_state(), [&](LuaUtil::LuaView& view) {
            const auto validateScripts = [&](const ESM::LuaScripts& data, const LuaUtil::UserdataSerializer* serializer) {
                std::set<int> ids;
                for (const auto& script : data.mScripts)
                {
                    if (script.mScriptId == -1 && reader.getFormatVersion() <= ESM::MaxLuaScriptPathFormatVersion)
                        continue; // Legacy path-based saves may name removed scripts.
                    if (!configuration.isValidSavedId(script.mScriptId) || !ids.insert(script.mScriptId).second)
                        throw std::runtime_error("Invalid or duplicate saved Lua script ID");
                    for (const auto& timer : script.mTimers)
                        if (!std::isfinite(timer.mTime))
                            throw std::runtime_error("Invalid saved Lua timer time");
                    if (!configuration.mapId(script.mScriptId))
                        continue; // Deliberately removed scripts are not restored.
                    static_cast<void>(LuaUtil::deserialize(view.sol(), script.mData, serializer));
                    for (const auto& timer : script.mTimers)
                        static_cast<void>(LuaUtil::deserialize(view.sol(), timer.mCallbackArgument, serializer));
                }
            };
            validateScripts(globals, globalSerializer.get());
            while (reader.isNextSub("LUAE"))
            {
                static_cast<void>(reader.getHString());
                static_cast<void>(reader.getFormId(true));
                static_cast<void>(LuaUtil::deserialize(view.sol(), ESM::loadLuaBinaryData(reader), globalSerializer.get()));
            }
            native = ESM4::loadLocalLuaScripts(reader);
            for (const auto& [key, data] : native)
                validateScripts(data, localSerializer.get());
        });
        return native;
    }

}
