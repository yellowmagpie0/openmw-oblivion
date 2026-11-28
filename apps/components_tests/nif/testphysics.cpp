#include <components/nif/controller.hpp>
#include <components/nif/physics.hpp>
#include <components/nif/suspendedtrap.hpp>
#include "node.hpp"

#include <gtest/gtest.h>

TEST(ESM4SuspendedTrap, UsesConnectedBodiesAndPreservesAuthoredMotion)
{
    using namespace Nif;
    NIFFile file(VFS::Path::NormalizedView("custom.nif"));
    auto sequence = std::make_unique<NiControllerSequence>();
    auto& release = *sequence;
    file.mRecords.push_back(std::move(sequence));
    release.mName = "Unequip";
    release.mStopTime = 2.f;
    release.mControlledBlocks.emplace_back().mNodeName = "Catch";

    NiNode anchor, pivot, head;
    auto volume = std::make_unique<NiNode>();
    std::array<bhkCollisionObject, 4> collisions;
    bhkRigidBody fixed{}, link{}, weight{};
    bhkSimpleShapePhantom damage{};
    bhkConvexVerticesShape hull;
    damage.mShape = &hull;
    hull.mVertices.emplace_back(2, 0, 0, 0);
    damage.mTransform(3, 3) = 0; // Padding in the serialized Havok transform.
    damage.mHavokFilter.mLayer = 14;
    fixed.mInfo.mMotionType = HkMotionType::Motion_Fixed;
    link.mInfo.mMass = weight.mInfo.mMass = 10;
    link.mInfo.mMotionType = weight.mInfo.mMotionType = HkMotionType::Motion_Dynamic;
    std::array<NiNode*, 4> nodes{ &anchor, &pivot, &head, volume.get() };
    std::array<bhkWorldObject*, 4> bodies{ &fixed, &link, &weight, &damage };
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        Nif::Testing::init(*nodes[i]);
        nodes[i]->mCollision = &collisions[i];
        collisions[i].mBody = bodies[i];
        if (i > 0)
            nodes[i]->mParents.push_back(nodes[i - 1]);
    }
    file.mRecords.push_back(std::move(volume));
    pivot.mName = "Fulcrum";
    head.mName = "Weight";
    head.mTransform.mTranslation.set(40, 0, 20);
    bhkRagdollConstraint upper, lower;
    link.mConstraints.emplace_back(&upper);
    weight.mConstraints.emplace_back(&lower);
    upper.mInfo.mEntityA = &link;
    upper.mInfo.mEntityB = &fixed;
    lower.mInfo.mEntityA = &weight;
    lower.mInfo.mEntityB = &fixed;
    animateSuspendedTrap(file.mRecords);
    EXPECT_EQ(release.mControlledBlocks.size(), 1); // Disconnected from the render parent.

    lower.mInfo.mEntityB = &link;
    release.mControlledBlocks.front().mNodeName = head.mName;
    animateSuspendedTrap(file.mRecords);
    EXPECT_EQ(release.mControlledBlocks.size(), 1);

    release.mControlledBlocks.front().mNodeName = "Catch";
    animateSuspendedTrap(file.mRecords);
    ASSERT_EQ(release.mControlledBlocks.size(), 3);
    EXPECT_EQ(release.mControlledBlocks[1].mNodeName, pivot.mName);
    EXPECT_EQ(release.mControlledBlocks[2].mNodeName, head.mName);
    const auto* interp
        = dynamic_cast<const NiTransformInterpolator*>(release.mControlledBlocks[1].mInterpolator.getPtr());
    ASSERT_NE(interp, nullptr);
    const auto& keys = interp->mData->mRotations->mKeys;
    ASSERT_GT(keys.size(), 1);
    EXPECT_EQ(keys.front().first, 2.f);
    EXPECT_EQ(keys.back().first, release.mStopTime);
    EXPECT_NE(keys.front().second.mValue, keys.back().second.mValue);
}

TEST(ESM4CreatureModelEligibility, UsesOnlyRootAndThreeFirstChildren)
{
    for (unsigned mask = 0; mask < 64; ++mask)
    {
        SCOPED_TRACE(mask);
        std::array<Nif::NiNode, 6> nodes;
        Nif::bhkBlendCollisionObject collision;
        for (unsigned i = 0; i < nodes.size(); ++i)
        {
            Nif::Testing::init(static_cast<Nif::NiAVObject&>(nodes[i]));
            if (mask & (1u << i)) nodes[i].mCollision = Nif::NiCollisionObjectPtr(&collision);
            if (i + 1 < nodes.size()) nodes[i].mChildren.emplace_back(&nodes[i + 1]);
        }
        EXPECT_EQ(Nif::tes4CreatureHasFatigueCollision(&nodes.front()), (mask & 15) != 0);
    }
}

