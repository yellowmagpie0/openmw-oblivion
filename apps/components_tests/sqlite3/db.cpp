#include <components/files/conversion.hpp>
#include <components/sqlite3/transaction.hpp>
#include <format>
#include <components/testing/util.hpp>
#include <sqlite3.h>
#include <chrono>
#include <cstdlib>
#include <future>
#include <components/sqlite3/db.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace Sqlite3;

    TEST(Sqlite3DbTest, makeDbShouldCreateInMemoryDbWithSchema)
    {
        const auto db = makeDb(":memory:", "CREATE TABLE test ( id INTEGER )");
        EXPECT_NE(db, nullptr);
    }
    TEST(Sqlite3DbTest, competingConnectionShouldWaitForWriteTransaction)
    {
        const auto path = Files::pathToUnicodeString(TestingOpenMW::currentTestDirPath() / "cache.db");
        auto first = makeDb(path, "CREATE TABLE test (id INTEGER)");
        Transaction write(*first, TransactionMode::Immediate);
        std::promise<void> started;
        auto opening = std::async(std::launch::async, [&] {
            started.set_value();
            return makeDb(path, "INSERT INTO test VALUES (2)");
        });
        started.get_future().wait();
        EXPECT_EQ(opening.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout);
        write.commit();
        auto second = opening.get();
        int count = 0;
        EXPECT_EQ(sqlite3_exec(second.get(), "SELECT count(*) FROM test",
            [](void* value, int, char** columns, char**) {
                *static_cast<int*>(value) = std::atoi(columns[0]);
                return 0;
            }, &count, nullptr), SQLITE_OK);
        EXPECT_EQ(count, 1);
    }

}
