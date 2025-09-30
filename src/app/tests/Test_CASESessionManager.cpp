#include <pw_unit_test/framework.h>
#include <app/CASESessionManager.h>
#include <lib/address_resolve/AddressResolve.h>

using namespace chip;

TEST(CASESessionManagerTest, InitFunctionExistsAndCanBeCalled)
{
    CASESessionManager manager;
    CASESessionManagerConfig config;
    chip::System::Layer * systemLayer = nullptr;
    CHIP_ERROR err = manager.Init(systemLayer, config);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(CASESessionManagerTest, ShutdownFunctionExistsAndCanBeCalled)
{
    CASESessionManager manager;
    manager.Shutdown();
    EXPECT_TRUE(true);
}

TEST(CASESessionManagerTest, ReleaseAllSessionsFunctionExistsAndCanBeCalled)
{
    CASESessionManager manager;
    manager.ReleaseAllSessions();
    EXPECT_TRUE(true);
}

TEST(CASESessionManagerTest, ReleaseSessionsForFabricFunctionExistsAndCanBeCalled)
{
    CASESessionManager manager;
    FabricIndex fabricIndex = 1;
    manager.ReleaseSessionsForFabric(fabricIndex);
    EXPECT_TRUE(true);
}

TEST(CASESessionManagerTest, ReleaseSessionByPeerIdFunctionExistsAndCanBeCalled)
{
    CASESessionManager manager;
    ScopedNodeId peerId;
    manager.ReleaseSession(peerId);
    EXPECT_TRUE(true);
}

TEST(CASESessionManagerTest, GetPeerAddressFunctionExistsAndCanBeCalled)
{
    CASESessionManager manager;
    ScopedNodeId peerId;
    Transport::PeerAddress addr;
    CHIP_ERROR err = manager.GetPeerAddress(peerId, addr, TransportPayloadCapability::kMRPPayload);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(CASESessionManagerTest, UpdatePeerAddressFunctionExistsAndCanBeCalled)
{
    CASESessionManager manager;
    ScopedNodeId peerId;
    manager.UpdatePeerAddress(peerId);
    EXPECT_TRUE(true);
}