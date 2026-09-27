#include <components/nif/data.hpp>
#include <components/nif/niffile.hpp>
#include <components/nif/nifstream.hpp>

#include <gtest/gtest.h>

#include <sstream>

TEST(NifSkinPartition, ReadsAndMovesPartitionsWithVersionDependentFields)
{
    for (const unsigned int bethVersion : { 11u, 34u, 83u })
    {
        SCOPED_TRACE(bethVersion);
        Nif::NIFFile file(VFS::Path::NormalizedView("partition-test"));
        file.mVersion = Nif::NIFFile::VER_OB;
        file.mBethVersion = bethVersion;
        Nif::Reader reader(file, nullptr);
        // Two empty partitions, each with five 16-bit counts and four presence flags.
        std::string bytes;
        for (int i = 0; i < 2; ++i)
        {
            bytes.append(14, '\0');
            if (bethVersion > Nif::NIFFile::BETHVER_FO3)
            {
                bytes.push_back(7);
                bytes.push_back(1);
            }
        }
        Nif::NIFStream stream(reader, std::make_unique<std::istringstream>(bytes), nullptr);
        std::vector<Nif::NiSkinPartition::Partition> partitions;
        stream.readVectorOfRecords(2u, partitions);
        ASSERT_EQ(partitions.size(), 2u);
        for (const auto& partition : partitions)
        {
            EXPECT_EQ(partition.mLODLevel, bethVersion > Nif::NIFFile::BETHVER_FO3 ? 7 : 0);
            EXPECT_EQ(partition.mGlobalVB, bethVersion > Nif::NIFFile::BETHVER_FO3);
            EXPECT_EQ(partition.mVertexDesc.mFlags, 0);
            EXPECT_TRUE(partition.mBones.empty());
            EXPECT_TRUE(partition.mTriangles.empty());
        }
    }
}
