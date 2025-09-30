#include <app/CASESessionManager.h>
#include <lib/address_resolve/AddressResolve.h>
#include <lib/core/StringBuilderAdapters.h>
#include <pw_unit_test/framework.h>
#include <app/data-model/Nullable.h>

using namespace chip;

// Minimal stub classes to allow construction and Init
namespace {
class DummyReliableMessageMgr : public ReliableMessageMgr
{
public:
    void RegisterSessionUpdateDelegate(SessionUpdateDelegate *) override {}
};

class DummyExchangeMgr : public Messaging::ExchangeManager
{
public:
    DummyReliableMessageMgr mReliableMessageMgr;
    ReliableMessageMgr * GetReliableMessageMgr() override { return &mReliableMessageMgr; }
};

class DummySessionManager : public SessionManager
{
public:
    Optional<SessionHandle> FindSecureSessionForNode(const ScopedNodeId &, Optional<Transport::SecureSession::Type>, TransportPayloadCapability) override
    {
        return NullOptional;
    }
};

class DummyCASEClientPool : public CASEClientPoolDelegate
{
};

class DummyOperationalSessionSetupPool : public OperationalSessionSetupPoolDelegate
{
public:
    OperationalSessionSetup * Allocate(const CASEClientInitParams &, CASEClientPoolDelegate *, const ScopedNodeId &, OperationalSessionReleaseDelegate *) override { return nullptr; }
    void Release(OperationalSessionSetup *) override {}
    void ReleaseAllSessionSetupsForFabric(FabricIndex) override {}
    void ReleaseAllSessionSetup() override {}
    OperationalSessionSetup * FindSessionSetup(const ScopedNodeId &, bool) override { return nullptr; }
};
} // namespace

TEST(CASESessionManagerTest, InitReturnsErrorOnInvalidParams)
{
    CASESessionManager mgr;
    CASESessionManagerConfig config;
    // sessionInitParams.Validate() will fail because exchangeMgr is nullptr
    EXPECT_EQ(mgr.Init(nullptr, config), CHIP_ERROR_INVALID_ARGUMENT);
}

TEST(CASESessionManagerTest, InitSucceedsWithValidParams)
{
    CASESessionManager mgr;
    CASESessionManagerConfig config;
    DummyExchangeMgr dummyExchangeMgr;
    DummySessionManager dummySessionManager;
    config.sessionInitParams.exchangeMgr = &dummyExchangeMgr;
    config.sessionInitParams.sessionManager = &dummySessionManager;
    DummyCASEClientPool dummyClientPool;
    DummyOperationalSessionSetupPool dummySessionSetupPool;
    config.clientPool = &dummyClientPool;
    config.sessionSetupPool = &dummySessionSetupPool;
    EXPECT_EQ(mgr.Init(nullptr, config), CHIP_NO_ERROR);
}

TEST(CASESessionManagerTest, ShutdownDoesNotCrash)
{
    CASESessionManager mgr;
    CASESessionManagerConfig config;
    DummyExchangeMgr dummyExchangeMgr;
    DummySessionManager dummySessionManager;
    config.sessionInitParams.exchangeMgr = &dummyExchangeMgr;
    config.sessionInitParams.sessionManager = &dummySessionManager;
    DummyCASEClientPool dummyClientPool;
    DummyOperationalSessionSetupPool dummySessionSetupPool;
    config.clientPool = &dummyClientPool;
    config.sessionSetupPool = &dummySessionSetupPool;
    EXPECT_EQ(mgr.Init(nullptr, config), CHIP_NO_ERROR);
    mgr.Shutdown();
}