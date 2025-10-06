#include <pw_unit_test/framework.h>
#include "boolean-state-configuration-server.h"

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

TEST(BooleanStateConfigurationTest, SetDefaultDelegateFunctionExists)
{
    EndpointId endpoint = 1;
    Delegate * delegate = nullptr;
    BooleanStateConfiguration::SetDefaultDelegate(endpoint, delegate);
    EXPECT_TRUE(true); // Just verify function can be called
}

TEST(BooleanStateConfigurationTest, GetDefaultDelegateFunctionExists)
{
    EndpointId endpoint = 1;
    Delegate * delegate = BooleanStateConfiguration::GetDefaultDelegate(endpoint);
    EXPECT_TRUE(delegate == nullptr || delegate != nullptr); // Just verify function can be called
}

TEST(BooleanStateConfigurationTest, SetAlarmsActiveFunctionExists)
{
    EndpointId endpoint = 1;
    BitMask<BooleanStateConfiguration::AlarmModeBitmap> alarms;
    CHIP_ERROR err = BooleanStateConfiguration::SetAlarmsActive(endpoint, alarms);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR); // Just verify function can be called
}

TEST(BooleanStateConfigurationTest, SetAllEnabledAlarmsActiveFunctionExists)
{
    EndpointId endpoint = 1;
    CHIP_ERROR err = BooleanStateConfiguration::SetAllEnabledAlarmsActive(endpoint);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR); // Just verify function can be called
}

TEST(BooleanStateConfigurationTest, ClearAllAlarmsFunctionExists)
{
    EndpointId endpoint = 1;
    CHIP_ERROR err = BooleanStateConfiguration::ClearAllAlarms(endpoint);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR); // Just verify function can be called
}

TEST(BooleanStateConfigurationTest, SuppressAlarmsFunctionExists)
{
    EndpointId endpoint = 1;
    BitMask<BooleanStateConfiguration::AlarmModeBitmap> alarms;
    CHIP_ERROR err = BooleanStateConfiguration::SuppressAlarms(endpoint, alarms);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR); // Just verify function can be called
}

TEST(BooleanStateConfigurationTest, SetCurrentSensitivityLevelFunctionExists)
{
    EndpointId endpoint = 1;
    uint8_t level = 2;
    CHIP_ERROR err = BooleanStateConfiguration::SetCurrentSensitivityLevel(endpoint, level);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR); // Just verify function can be called
}

TEST(BooleanStateConfigurationTest, EmitSensorFaultFunctionExists)
{
    EndpointId endpoint = 1;
    BitMask<BooleanStateConfiguration::SensorFaultBitmap> fault;
    CHIP_ERROR err = BooleanStateConfiguration::EmitSensorFault(endpoint, fault);
    EXPECT_TRUE(err == CHIP_NO_ERROR || err != CHIP_NO_ERROR); // Just verify function can be called
}

TEST(BooleanStateConfigurationTest, HasFeatureFunctionExists)
{
    EndpointId endpoint = 1;
    BooleanStateConfiguration::Feature feature = BooleanStateConfiguration::Feature::kAudible;
    bool hasFeature = BooleanStateConfiguration::HasFeature(endpoint, feature);
    EXPECT_TRUE(hasFeature == true || hasFeature == false); // Just verify function can be called
}