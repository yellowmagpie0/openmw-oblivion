#include <gtest/gtest.h>

#include <components/esm/records.hpp>
#include <components/esm3/readerscache.hpp>
#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwclass/weapon.hpp"
#include "apps/openmw/mwclass/clothing.hpp"
#include "apps/openmw/mwworld/inventorystore.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"

#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivionprofileservices.hpp"

namespace
{
    TEST(OblivionProfileServicesTest, preparesDetachedInventoryBeforeAnyLiveReplacement)
    {
        MWWorld::ESMStore store;
        MWClass::Weapon::registerSelf();
        MWClass::Clothing::registerSelf();
        ESM4::Weapon weapon{};
        weapon.mId = {0x100, 0};
        const auto weaponKey = ESM::FormKey::content("items.esm", 0x100);
        store.getWritable<ESM4::Weapon>().insertStatic(weapon, weaponKey);
        ESM::Weapon sharedWeapon;
        sharedWeapon.blank();
        sharedWeapon.mId = ESM::RefId(weapon.mId);
        sharedWeapon.mData.mType = ESM::Weapon::LongBladeOneHand;
        sharedWeapon.mData.mWeight = 4;
        store.insertStatic(sharedWeapon);
        ESM4::Clothing ring{};
        ring.mId = {0x101, 0};
        const auto ringKey = ESM::FormKey::content("items.esm", 0x101);
        ring.mClothingFlags = ESM4::Armor::TES4_LeftRing | ESM4::Armor::TES4_RightRing;
        store.getWritable<ESM4::Clothing>().insertStatic(ring, ringKey);
        ESM::Clothing sharedRing;
        sharedRing.blank();
        sharedRing.mId = ESM::RefId(ring.mId);
        sharedRing.mData.mType = ESM::Clothing::Ring;
        sharedRing.mData.mWeight = 1;
        store.insertStatic(sharedRing);
        MWBase::Environment environment;
        environment.setESMStore(store);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(store, readers);
        environment.setWorldModel(model);
        struct TestInventory : MWWorld::InventoryStore
        {
            using ContainerStore::addNewStack;
        } live;
        MWWorld::ManualRef original(store, sharedWeapon.mId, 3);
        const auto originalItem = live.addNewStack(original.getPtr(), 3);
        model.registerPtr(*originalItem);
        const auto originalRef = originalItem->getCellRef().getRefNum();
        const auto revision = model.getPtrRegistryRevision();
        const auto lastGenerated = model.getLastGeneratedRefNum();
        const ESM::FormKeyResolver resolver({"items.esm", "owners.esm"});
        ESM4::RuntimeInventoryItem savedWeapon;
        savedWeapon.mBase = weaponKey;
        savedWeapon.mCount = 2;
        savedWeapon.mCondition = 31;
        savedWeapon.mCharge = 7.5f;
        savedWeapon.mOwner = ESM::FormKey::content("owners.esm", 0x200);
        savedWeapon.mEquippedSlots = ESM4::InventorySlotWeapon;
        ESM4::GlobalVariable permission{};
        permission.mId = {0x201, 1};
        permission.mType = 'f';
        permission.mValue = 1.f;
        const auto permissionKey = ESM::FormKey::content("owners.esm", 0x201);
        store.getWritable<ESM4::GlobalVariable>().insertStatic(permission, permissionKey);
        savedWeapon.mOwnershipRank = -1;
        savedWeapon.mOwnershipGlobal = permissionKey;
        ESM4::RuntimeInventoryItem savedRing;
        savedRing.mBase = ringKey;
        savedRing.mCount = 1;
        savedRing.mEquippedSlots = ESM4::Armor::TES4_LeftRing;
        const auto checkUntouched = [&]() {
            EXPECT_EQ(live.count(sharedWeapon.mId), 3);
            EXPECT_EQ(model.getPtr(originalRef), *originalItem);
            EXPECT_EQ(model.getPtrRegistryRevision(), revision);
            EXPECT_EQ(model.getLastGeneratedRefNum(), lastGenerated);
        };
        {
            auto prepared = MWWorld::OblivionProfileServices::prepareActorInventory(
                store, resolver, {savedWeapon, savedRing});
            ASSERT_EQ(prepared.size(), 2);
            const auto ptr = prepared[0].mReference.getPtr();
            EXPECT_EQ(ptr.getCellRef().getCount(), 2);
            EXPECT_EQ(ptr.getCellRef().getCharge(), 31);
            EXPECT_EQ(ptr.getCellRef().getEnchantmentCharge(), 7.5f);
            EXPECT_EQ(ptr.getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x200, 1}));
            EXPECT_EQ(ptr.getCellRef().getNativeOwnershipRank(), -1);
            EXPECT_EQ(ptr.getCellRef().getNativeOwnershipGlobal(), ESM::RefId(permission.mId));
            EXPECT_EQ(MWWorld::OblivionProfileServices::captureOwnershipGlobal(store, resolver, ptr.getCellRef()),
                permissionKey);
            EXPECT_FALSE(ptr.getCellRef().getRefNum().isSet());
            EXPECT_EQ(prepared[0].mEquipmentSlot, MWWorld::InventoryStore::Slot_CarriedRight);
            EXPECT_EQ(prepared[1].mEquipmentSlot, MWWorld::InventoryStore::Slot_LeftRing);
            checkUntouched();
            // Prepared references retain their identity across owning-vector moves.
            auto moved = std::move(prepared);
            EXPECT_EQ(moved[0].mReference.getPtr(), ptr);
            EXPECT_EQ(moved[0].mReference.getPtr().getCellRef().getCount(), 2);
        }
        checkUntouched();
        for (int invalid = 0; invalid < 3; ++invalid)
        {
            SCOPED_TRACE(invalid);
            auto bad = savedRing;
            if (invalid == 0)
                bad.mBase = ESM::FormKey::content("missing.esm", 0x101);
            else if (invalid == 1)
                bad.mBase = ESM::FormKey::content("items.esm", 0x999);
            else
                bad.mOwner = ESM::FormKey::content("missing.esm", 0x200);
            EXPECT_THROW(MWWorld::OblivionProfileServices::prepareActorInventory(
                store, resolver, {savedWeapon, bad}), std::runtime_error);
            checkUntouched();
        }
        for (const auto& global : {ESM::FormKey::content("owners.esm", 0x999),
                ESM::FormKey::content("missing.esm", 0x201), weaponKey,
                ESM::FormKey::dynamic("not-a-global", 1)})
        {
            auto bad = savedRing;
            bad.mOwnershipGlobal = global;
            EXPECT_THROW(MWWorld::OblivionProfileServices::prepareActorInventory(
                store, resolver, {savedWeapon, bad}), std::invalid_argument);
            checkUntouched();
        }
        EXPECT_TRUE(MWWorld::OblivionProfileServices::prepareActorInventory(store, resolver, {}).empty());

        struct Listener : MWWorld::InventoryStoreListener, MWWorld::ContainerStoreListener
        {
            int mEquipment = 0;
            int mItems = 0;
            void equipmentChanged() override { ++mEquipment; }
            void itemAdded(const MWWorld::ConstPtr&, int) override { ++mItems; }
            void itemRemoved(const MWWorld::ConstPtr&, int) override { ++mItems; }
        } listener;
        MWWorld::ManualRef owner(store, sharedWeapon.mId);
        model.registerPtr(owner.getPtr());
        live.setPtr(owner.getPtr());
        live.setInvListener(&listener);
        live.setContListener(&listener);
        live.setSelectedEnchantItem(originalItem);
        EXPECT_EQ(live.getWeight(), 12);
        const auto beforeStaging = model.getPtrRegistryRevision();
        const auto beforeSerial = model.getLastGeneratedRefNum();
        auto staged = MWWorld::OblivionProfileServices::stageActorInventory(
            MWWorld::OblivionProfileServices::prepareActorInventory(store, resolver, {savedWeapon, savedRing}));
        ASSERT_EQ(staged->count(sharedWeapon.mId), 2);
        ASSERT_EQ(staged->count(sharedRing.mId), 1);
        EXPECT_TRUE(staged->isResolved());
        EXPECT_EQ(staged->getWeight(), 9);
        EXPECT_EQ(model.getPtrRegistryRevision(), beforeStaging);
        EXPECT_EQ(model.getLastGeneratedRefNum(), beforeSerial);
        const auto held = staged->getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
        ASSERT_NE(held, staged->end());
        EXPECT_EQ(held->getCellRef().getCount(), 1);
        EXPECT_EQ(held->getCellRef().getCharge(), 31);
        EXPECT_EQ(held->getCellRef().getEnchantmentCharge(), 7.5f);
        staged->setSelectedEnchantItem(held);
        const auto heldReference = *held;
        std::vector<MWWorld::Ptr> inserted;
        for (auto ptr : *staged)
        {
            EXPECT_EQ(ptr.mRef->mWorldModel, nullptr);
            ptr.setContainerStore(&live);
            inserted.push_back(ptr);
        }
        // Equipped non-stackable weapons are split before publication.
        ASSERT_EQ(inserted.size(), 3);
        const std::array removed{*originalItem};
        auto registry = model.preparePtrReplacement(removed, inserted);
        EXPECT_EQ(live.count(sharedWeapon.mId), 3);
        EXPECT_EQ(live.count(sharedRing.mId), 0);
        registry.commit();
        live.swapPreparedContents(*staged);
        const auto publishedRevision = model.getPtrRegistryRevision();
        EXPECT_EQ(live.getPtr(), owner.getPtr());
        EXPECT_EQ(live.getInvListener(), &listener);
        EXPECT_EQ(live.getContListener(), &listener);
        EXPECT_TRUE(staged->getPtr().isEmpty());
        EXPECT_EQ(live.count(sharedWeapon.mId), 2);
        EXPECT_EQ(live.count(sharedRing.mId), 1);
        EXPECT_EQ(live.getWeight(), 9);
        EXPECT_EQ(staged->getWeight(), 12);
        EXPECT_TRUE(live.isResolved());
        EXPECT_EQ(live.getSelectedEnchantItem().getContainerStore(), &live);
        EXPECT_EQ(*live.getSelectedEnchantItem(), heldReference);
        EXPECT_EQ(staged->getSelectedEnchantItem().getContainerStore(), staged.get());
        EXPECT_EQ(live.getSlot(MWWorld::InventoryStore::Slot_CarriedRight).getContainerStore(), &live);
        EXPECT_EQ(live.getSlot(MWWorld::InventoryStore::Slot_LeftRing).getContainerStore(), &live);
        for (const auto& ptr : inserted)
        {
            const auto registered = model.getPtr(ptr.getCellRef().getRefNum());
            EXPECT_EQ(registered, ptr);
            EXPECT_EQ(registered.getContainerStore(), &live);
        }
        EXPECT_TRUE(model.getPtr(originalRef).isEmpty());
        staged.reset();
        EXPECT_EQ(model.getPtrRegistryRevision(), publishedRevision);
        EXPECT_EQ(listener.mItems, 0);
        EXPECT_EQ(listener.mEquipment, 0);
        EXPECT_EQ(*live.getSlot(MWWorld::InventoryStore::Slot_CarriedRight), heldReference);
    }

    TEST(OblivionProfileServicesTest, adaptsNativeBootRecordsWithoutACatchAll)
    {
        MWWorld::ESMStore store;

        ESM4::GameSetting gameSetting{};
        gameSetting.mId = ESM::FormId{ 0x10, 0 };
        gameSetting.mEditorId = "fFatigueBase";
        gameSetting.mData = 42.f;
        store.getWritable<ESM4::GameSetting>().insertStatic(gameSetting);
        ESM4::GameSetting walkMax{};
        walkMax.mId = ESM::FormId{ 0x11, 0 };
        walkMax.mEditorId = "fMoveCharWalkMax";
        walkMax.mData = 137.f;
        store.getWritable<ESM4::GameSetting>().insertStatic(walkMax);

        ESM4::GlobalVariable year{};
        year.mId = ESM::FormId{ 0x20, 0 };
        year.mEditorId = "GameYear";
        year.mType = 'l';
        year.mValue = 431.f;
        store.getWritable<ESM4::GlobalVariable>().insertStatic(year);

        ESM4::GlobalVariable daysPassed{};
        daysPassed.mId = ESM::FormId{ 0x21, 0 };
        daysPassed.mEditorId = "GameDaysPassed";
        daysPassed.mType = 'f';
        daysPassed.mValue = 12.5f;
        store.getWritable<ESM4::GlobalVariable>().insertStatic(daysPassed);

        ESM4::Race race{};
        race.mId = ESM::FormId{ 0x907, 0 };
        race.mEditorId = "Imperial";
        race.mFullName = "Native Imperial";
        race.mAttribMale = { 45, 40, 35, 30, 25, 20, 15, 10 };
        race.mAttribFemale = { 40, 45, 35, 30, 25, 20, 15, 10 };
        store.getWritable<ESM4::Race>().insertStatic(race);

        ESM4::Class characterClass{};
        characterClass.mId = ESM::FormId{ 0x30e6, 0 };
        characterClass.mEditorId = "CharactergenClass";
        characterClass.mFullName = "Native Adventurer";
        characterClass.mData.mFavoredAttributes = { 3, 5 };
        characterClass.mData.mSpecialization = 2;
        characterClass.mData.mMajorSkills = { 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20 };
        characterClass.mData.mFlags = 1;
        store.getWritable<ESM4::Class>().insertStatic(characterClass);

        ESM4::BirthSign birthSign{};
        birthSign.mId = ESM::FormId{ 0x1fd94, 0 };
        birthSign.mEditorId = "BirthSignThief";
        birthSign.mFullName = "The Thief";
        birthSign.mDescription = "Fortifies Agility, Speed, and Luck.";
        birthSign.mSpells.push_back(ESM::FormId{ 0x22a46, 0 });
        store.getWritable<ESM4::BirthSign>().insertStatic(birthSign);

        ESM4::Npc player{};
        player.mId = ESM::FormId{ 7, 0 };
        player.mEditorId = "Player";
        player.mFullName = "Native Prisoner";
        player.mModel = "characters/_male/skeleton.nif";
        player.mRace = race.mId;
        player.mClass = characterClass.mId;
        player.mIsTES4 = true;
        player.mBaseConfig.tes4.levelOrOffset = 3;
        player.mBaseConfig.tes4.fatigue = 77;
        player.mData.health = 88;
        player.mData.attribs = { 41, 42, 43, 44, 45, 46, 47, 48 };
        player.mData.skills.armorer = 31;
        player.mData.skills.speechcraft = 52;

        ESM4::Ammunition ammunition{};
        ammunition.mId = ESM::FormId{ 0x100, 0 };
        ammunition.mFullName = "Iron Arrow";
        ammunition.mModel = "weapons/iron/arrow.nif";
        ammunition.mIcon = "icons/iron_arrow.dds";
        ammunition.mData.mValue = 1;
        ammunition.mData.mWeight = 0.1f;
        ammunition.mData.mDamage = 8.f;
        store.getWritable<ESM4::Ammunition>().insertStatic(ammunition);

        ESM4::Apparatus apparatus{};
        apparatus.mId = ESM::FormId{ 0x109, 0 };
        apparatus.mFullName = "Novice Mortar & Pestle";
        apparatus.mData = { 0, 40, 3.f, 0.5f };
        store.getWritable<ESM4::Apparatus>().insertStatic(apparatus);

        ESM4::Armor armor{};
        armor.mId = ESM::FormId{ 0x101, 0 };
        armor.mFullName = "Iron Cuirass";
        armor.mModelMaleWorld = "armor/iron/cuirass_gnd.nif";
        armor.mIconMale = "icons/iron_cuirass.dds";
        armor.mArmorFlags = ESM4::Armor::TES4_UpperBody;
        // Missing ANAM used to expose an indeterminate capacity from native
        // records. ENAM is authoritative; stray points alone are not.
        armor.mEnchantmentPoints = 54321;
        armor.mData = { 12, 100, 250, 30.f };
        store.getWritable<ESM4::Armor>().insertStatic(armor);

        ESM4::Book book{};
        book.mId = ESM::FormId{ 0x102, 0 };
        book.mFullName = "Armorer Manual";
        book.mData = { 0, 0, 0, ESM4::Book::BookSkill_Armorer, 25, 1.f };
        store.getWritable<ESM4::Book>().insertStatic(book);

        ESM4::Clothing clothing{};
        clothing.mId = ESM::FormId{ 0x103, 0 };
        clothing.mFullName = "Copper Ring";
        clothing.mClothingFlags = ESM4::Armor::TES4_RightRing | ESM4::Armor::TES4_LeftRing;
        clothing.mData = { 12, 0.1f };
        store.getWritable<ESM4::Clothing>().insertStatic(clothing);

        ESM4::Ingredient ingredient{};
        ingredient.mId = ESM::FormId{ 0x104, 0 };
        ingredient.mFullName = "Stinkhorn Cap";
        ingredient.mData = { 3, 0.2f };
        store.getWritable<ESM4::Ingredient>().insertStatic(ingredient);

        ESM4::Key key{};
        key.mId = ESM::FormId{ 0x105, 0 };
        key.mFullName = "Market Key";
        key.mData = { 0, 0.f };
        store.getWritable<ESM4::Key>().insertStatic(key);

        ESM4::MiscItem miscellaneous{};
        miscellaneous.mId = ESM::FormId{ 0x106, 0 };
        miscellaneous.mFullName = "Ruby";
        miscellaneous.mData = { 50, 0.2f };
        store.getWritable<ESM4::MiscItem>().insertStatic(miscellaneous);

        ESM4::MiscItem lockpick{};
        lockpick.mId = ESM::FormId{ 0x10d, 0 };
        lockpick.mEditorId = "Lockpick";
        lockpick.mFullName = "Lockpick";
        lockpick.mData = { 3, 0.f };
        store.getWritable<ESM4::MiscItem>().insertStatic(lockpick);

        ESM4::MiscItem repairHammer{};
        repairHammer.mId = ESM::FormId{ 0x10e, 0 };
        repairHammer.mEditorId = "RepairHammer";
        repairHammer.mFullName = "Repair Hammer";
        repairHammer.mData = { 10, 1.f };
        store.getWritable<ESM4::MiscItem>().insertStatic(repairHammer);

        ESM4::Potion potion{};
        potion.mId = ESM::FormId{ 0x107, 0 };
        potion.mFullName = "Restore Health";
        potion.mData.weight = 0.5f;
        potion.mItem.value = 30;
        store.getWritable<ESM4::Potion>().insertStatic(potion);

        ESM4::SigilStone sigilStone{};
        sigilStone.mId = ESM::FormId{ 0x10a, 0 };
        sigilStone.mFullName = "Descendent Sigil Stone";
        sigilStone.mData = { 1, 500, 1.f };
        store.getWritable<ESM4::SigilStone>().insertStatic(sigilStone);

        ESM4::SoulGem soulGem{};
        soulGem.mId = ESM::FormId{ 0x10b, 0 };
        soulGem.mFullName = "Common Soul Gem";
        soulGem.mSoul = 3;
        soulGem.mSoulCapacity = 3;
        soulGem.mData = { 150, 0.4f };
        store.getWritable<ESM4::SoulGem>().insertStatic(soulGem);

        ESM4::Light light{};
        light.mId = ESM::FormId{ 0x10c, 0 };
        light.mFullName = "Torch";
        light.mData.flags = ESM4::Light::Carryable;
        light.mData.time = 1000;
        light.mData.value = 1;
        light.mData.weight = 0.f;
        store.getWritable<ESM4::Light>().insertStatic(light);

        ESM4::Weapon weapon{};
        weapon.mId = ESM::FormId{ 0x108, 0 };
        weapon.mFullName = "Iron Sword";
        weapon.mData.type = 0;
        weapon.mData.value = 25;
        weapon.mData.health = 180;
        weapon.mData.weight = 12.f;
        weapon.mData.damage = 9;
        store.getWritable<ESM4::Weapon>().insertStatic(weapon);

        player.mInventory.push_back({ ammunition.mId.toUint32(), 20 });
        player.mInventory.push_back({ clothing.mId.toUint32(), -1 });
        player.mInventory.push_back({ ingredient.mId.toUint32(), -3 });
        store.getWritable<ESM4::Npc>().insertStatic(player);

        const MWWorld::OblivionProfileInstallReport report = MWWorld::OblivionProfileServices::install(store);

        EXPECT_EQ(report.mNativeGameSettings, 2);
        EXPECT_EQ(report.mNativeGlobals, 2);
        EXPECT_EQ(report.mProjectedItems, 15);
        EXPECT_EQ(report.mPlayerSource, "Player@0x7");
        EXPECT_EQ(report.mRaceSource, "Imperial@0x907");
        EXPECT_EQ(report.mClassSource, "CharactergenClass@0x30e6");

        const auto& settings = store.get<ESM::GameSetting>();
        ASSERT_NE(settings.search("fFatigueBase"), nullptr);
        EXPECT_FLOAT_EQ(settings.find("fFatigueBase")->mValue.getFloat(), 42.f);
        EXPECT_FLOAT_EQ(settings.find("fMaxWalkSpeed")->mValue.getFloat(), 137.f);
        EXPECT_EQ(settings.find("iMaxActivateDist")->mValue.getInteger(), 150);
        EXPECT_FLOAT_EQ(settings.find("fSwimHeightScale")->mValue.getFloat(), 0.75f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSneakBase")->mValue.getFloat(), 1.5f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSneakMult")->mValue.getFloat(), 1.5f);
        EXPECT_EQ(store.get<ESM::Skill>().find(ESM::Skill::LongBlade)->mName, "Blade");
        EXPECT_EQ(store.get<ESM::Skill>().find(ESM::Skill::Mercantile)->mName, "Mercantile");
        EXPECT_TRUE(store.get<ESM::Skill>().find(ESM::Skill::ShortBlade)->mName.empty());
        EXPECT_FLOAT_EQ(settings.find("fFatigueSwimRunBase")->mValue.getFloat(), 7.f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSwimRunMult")->mValue.getFloat(), 0.f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSwimWalkBase")->mValue.getFloat(), 2.5f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSwimWalkMult")->mValue.getFloat(), 0.f);
        EXPECT_FLOAT_EQ(settings.find("fSneakUseDist")->mValue.getFloat(), 500.f);
        EXPECT_FLOAT_EQ(settings.find("fSneakUseDelay")->mValue.getFloat(), 1.f);
        EXPECT_EQ(settings.find("sSkillLongblade")->mValue.getString(), "Blade");
        EXPECT_EQ(settings.find("sSkillHandtohand")->mValue.getString(), "Hand to Hand");
        EXPECT_EQ(settings.find("sAttributeStrength")->mValue.getString(), "Strength");
        EXPECT_EQ(settings.find("sSpecializationStealth")->mValue.getString(), "Stealth");
        EXPECT_EQ(settings.find("sChooseClassMenu3")->mValue.getString(), "Major Skills");
        EXPECT_EQ(settings.find("sChooseClassMenu4")->mValue.getString(), "Major Skills 6-7");
        EXPECT_EQ(settings.find("sBack")->mValue.getString(), "Back");
        EXPECT_EQ(settings.find("sDone")->mValue.getString(), "Done");
        EXPECT_FLOAT_EQ(settings.find("fAudioDefaultMinDistance")->mValue.getFloat(), 5.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioDefaultMaxDistance")->mValue.getFloat(), 40.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioVoiceDefaultMinDistance")->mValue.getFloat(), 10.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioVoiceDefaultMaxDistance")->mValue.getFloat(), 60.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioMinDistanceMult")->mValue.getFloat(), 20.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioMaxDistanceMult")->mValue.getFloat(), 50.f);
        EXPECT_GT(settings.find("fMajorSkillBonus")->mValue.getFloat(), 0.f);
        EXPECT_GT(settings.find("fMinorSkillBonus")->mValue.getFloat(), 0.f);
        EXPECT_GT(settings.find("fMiscSkillBonus")->mValue.getFloat(), 0.f);
        EXPECT_GT(settings.find("fSpecialSkillBonus")->mValue.getFloat(), 0.f);
        EXPECT_EQ(settings.search("fUnreviewedOblivionFallback"), nullptr);

        const auto& globals = store.get<ESM::Global>();
        EXPECT_EQ(globals.find(ESM::RefId::stringRefId("GameYear"))->mValue.getInteger(), 431);
        EXPECT_EQ(globals.find(ESM::RefId::stringRefId("year"))->mValue.getInteger(), 431);
        EXPECT_FLOAT_EQ(globals.find(ESM::RefId::stringRefId("dayspassed"))->mValue.getFloat(), 12.5f);

        const ESM::NPC* adaptedPlayer = store.get<ESM::NPC>().search(ESM::RefId::stringRefId("Player"));
        ASSERT_NE(adaptedPlayer, nullptr);
        EXPECT_EQ(adaptedPlayer->mName, "Native Prisoner");
        // ESM3 NPC model fields are meshes-root-relative; NpcAnimation adds
        // the VFS prefix when selecting the Oblivion skeleton.
        EXPECT_EQ(adaptedPlayer->mModel.getNormalized().value(), "characters/_male/skeleton.nif");
        EXPECT_EQ(adaptedPlayer->mNpdt.mLevel, 3);
        // Initial live stats are projected from the native race/class formula, not the NPC's placeholder DATA.
        EXPECT_EQ(adaptedPlayer->mNpdt.mHealth, 50);
        EXPECT_EQ(adaptedPlayer->mNpdt.mMana, 80);
        EXPECT_EQ(adaptedPlayer->mNpdt.mFatigue, 140);
        EXPECT_EQ(adaptedPlayer->mNpdt.getAttribute(ESM::Attribute::Strength), 45);
        EXPECT_EQ(adaptedPlayer->mNpdt.getAttribute(ESM::Attribute::Agility), 35);
        EXPECT_EQ(adaptedPlayer->mNpdt.getSkill(ESM::Skill::Armorer), 5);
        EXPECT_EQ(adaptedPlayer->mNpdt.getSkill(ESM::Skill::Acrobatics), 30);
        EXPECT_EQ(adaptedPlayer->mNpdt.getSkill(ESM::Skill::Speechcraft), 30);
        ASSERT_EQ(adaptedPlayer->mInventory.mList.size(), 3u);
        EXPECT_EQ(adaptedPlayer->mInventory.mList[0].mItem, ESM::RefId(ammunition.mId));
        EXPECT_EQ(adaptedPlayer->mInventory.mList[0].mCount, 20);
        EXPECT_EQ(adaptedPlayer->mInventory.mList[1].mItem, ESM::RefId(clothing.mId));
        EXPECT_EQ(adaptedPlayer->mInventory.mList[1].mCount, -1);
        EXPECT_EQ(adaptedPlayer->mInventory.mList[2].mItem, ESM::RefId(ingredient.mId));
        EXPECT_EQ(adaptedPlayer->mInventory.mList[2].mCount, -3);

        const ESM::Weapon* adaptedAmmo = store.get<ESM::Weapon>().search(ESM::RefId(ammunition.mId));
        ASSERT_NE(adaptedAmmo, nullptr);
        EXPECT_EQ(adaptedAmmo->mData.mType, ESM::Weapon::Arrow);
        EXPECT_EQ(adaptedAmmo->mData.mChop[0], 8);
        const ESM::Armor* adaptedArmor = store.get<ESM::Armor>().search(ESM::RefId(armor.mId));
        ASSERT_NE(adaptedArmor, nullptr);
        EXPECT_EQ(adaptedArmor->mData.mType, ESM::Armor::Cuirass);
        EXPECT_EQ(adaptedArmor->mData.mHealth, 250);
        EXPECT_EQ(adaptedArmor->mData.mEnchant, 0);
        EXPECT_EQ(store.get<ESM::Book>().find(ESM::RefId(book.mId))->mData.mSkillId, ESM::Skill::Armorer);
        EXPECT_EQ(store.get<ESM::Clothing>().find(ESM::RefId(clothing.mId))->mData.mType, ESM::Clothing::Ring);
        EXPECT_FLOAT_EQ(store.get<ESM::Ingredient>().find(ESM::RefId(ingredient.mId))->mData.mWeight, 0.2f);
        EXPECT_EQ(store.get<ESM::Miscellaneous>().find(ESM::RefId(key.mId))->mData.mFlags,
            ESM::Miscellaneous::Key);
        EXPECT_EQ(store.get<ESM::Miscellaneous>().find(ESM::RefId(miscellaneous.mId))->mData.mValue, 50);
        EXPECT_EQ(store.get<ESM::Lockpick>().find(ESM::RefId(lockpick.mId))->mData.mUses, 1);
        EXPECT_EQ(store.get<ESM::Repair>().find(ESM::RefId(repairHammer.mId))->mData.mUses, 1);
        EXPECT_EQ(store.get<ESM::Potion>().find(ESM::RefId(potion.mId))->mData.mValue, 30);
        EXPECT_EQ(store.get<ESM::Weapon>().find(ESM::RefId(weapon.mId))->mData.mHealth, 180);
        EXPECT_FLOAT_EQ(store.get<ESM::Apparatus>().find(ESM::RefId(apparatus.mId))->mData.mQuality, 0.5f);
        EXPECT_EQ(store.get<ESM::Miscellaneous>().find(ESM::RefId(sigilStone.mId))->mData.mValue, 500);
        EXPECT_EQ(store.get<ESM::Miscellaneous>().find(ESM::RefId(soulGem.mId))->mData.mValue, 150);
        EXPECT_EQ(store.get<ESM::Light>().find(ESM::RefId(light.mId))->mData.mTime, 1000);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(ammunition.mId)),
            ESM::REC_WEAP);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(apparatus.mId)),
            ESM::REC_APPA);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(armor.mId)), ESM::REC_ARMO);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(book.mId)), ESM::REC_BOOK);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(clothing.mId)), ESM::REC_CLOT);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(ingredient.mId)),
            ESM::REC_INGR);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(key.mId)), ESM::REC_MISC);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(light.mId)), ESM::REC_LIGH);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(miscellaneous.mId)),
            ESM::REC_MISC);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(lockpick.mId)), ESM::REC_LOCK);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(repairHammer.mId)),
            ESM::REC_REPA);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(potion.mId)), ESM::REC_ALCH);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(sigilStone.mId)),
            ESM::REC_MISC);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(soulGem.mId)), ESM::REC_MISC);
        EXPECT_EQ(MWWorld::OblivionProfileServices::sharedItemType(store, ESM::RefId(weapon.mId)), ESM::REC_WEAP);

        const auto armorDefinition
            = MWWorld::OblivionProfileServices::itemDefinition(store, ESM::RefId(armor.mId));
        ASSERT_TRUE(armorDefinition.has_value());
        EXPECT_EQ(armorDefinition->mType, ESM4::InventoryItemType::Armor);
        EXPECT_EQ(armorDefinition->mSlots, ESM4::Armor::TES4_UpperBody);
        EXPECT_EQ(armorDefinition->mMaxCondition, 250);
        EXPECT_EQ(armorDefinition->mMaxCharge, -1.f);
        const auto ringDefinition
            = MWWorld::OblivionProfileServices::itemDefinition(store, ESM::RefId(clothing.mId));
        ASSERT_TRUE(ringDefinition.has_value());
        EXPECT_TRUE(ringDefinition->mChooseOneSlot);
        const auto lightDefinition
            = MWWorld::OblivionProfileServices::itemDefinition(store, ESM::RefId(light.mId));
        ASSERT_TRUE(lightDefinition.has_value());
        EXPECT_EQ(lightDefinition->mType, ESM4::InventoryItemType::Light);
        EXPECT_EQ(lightDefinition->mSlots, ESM4::InventorySlotLight);
        EXPECT_FLOAT_EQ(lightDefinition->mMaxUsageTime, 1000.f);
        EXPECT_TRUE(MWWorld::OblivionProfileServices::itemDefinition(store, ESM::RefId(soulGem.mId))->mConsumable);

        EXPECT_EQ(store.get<ESM::Race>().find(adaptedPlayer->mRace)->mName, "Native Imperial");
        const ESM::Class* adaptedClass = store.get<ESM::Class>().find(adaptedPlayer->mClass);
        EXPECT_EQ(adaptedClass->mName, "Native Adventurer");
        EXPECT_EQ(adaptedClass->mData.mAttribute[0], ESM::Attribute::Agility);
        EXPECT_EQ(adaptedClass->mData.mAttribute[1], ESM::Attribute::Endurance);
        EXPECT_EQ(adaptedClass->mData.mSpecialization, ESM::Class::Stealth);
        EXPECT_EQ(adaptedClass->mData.mSkills[0][1], ESM::Skill::Acrobatics);
        EXPECT_EQ(adaptedClass->mData.mSkills[0][0], ESM::Skill::Sneak);
        const ESM::BirthSign* adaptedSign = store.get<ESM::BirthSign>().find(ESM::RefId(birthSign.mId));
        EXPECT_EQ(adaptedSign->mName, "The Thief");
        ASSERT_EQ(adaptedSign->mPowers.mList.size(), 1u);
        EXPECT_EQ(adaptedSign->mPowers.mList.front(), ESM::RefId(birthSign.mSpells.front()));
    }
}
