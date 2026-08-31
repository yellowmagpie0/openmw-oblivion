#ifndef OPENMW_COMPONENTS_ESM4_INVENTORYMECHANICS_H
#define OPENMW_COMPONENTS_ESM4_INVENTORYMECHANICS_H

#include <cstdint>
#include <vector>

#include "runtimestate.hpp"

namespace ESM4
{
    inline constexpr std::uint32_t InventorySlotWeapon = 1u << 16;
    inline constexpr std::uint32_t InventorySlotAmmunition = 1u << 17;
    inline constexpr std::uint32_t InventorySlotLight = 1u << 18;
    inline constexpr std::uint32_t InventorySlotMask = 0x7ffffu;

    enum class InventoryItemType : std::uint8_t
    {
        Ammunition,
        Apparatus,
        Armor,
        Book,
        Clothing,
        Ingredient,
        Key,
        Light,
        Miscellaneous,
        Potion,
        Scroll,
        SigilStone,
        SoulGem,
        Weapon,
    };

    struct InventoryItemDefinition
    {
        ESM::FormKey mBase;
        InventoryItemType mType = InventoryItemType::Miscellaneous;
        std::int32_t mValue = 0;
        float mWeight = 0.f;
        std::int32_t mMaxCondition = -1;
        float mMaxCharge = -1.f;
        std::uint32_t mSlots = 0;
        bool mChooseOneSlot = false;
        bool mConsumable = false;
        float mMaxUsageTime = -1.f;
    };

    struct BarterParameters
    {
        float mActorLuckSkillMult = 0.4f;
        float mHaggleDispositionMult = 0.5f;
        float mHaggleBase = 0.55f;
        float mBuyBase = 1.55f;
        float mBuyMult = -0.0045f;
        float mSellBase = 0.45f;
        float mSellMult = 0.0045f;
    };

    struct BarterActor
    {
        float mMercantile = 0.f;
        float mLuck = 50.f;
    };

    void normalizeInventory(std::vector<RuntimeInventoryItem>& inventory);
    std::int32_t inventoryCount(
        const std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base);
    bool addInventoryItem(std::vector<RuntimeInventoryItem>& inventory, RuntimeInventoryItem item);
    std::int32_t removeInventoryItem(
        std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base, std::int32_t count);
    std::int32_t transferInventoryItem(std::vector<RuntimeInventoryItem>& source,
        std::vector<RuntimeInventoryItem>& destination, const ESM::FormKey& base, std::int32_t count);

    bool equipInventoryItem(std::vector<RuntimeInventoryItem>& inventory,
        const InventoryItemDefinition& definition, std::uint32_t preferredSlot = 0);
    bool unequipInventoryItem(std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base);
    bool setInventoryHotkey(
        std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base, std::int32_t hotkey);
    bool consumeInventoryItem(std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base);
    bool repairInventoryItem(RuntimeInventoryItem& item, const InventoryItemDefinition& definition);
    bool rechargeInventoryItem(RuntimeInventoryItem& item, const InventoryItemDefinition& definition);

    float effectiveBarterSkill(const BarterActor& actor, const BarterParameters& parameters = {});
    std::int32_t barterOffer(std::int32_t baseValue, bool playerBuys, const BarterActor& player,
        const BarterActor& merchant, float disposition, const BarterParameters& parameters = {});
    float barterAcceptance(const BarterActor& player, const BarterActor& merchant, float disposition,
        float sliderPosition, const BarterParameters& parameters = {});
    std::int32_t repairCost(
        std::int32_t baseValue, std::int32_t condition, std::int32_t maxCondition, float costMultiplier = 0.9f);
    std::int32_t rechargeCost(float charge, float maxCharge, float costMultiplier = 1.f);
    std::int32_t trainingCost(std::int32_t currentSkill, float costMultiplier = 10.f);
    float lockpickAutoChance(std::int32_t security, float agility, float luck, std::int32_t lockLevel,
        float autoBase = 45.f, float difficultyMultiplier = 0.3f, float autoOffset = -4.f);
}

#endif
