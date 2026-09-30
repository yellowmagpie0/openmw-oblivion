#ifndef GAME_MWMECHANICS_STAT_H
#define GAME_MWMECHANICS_STAT_H

#include <array>
#include <cstdint>
#include <optional>

namespace ESM4
{
    struct ActorValueState;
    enum class ActorValueOwner : std::uint8_t;
    enum class ActorValueProcess : std::uint8_t;
}

namespace ESM
{
    template <typename T>
    struct StatState;
}

namespace MWMechanics
{
    template <typename T>
    class Stat
    {
        T mBase;
        T mModifier;
        struct NativeValue
        {
            float mBase;
            std::array<std::optional<float>, 3> mModifiers;
            ESM4::ActorValueOwner mOwner;
            ESM4::ActorValueProcess mProcess;
            T mCurrent;
        };
        std::optional<NativeValue> mNativeCurrent;
        void requireWritable() const;

    public:
        typedef T Type;

        Stat();
        Stat(T base, T modified);

        const T& getBase() const { return mBase; }

        T getModified(bool capped = true) const;
        T getModifier() const { return mModifier; }

        void setBase(const T& value) { requireWritable(); mBase = value; }

        void setModifier(const T& modifier) { requireWritable(); mModifier = modifier; }

        void setNativeProjection(const ESM4::ActorValueState& state,
            ESM4::ActorValueOwner owner, ESM4::ActorValueProcess process);
        bool isNativeProjection() const { return mNativeCurrent.has_value(); }
        float getNativeModifier() const;
        T getModifiedWithOverrides(std::optional<float> base, std::optional<float> modifier) const;

        void writeState(ESM::StatState<T>& state) const;
        void readState(const ESM::StatState<T>& state);
    };

    template <typename T>
    inline bool operator==(const Stat<T>& left, const Stat<T>& right)
    {
        return left.getBase() == right.getBase() && left.getModifier() == right.getModifier()
            && left.isNativeProjection() == right.isNativeProjection()
            && (!left.isNativeProjection() || (left.getModified() == right.getModified()
                && left.getNativeModifier() == right.getNativeModifier()));
    }

    template <typename T>
    inline bool operator!=(const Stat<T>& left, const Stat<T>& right)
    {
        return !(left == right);
    }

    template <typename T>
    class DynamicStat
    {
        friend class OblivionActorProjection;
        Stat<T> mStatic;
        T mCurrent;
        std::optional<T> mNativeModified;

        void requireWritable() const;

    public:
        typedef T Type;

        DynamicStat();
        DynamicStat(const DynamicStat&) = default;
        DynamicStat& operator=(const DynamicStat& other);
        DynamicStat(T base);
        DynamicStat(T base, T modified, T current);
        DynamicStat(const Stat<T>& stat, T current);

        const T& getBase() const { return mStatic.getBase(); }
        T getModified(bool capped = true) const
        {
            return mNativeModified ? *mNativeModified : mStatic.getModified(capped);
        }
        const T& getCurrent() const { return mCurrent; }
        T getRatio(bool nanIsZero = true) const;

        /// Set base and adjust current accordingly.
        void setBase(const T& value) { requireWritable(); mStatic.setBase(value); }

        void setCurrent(const T& value, bool allowDecreaseBelowZero = false, bool allowIncreaseAboveModified = false);

        T getModifier() const { return mStatic.getModifier(); }
        void setModifier(T value) { requireWritable(); mStatic.setModifier(value); }

        // Values are prepared by native authority, including any AV-specific
        // maximum/current semantics. This adapter performs no native formulas.
        void setNativeProjection(T base, T modified, T current);
        bool isNativeProjection() const { return mNativeModified.has_value(); }

        void writeState(ESM::StatState<T>& state) const;
        void readState(const ESM::StatState<T>& state);
    };

    template <typename T>
    inline bool operator==(const DynamicStat<T>& left, const DynamicStat<T>& right)
    {
        return left.getBase() == right.getBase() && left.getModifier() == right.getModifier()
            && left.getCurrent() == right.getCurrent() && left.isNativeProjection() == right.isNativeProjection()
            && (!left.isNativeProjection() || left.getModified() == right.getModified());
    }

    template <typename T>
    inline bool operator!=(const DynamicStat<T>& left, const DynamicStat<T>& right)
    {
        return !(left == right);
    }

    class AttributeValue
    {
        friend class OblivionActorProjection;
        float mBase;
        float mModifier;
        float mDamage; // needs to be float to allow continuous damage
        struct NativeValue
        {
            float mCurrent;
            float mScript;
            ESM4::ActorValueOwner mOwner;
            ESM4::ActorValueProcess mProcess;
        };
        std::optional<NativeValue> mNativeCurrent;

        void requireWritable() const;

    public:
        AttributeValue();
        AttributeValue(const AttributeValue&) = default;
        AttributeValue& operator=(const AttributeValue& other);

        // A read-only view of native authority. Legacy mutations must not
        // silently collapse native modifier categories or apply TES3 clamps.
        void setNativeProjection(const ESM4::ActorValueState& state,
            ESM4::ActorValueOwner owner, ESM4::ActorValueProcess process);
        bool isNativeProjection() const { return mNativeCurrent.has_value(); }

        float getModified() const;
        // Read a queued Lua view without changing the projection/authority.
        float getModifiedWithOverrides(float base, float modifier, float damage) const;
        float getBase() const;
        float getModifier() const;

        void setBase(float base, bool clearModifier = false);

        void setModifier(float mod);

        // Maximum attribute damage is limited to the modified value.
        // Note: MW applies damage directly to mModified, however it does track how much
        // a damaged attribute that has been fortified beyond its base can be restored.
        // Getting rid of mDamage would require calculating its value by ignoring active effects when restoring
        void damage(float damage);
        void restore(float amount);

        float getDamage() const;

        void writeState(ESM::StatState<float>& state) const;
        void readState(const ESM::StatState<float>& state);
    };

    class SkillValue : public AttributeValue
    {
        float mProgress;

    public:
        SkillValue();
        float getProgress() const;
        void setProgress(float progress);

        void writeState(ESM::StatState<float>& state) const;
        void readState(const ESM::StatState<float>& state);
    };

    inline bool operator==(const AttributeValue& left, const AttributeValue& right)
    {
        return left.getBase() == right.getBase() && left.getModifier() == right.getModifier()
            && left.getDamage() == right.getDamage() && left.isNativeProjection() == right.isNativeProjection()
            && (!left.isNativeProjection() || left.getModified() == right.getModified());
    }
    inline bool operator!=(const AttributeValue& left, const AttributeValue& right)
    {
        return !(left == right);
    }

    inline bool operator==(const SkillValue& left, const SkillValue& right)
    {
        return static_cast<const AttributeValue&>(left) == static_cast<const AttributeValue&>(right)
            && left.getProgress() == right.getProgress();
    }
    inline bool operator!=(const SkillValue& left, const SkillValue& right)
    {
        return !(left == right);
    }
}

#endif
