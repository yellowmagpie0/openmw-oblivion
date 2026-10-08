#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loaddial.hpp>

#include <gtest/gtest.h>

#include <map>
#include <sstream>

namespace ESM
{
    namespace
    {
        TEST(Esm3CStringIdTest, unmappedReferencesRetainOriginalIdentityWithoutChangingLaterReads)
        {
            ESMWriter writer;
            writer.setFormatVersion(CurrentSaveGameFormatVersion);
            auto stream = std::make_unique<std::stringstream>();
            writer.save(*stream);
            writer.startRecord(fourCC("TEST"));
            for (const auto id : {FormId{0x881, 0}, FormId{0x881, 0}, FormId{0x882, 1}, FormId{0x882, 1}})
                writer.writeHNRefId("NAME", RefId(id));
            writer.endRecord(fourCC("TEST"));
            ESMReader reader; reader.open(std::move(stream), "unmapped-reference-fixture");
            ASSERT_EQ(reader.getRecName(), fourCC("TEST")); reader.getRecHeader();
            const std::map<int, int> mapping{{0, 3}};
            reader.setContentFileMapping(&mapping);
            ASSERT_TRUE(reader.isNextSub("NAME"));
            EXPECT_EQ(reader.getUnmappedRefId(), RefId(FormId{0x881, 0}));
            ASSERT_TRUE(reader.isNextSub("NAME"));
            EXPECT_EQ(reader.getRefId(), RefId(FormId{0x881, 3}));
            ASSERT_TRUE(reader.isNextSub("NAME"));
            EXPECT_EQ(reader.getUnmappedRefId(), RefId(FormId{0x882, 1}));
            ASSERT_TRUE(reader.isNextSub("NAME"));
            EXPECT_TRUE(reader.getRefId().empty());
            EXPECT_FALSE(reader.hasMoreSubs());
        }

        TEST(Esm3CStringIdTest, unmappedStringReferencesKeepLegacyAndTypedEncodingBehavior)
        {
            for (const auto format : {MaxStringRefIdFormatVersion, CurrentSaveGameFormatVersion})
            {
                SCOPED_TRACE(format);
                ESMWriter writer; writer.setFormatVersion(format);
                auto stream = std::make_unique<std::stringstream>(); writer.save(*stream);
                const auto id = RefId::stringRefId("Saved Region");
                writer.startRecord(fourCC("TEST"));
                writer.writeHNRefId("NAME", id); writer.writeHNRefId("NAME", id);
                writer.endRecord(fourCC("TEST"));
                ESMReader reader; reader.open(std::move(stream), "unmapped-string-fixture");
                ASSERT_EQ(reader.getRecName(), fourCC("TEST")); reader.getRecHeader();
                const std::map<int, int> mapping{}; reader.setContentFileMapping(&mapping);
                ASSERT_TRUE(reader.isNextSub("NAME")); EXPECT_EQ(reader.getUnmappedRefId(), id);
                ASSERT_TRUE(reader.isNextSub("NAME")); EXPECT_EQ(reader.getRefId(), id);
                EXPECT_FALSE(reader.hasMoreSubs());
            }
        }

        TEST(Esm3CStringIdTest, dialNameShouldBeNullTerminated)
        {
            std::unique_ptr<std::istream> stream;

            {
                auto ostream = std::make_unique<std::stringstream>();

                ESMWriter writer;
                writer.setFormatVersion(DefaultFormatVersion);
                writer.save(*ostream);

                Dialogue record;
                record.blank();
                record.mStringId = "topic name";
                record.mId = RefId::stringRefId(record.mStringId);
                record.mType = Dialogue::Topic;
                writer.startRecord(Dialogue::sRecordId);
                record.save(writer);
                writer.endRecord(Dialogue::sRecordId);

                stream = std::move(ostream);
            }

            ESMReader reader;
            reader.open(std::move(stream), "stream");
            ASSERT_TRUE(reader.hasMoreRecs());
            ASSERT_EQ(reader.getRecName(), Dialogue::sRecordId);
            reader.getRecHeader();
            while (reader.hasMoreSubs())
            {
                reader.getSubName();
                if (reader.retSubName().toInt() == SREC_NAME)
                {
                    reader.getSubHeader();
                    auto size = reader.getSubSize();
                    std::string buffer(size, '1');
                    reader.getExact(buffer.data(), size);
                    ASSERT_EQ(buffer[size - 1], '\0');
                    return;
                }
                else
                    reader.skipHSub();
            }
            ASSERT_FALSE(true);
        }
    }
}
