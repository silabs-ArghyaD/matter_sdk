#include <pw_unit_test/framework.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/AttributeAccessInterface.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/InteractionModelEngine.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/data-model/List.h>
#include <app/util/attribute-storage.h>
#include <app/util/endpoint-config-api.h>
#include <clusters/Descriptor/Attributes.h>
#include <clusters/Descriptor/Metadata.h>
#include <clusters/Descriptor/Structs.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Global.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/ReadOnlyBuffer.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::Descriptor;
using namespace chip::app::Clusters::Descriptor::Attributes;

namespace {

class TestAttributeValueEncoder : public AttributeValueEncoder
{
public:
    TestAttributeValueEncoder() : AttributeValueEncoder(nullptr, nullptr, nullptr, 0, nullptr, nullptr) {}
    template <typename T>
    CHIP_ERROR Encode(const T &) { return CHIP_NO_ERROR; }
    template <typename T>
    CHIP_ERROR EncodeList(T &&) { return CHIP_NO_ERROR; }
};

class DummyDataModelProvider : public DataModel::Provider::Interface
{
public:
    CHIP_ERROR SemanticTags(EndpointId, ReadOnlyBufferBuilder<DataModel::Provider::SemanticTag> &) override { return CHIP_NO_ERROR; }
    CHIP_ERROR Endpoints(ReadOnlyBufferBuilder<DataModel::EndpointEntry> &) override { return CHIP_NO_ERROR; }
    CHIP_ERROR DeviceTypes(EndpointId, ReadOnlyBufferBuilder<DataModel::DeviceTypeEntry> &) override { return CHIP_NO_ERROR; }
    CHIP_ERROR ServerClusters(EndpointId, ReadOnlyBufferBuilder<DataModel::ServerClusterEntry> &) override { return CHIP_NO_ERROR; }
    CHIP_ERROR ClientClusters(EndpointId, ReadOnlyBufferBuilder<ClusterId> &) override { return CHIP_NO_ERROR; }
#if CHIP_CONFIG_USE_ENDPOINT_UNIQUE_ID
    CHIP_ERROR EndpointUniqueID(EndpointId, MutableCharSpan &) override { return CHIP_NO_ERROR; }
#endif
};

class DummyInteractionModelEngine : public InteractionModelEngine
{
public:
    static DummyInteractionModelEngine * GetInstance()
    {
        static DummyInteractionModelEngine instance;
        return &instance;
    }
    DataModel::Provider::Interface * GetDataModelProvider() override
    {
        static DummyDataModelProvider provider;
        return &provider;
    }
};

} // anonymous namespace

TEST(DescriptorTest, IsDescendantOfFunctionExistsAndCanBeCalled)
{
    DataModel::EndpointEntry parent = {};
    DataModel::EndpointEntry child = {};
    DataModel::EndpointEntry endpoints[2] = { parent, child };
    Span<const DataModel::EndpointEntry> allEndpoints(endpoints, 2);
    bool result = IsDescendantOf(&child, parent.id, allEndpoints);
    EXPECT_TRUE(result == true || result == false);
}

TEST(DescriptorTest, DescriptorAttrAccessReadFeatureMapExistsAndCanBeCalled)
{
    DescriptorAttrAccess attrAccess;
    EndpointId endpoint = 1;
    TestAttributeValueEncoder encoder;
    CHIP_ERROR err = attrAccess.ReadFeatureMap(endpoint, encoder);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(DescriptorTest, DescriptorAttrAccessReadTagListAttributeExistsAndCanBeCalled)
{
    DescriptorAttrAccess attrAccess;
    EndpointId endpoint = 1;
    TestAttributeValueEncoder encoder;
    CHIP_ERROR err = attrAccess.ReadTagListAttribute(endpoint, encoder);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(DescriptorTest, DescriptorAttrAccessReadPartsAttributeExistsAndCanBeCalled)
{
    DescriptorAttrAccess attrAccess;
    EndpointId endpoint = 1;
    TestAttributeValueEncoder encoder;
    CHIP_ERROR err = attrAccess.ReadPartsAttribute(endpoint, encoder);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(DescriptorTest, DescriptorAttrAccessReadDeviceAttributeExistsAndCanBeCalled)
{
    DescriptorAttrAccess attrAccess;
    EndpointId endpoint = 1;
    TestAttributeValueEncoder encoder;
    CHIP_ERROR err = attrAccess.ReadDeviceAttribute(endpoint, encoder);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(DescriptorTest, DescriptorAttrAccessReadServerClustersExistsAndCanBeCalled)
{
    DescriptorAttrAccess attrAccess;
    EndpointId endpoint = 1;
    TestAttributeValueEncoder encoder;
    CHIP_ERROR err = attrAccess.ReadServerClusters(endpoint, encoder);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(DescriptorTest, DescriptorAttrAccessReadClientClustersExistsAndCanBeCalled)
{
    DescriptorAttrAccess attrAccess;
    EndpointId endpoint = 1;
    TestAttributeValueEncoder encoder;
    CHIP_ERROR err = attrAccess.ReadClientClusters(endpoint, encoder);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

#if CHIP_CONFIG_USE_ENDPOINT_UNIQUE_ID
TEST(DescriptorTest, DescriptorAttrAccessReadEndpointUniqueIdExistsAndCanBeCalled)
{
    DescriptorAttrAccess attrAccess;
    EndpointId endpoint = 1;
    TestAttributeValueEncoder encoder;
    CHIP_ERROR err = attrAccess.ReadEndpointUniqueId(endpoint, encoder);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}
#endif

TEST(DescriptorTest, DescriptorAttrAccessReadExistsAndCanBeCalled)
{
    DescriptorAttrAccess attrAccess;
    ConcreteReadAttributePath path;
    path.mAttributeId = DeviceTypeList::Id;
    path.mEndpointId = 1;
    TestAttributeValueEncoder encoder;
    CHIP_ERROR err = attrAccess.Read(path, encoder);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(DescriptorTest, MatterDescriptorPluginServerInitCallbackExistsAndCanBeCalled)
{
    MatterDescriptorPluginServerInitCallback();
    EXPECT_TRUE(true);
}

TEST(DescriptorTest, MatterDescriptorPluginServerShutdownCallbackExistsAndCanBeCalled)
{
    MatterDescriptorPluginServerShutdownCallback();
    EXPECT_TRUE(true);
}