TEST(ESM4CreatureModelEligibility, IgnoresSiblingsAndGenericCollision)
{
    Nif::NiNode root, first, second;
    for (auto* node : {&root, &first, &second})
        Nif::Testing::init(static_cast<Nif::NiAVObject&>(*node));
    Nif::NiCollisionObject generic;
    Nif::bhkCollisionObject havok;
    root.mCollision = Nif::NiCollisionObjectPtr(&generic);
    second.mCollision = Nif::NiCollisionObjectPtr(&havok);
    root.mChildren = {Nif::NiAVObjectPtr(&first), Nif::NiAVObjectPtr(&second)};
    EXPECT_FALSE(Nif::tes4CreatureHasFatigueCollision(&root));
    root.mChildren.front() = Nif::NiAVObjectPtr(nullptr);
    EXPECT_FALSE(Nif::tes4CreatureHasFatigueCollision(&root));
    root.mChildren.front() = Nif::NiAVObjectPtr(&second);
    EXPECT_TRUE(Nif::tes4CreatureHasFatigueCollision(&root));
}

TEST(ESM4CreatureModelEligibility, HandlesMissingRootLeafAndCycle)
{
    EXPECT_FALSE(Nif::tes4CreatureHasFatigueCollision(nullptr));
    Nif::NiTriShape leaf;
    Nif::Testing::init(leaf);
    EXPECT_FALSE(Nif::tes4CreatureHasFatigueCollision(&leaf));
    Nif::bhkCollisionObject havok;
    leaf.mCollision = Nif::NiCollisionObjectPtr(&havok);
    EXPECT_TRUE(Nif::tes4CreatureHasFatigueCollision(&leaf));
    Nif::NiNode root;
    Nif::Testing::init(static_cast<Nif::NiAVObject&>(root));
    root.mChildren.emplace_back(&root);
    EXPECT_FALSE(Nif::tes4CreatureHasFatigueCollision(&root));
}

namespace Nif
{
    namespace
    {
        using namespace testing;

        constexpr VFS::Path::NormalizedView path("test");

        void writeUInt8(std::uint8_t value, std::ostream& stream)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        void writeUInt16(std::uint16_t value, std::ostream& stream)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        void writeUInt32(std::uint32_t value, std::ostream& stream)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        void writeString(std::string_view value, std::ostream& stream)
        {
            stream.write(value.data(), value.size());
        }

        void writeUInt8SizedString(std::string_view value, std::ostream& stream)
        {
            writeUInt8(static_cast<std::uint8_t>(value.size() + 1), stream);
            writeString(value, stream);
            stream.put(0);
        }

        void writeUInt32SizedString(std::string_view value, std::ostream& stream)
        {
            writeUInt32(static_cast<std::uint32_t>(value.size() + 1), stream);
            writeString(value, stream);
            stream.put(0);
        }

        void writeFloat(float value, std::ostream& stream)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        struct NifBhkRagdollTemplateTest : Test
        {
            NifBhkRagdollTemplateTest() { Nif::Reader::setWriteNifDebugLog(true); }
        };

