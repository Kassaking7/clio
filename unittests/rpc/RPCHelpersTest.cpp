//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <rpc/RPCHelpers.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

#include <variant>

using namespace RPC;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto INDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr static auto TXNID = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

class RPCHelpersTest : public MockBackendTest, public SyncAsioContextTest
{
    void
    SetUp() override
    {
        MockBackendTest::SetUp();
        SyncAsioContextTest::SetUp();
    }
    void
    TearDown() override
    {
        MockBackendTest::TearDown();
        SyncAsioContextTest::TearDown();
    }
};

TEST_F(RPCHelpersTest, TraverseOwnedNodesNotAccount)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // fetch account object return emtpy
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto account = GetAccountIDWithString(ACCOUNT);
        auto ret = traverseOwnedNodes(*mockBackendPtr, account, 9, 10, "", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, RippledError::rpcACT_NOT_FOUND);
    });
    ctx.run();
}

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidIndexNotHex)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto account = GetAccountIDWithString(ACCOUNT);
        auto ret = traverseOwnedNodes(*mockBackendPtr, account, 9, 10, "nothex,10", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Malformed cursor");
    });
    ctx.run();
}

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidPageNotInt)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto account = GetAccountIDWithString(ACCOUNT);
        auto ret = traverseOwnedNodes(*mockBackendPtr, account, 9, 10, "nothex,abc", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Malformed cursor");
    });
    ctx.run();
}

// limit = 10, return 2 objects
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarker)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    // return owner index
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx, [this, &account](boost::asio::yield_context yield) {
        auto ret = traverseOwnedNodes(*mockBackendPtr, account, 9, 10, {}, yield, [](auto) {

        });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(
            cursor->toString(),
            "0000000000000000000000000000000000000000000000000000000000000000,"
            "0");
    });
    ctx.run();
}

// limit = 10, return 10 objects and marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnSamePageMarker)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 11;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0)
    {
        // return owner index
        indexes.push_back(ripple::uint256{INDEX1});
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx, [this, &account](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*mockBackendPtr, account, 9, 10, {}, yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, 10);
        EXPECT_EQ(cursor->toString(), fmt::format("{},0", INDEX1));
    });
    ctx.run();
}

// 10 objects per page, limit is 15, return the second page as marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnOtherPageMarker)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto ownerDirKk = ripple::keylet::ownerDir(account).key;
    constexpr static auto nextPage = 99;
    constexpr static auto limit = 15;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), nextPage).key;

    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0)
    {
        // return owner index
        indexes.push_back(ripple::uint256{INDEX1});
        objectsCount--;
    }
    objectsCount = 15;
    while (objectsCount != 0)
    {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, nextPage);
    // first page 's next page is 99
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ripple::STObject ownerDir2 = CreateOwnerDirLedgerObject(indexes, INDEX1);
    // second page's next page is 0
    ownerDir2.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*mockBackendPtr, account, 9, limit, {}, yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, limit);
        EXPECT_EQ(cursor->toString(), fmt::format("{},{}", INDEX1, nextPage));
    });
    ctx.run();
}

// Send a valid marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithMarkerReturnSamePageMarker)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), 99).key;
    constexpr static auto limit = 8;
    constexpr static auto pageNum = 99;
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0)
    {
        // return owner index
        indexes.push_back(ripple::uint256{INDEX1});
        objectsCount--;
    }
    objectsCount = 10;
    while (objectsCount != 0)
    {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *mockBackendPtr, account, 9, limit, fmt::format("{},{}", INDEX1, pageNum), yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, limit);
        EXPECT_EQ(cursor->toString(), fmt::format("{},{}", INDEX1, pageNum));
    });
    ctx.run();
}

// Send a valid marker, but marker contain an unexisting index
// return empty
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithUnexistingIndexMarker)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), 99).key;
    constexpr static auto limit = 8;
    constexpr static auto pageNum = 99;
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    int objectsCount = 10;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0)
    {
        // return owner index
        indexes.push_back(ripple::uint256{INDEX1});
        objectsCount--;
    }
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    boost::asio::spawn(ctx, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *mockBackendPtr, account, 9, limit, fmt::format("{},{}", INDEX2, pageNum), yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, 0);
        EXPECT_EQ(
            cursor->toString(),
            "00000000000000000000000000000000000000000000000000000000000000"
            "00,0");
    });
    ctx.run();
}
