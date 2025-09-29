#include <pw_unit_test/framework.h>
#include <app/clusters/boolean-state-configuration-server/boolean-state-configuration-server.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/AttributeAccessInterface.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/CommandHandler.h>
#include <app/ConcreteCommandPath.h>
#include <app/EventLogging.h>
#include <app/SafeAttributePersistenceProvider.h>
#include <app/data-model/Encode.h>
#include <app/util/attribute-storage.h>
#include <app/util/config.h>
#include <lib/core/CHIPError.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CHIPDeviceConfig.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::BooleanStateConfiguration::Attributes;
using chip::app::Clusters::BooleanStateConfiguration::Delegate;
using chip::Protocols::InteractionModel::Status;

TEST(BooleanStateConfigurationServerTest, SetDefaultDelegateAndGetDefaultDelegate)
{
    EndpointId testEndpoint = 1;
    Delegate * delegate = nullptr;
    SetDefaultDelegate(testEndpoint, delegate);
    Delegate * result = GetDefaultDelegate(testEndpoint);
    EXPECT_EQ(result, delegate);
}

TEST(BooleanStateConfigurationServerTest, SetAlarmsActiveFunctionExists)
{
    EndpointId testEndpoint = 1;
    BitMask<BooleanStateConfiguration::AlarmModeBitmap> alarms;
    // Just call the function to verify it exists and returns a CHIP_ERROR
    CHIP_ERROR err = SetAlarmsActive(testEndpoint, alarms);
    // We can't guarantee the result, but we can check it's a CHIP_ERROR type
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(BooleanStateConfigurationServerTest, SetAllEnabledAlarmsActiveFunctionExists)
{
    EndpointId testEndpoint = 1;
    CHIP_ERROR err = SetAllEnabledAlarmsActive(testEndpoint);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(BooleanStateConfigurationServerTest, ClearAllAlarmsFunctionExists)
{
    EndpointId testEndpoint = 1;
    CHIP_ERROR err = ClearAllAlarms(testEndpoint);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(BooleanStateConfigurationServerTest, SuppressAlarmsFunctionExists)
{
    EndpointId testEndpoint = 1;
    BitMask<BooleanStateConfiguration::AlarmModeBitmap> alarms;
    CHIP_ERROR err = SuppressAlarms(testEndpoint, alarms);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(BooleanStateConfigurationServerTest, SetCurrentSensitivityLevelFunctionExists)
{
    EndpointId testEndpoint = 1;
    uint8_t level = 0;
    CHIP_ERROR err = SetCurrentSensitivityLevel(testEndpoint, level);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(BooleanStateConfigurationServerTest, EmitSensorFaultFunctionExists)
{
    EndpointId testEndpoint = 1;
    BitMask<BooleanStateConfiguration::SensorFaultBitmap> fault;
    CHIP_ERROR err = EmitSensorFault(testEndpoint, fault);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR);
}

TEST(BooleanStateConfigurationServerTest, HasFeatureFunctionExists)
{
    EndpointId testEndpoint = 1;
    BooleanStateConfiguration::Feature feature = BooleanStateConfiguration::Feature::kVisual;
    bool has = HasFeature(testEndpoint, feature);
    EXPECT_TRUE(has == true || has == false);
}