        TEST_F(NifBhkRagdollTemplateTest, shouldParseRecord)
        {
            NIFFile file(path);
            const ToUTF8::StatelessUtf8Encoder* const encoder = nullptr;
            Reader reader(file, encoder);

            std::ostringstream stream;

            writeString("Gamebryo File Format, Version 20.2.0.7\n", stream);
            writeUInt32(NIFStream::generateVersion(20, 2, 0, 7), stream);

            constexpr std::uint8_t endianness = 1;
            writeUInt8(endianness, stream);

            constexpr std::uint32_t userVersion = 11;
            writeUInt32(userVersion, stream);

            constexpr std::uint32_t recordsCount = 2;
            writeUInt32(recordsCount, stream);

            constexpr std::uint32_t bsStreamVersion = 34;
            writeUInt32(bsStreamVersion, stream);

            writeUInt8SizedString("author", stream);
            writeUInt8SizedString("process_script", stream);
            writeUInt8SizedString("export_script", stream);

            constexpr std::uint16_t recordTypesCount = 2;
            writeUInt16(recordTypesCount, stream);

            constexpr std::uint16_t bhkRagdollTemplateTypeIndex = 0;
            writeUInt32SizedString("bhkRagdollTemplate", stream);

            constexpr std::uint16_t bhkRagdollTemplateDataTypeIndex = 1;
            writeUInt32SizedString("bhkRagdollTemplateData", stream);

            writeUInt16(bhkRagdollTemplateTypeIndex, stream);
            writeUInt16(bhkRagdollTemplateDataTypeIndex, stream);

            for (std::uint32_t i = 0; i < recordsCount; ++i)
            {
                constexpr std::uint32_t recordSize = 0;
                writeUInt32(recordSize, stream);
            }

            constexpr std::uint32_t stringsCount = 2;
            writeUInt32(stringsCount, stream);

            constexpr std::uint32_t maxStringSize = 0;
            writeUInt32(maxStringSize, stream);

            constexpr std::uint32_t bhkRagdollTemplateNameIndex = 0;
            writeUInt32SizedString("bhkRagdollTemplateName", stream);

            constexpr std::uint32_t bhkRagdollTemplateDataNameIndex = 1;
            writeUInt32SizedString("bhkRagdollTemplateDataName", stream);

            constexpr std::uint32_t groupsCount = 0;
            writeUInt32(groupsCount, stream);

            writeUInt32(bhkRagdollTemplateNameIndex, stream);

            constexpr std::uint32_t bonesCount = 1;
            writeUInt32(bonesCount, stream);

            constexpr std::uint32_t bhkRagdollTemplateDataIndex = 1;
            writeUInt32(bhkRagdollTemplateDataIndex, stream);

            writeUInt32(bhkRagdollTemplateDataNameIndex, stream);

            constexpr float mass = 1.1f;
            writeFloat(mass, stream);

            constexpr float restitution = 2.2f;
            writeFloat(restitution, stream);

            constexpr float friction = 3.3f;
            writeFloat(friction, stream);

            constexpr float radius = 4.4f;
            writeFloat(radius, stream);

            constexpr std::uint32_t havokMaterial = 42;
            writeUInt32(havokMaterial, stream);

            constexpr std::uint32_t constraintsCount = 0;
            writeUInt32(constraintsCount, stream);

            constexpr std::uint32_t rootsCount = 0;
            writeUInt32(rootsCount, stream);

            const std::string buffer = stream.str();

            std::unique_ptr<std::istringstream> input = std::make_unique<std::istringstream>(buffer);
            input->exceptions(std::ios::failbit | std::ios_base::badbit);

            reader.parse(std::move(input));

            {
                const Record* const record = reader.getRecord(0);
                ASSERT_NE(record, nullptr);
                EXPECT_EQ(record->mRecordIndex, 0);
                EXPECT_EQ(record->mRecordName, "bhkRagdollTemplate");
                EXPECT_EQ(record->mRecordType, RC_bhkRagdollTemplate);

                const bhkRagdollTemplate* const typed = dynamic_cast<const bhkRagdollTemplate*>(record);
                ASSERT_NE(typed, nullptr);
                EXPECT_EQ(typed->mName, "bhkRagdollTemplateName");
                EXPECT_EQ(typed->mBones.size(), 1);
                EXPECT_NE(typed->mBones[0].getPtr(), nullptr);
            }

            {
                const Record* const record = reader.getRecord(1);
                ASSERT_NE(record, nullptr);
                EXPECT_EQ(record->mRecordIndex, 1);
                EXPECT_EQ(record->mRecordName, "bhkRagdollTemplateData");
                EXPECT_EQ(record->mRecordType, RC_bhkRagdollTemplateData);

                const bhkRagdollTemplateData* const typed = dynamic_cast<const bhkRagdollTemplateData*>(record);
                ASSERT_NE(typed, nullptr);
                EXPECT_EQ(typed->mName, "bhkRagdollTemplateDataName");
                EXPECT_EQ(typed->mMass, mass);
                EXPECT_EQ(typed->mRestitution, restitution);
                EXPECT_EQ(typed->mFriction, friction);
                EXPECT_EQ(typed->mRadius, radius);
                EXPECT_EQ(typed->mHavokMaterial.mMaterial, havokMaterial);
                EXPECT_EQ(typed->mConstraints.size(), 0);
            }
        }
    }
}
