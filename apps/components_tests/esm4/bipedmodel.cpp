#include <components/esm4/bipedmodel.hpp>
#include <components/esm4/crimerules.hpp>
#include <gtest/gtest.h>
#include <stdexcept>

TEST(ESM4BipedModel, NativeSexAndEmptyRoleTableReturnsTheSelectedBorrowedPath)
{
    // Immutable outputs from original469330/4692E0. No male->female fallback.
    struct Row { const char* male; const char* female; bool isFemale; const char* expected; };
    const Row rows[] = {
        {"male.nif","female.nif",false,"male.nif"}, {"male.nif","female.nif",true,"female.nif"},
        {"","female.nif",false,""}, {"","female.nif",true,"female.nif"},
        {"male.nif","",false,"male.nif"}, {"male.nif","",true,"male.nif"},
        {"","",false,""}, {"","",true,""}};
    for (const auto& r : rows)
    {
        ESM::Path male, female; male = r.male; female = r.female;
        const auto& selected = ESM4::bipedModelForSex(male,female,r.isFemale);
        EXPECT_EQ(selected.getOriginal(),r.expected);
        EXPECT_TRUE(&selected == &male || &selected == &female);
    }
}

TEST(ESM4BipedModel, OwnerLayerSelectionPreservesReferenceKindInheritance)
{
    using K = ESM4::OwnershipReferenceKind;
    EXPECT_EQ(ESM4::ownershipOwnerLayer(K::Other,{true,true,true}),0u);
    EXPECT_EQ(ESM4::ownershipOwnerLayer(K::Other,{false,true,true}),1u);
    EXPECT_EQ(ESM4::ownershipOwnerLayer(K::Other,{false,false,true}),2u);
    EXPECT_FALSE(ESM4::ownershipOwnerLayer(K::Other,{false,false,false}));
    EXPECT_FALSE(ESM4::ownershipOwnerLayer(K::Actor,{false,true,true}));
    for (auto kind : {K::Furniture,K::Door,K::Activator})
    {
        EXPECT_EQ(ESM4::ownershipOwnerLayer(kind,{false,true,true}),1u);
        EXPECT_FALSE(ESM4::ownershipOwnerLayer(kind,{false,false,true}));
    }
    EXPECT_THROW(ESM4::ownershipOwnerLayer(static_cast<K>(99),{true,true,true}),std::invalid_argument);
}
