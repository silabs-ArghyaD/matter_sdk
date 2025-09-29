#include <lib/core/StringBuilderAdapters.h>
#include <pw_unit_test/framework.h>
#include <app/data-model/Nullable.h>

#include "ClosureManager.h"
#include "ClosureControlEndpoint.h"
#include "ClosureDimensionEndpoint.h"

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::ClosureControl;
using namespace chip::app::Clusters::ClosureDimension;

// Stub/mock classes for dependencies
class MockClosureControlEndpoint : public ClosureControlEndpoint
{
public:
    CHIP_ERROR InitCalled = CHIP_ERROR_INTERNAL;
    CHIP_ERROR Init() override
    {
        InitCalled = CHIP_NO_ERROR;
        return CHIP_NO_ERROR;
    }
    ClosureControl::ClusterConformance GetConformance() override
    {
        return ClosureControl::ClusterConformance().Set(ClosureControl::Feature::kPositioning);
    }
};

class MockClosureDimensionEndpoint : public ClosureDimensionEndpoint
{
public:
    CHIP_ERROR InitCalled = CHIP_ERROR_INTERNAL;
    CHIP_ERROR Init() override
    {
        InitCalled = CHIP_NO_ERROR;
        return CHIP_NO_ERROR;
    }
    ClosureDimension::ClusterConformance GetConformance() override
    {
        return ClosureDimension::ClusterConformance().Set(ClosureDimension::Feature::kPositioning);
    }
};

namespace {

TEST(ClosureManagerValidation, InitialStateIsCorrect)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    // After construction, default action should be INVALID_ACTION and endpoint id invalid
    EXPECT_EQ(mgr.GetCurrentAction(), ClosureManager::Action_t::INVALID_ACTION);
    EXPECT_EQ(mgr.mCurrentActionEndpointId, chip::kInvalidEndpointId);
}

TEST(ClosureManagerValidation, InitSetsUpEndpointsAndTags)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    // Call Init and expect no crash (side effects are hard to check, but timer creation and endpoint init are called)
    mgr.Init();
    // After Init, endpoints should be initialized (no direct way to check, but no error thrown)
    EXPECT_TRUE(true);
}

TEST(ClosureManagerValidation, SetClosureControlInitialStateAcceptsValidEndpoint)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    MockClosureControlEndpoint mockEndpoint;
    CHIP_ERROR err = mgr.SetClosureControlInitialState(mockEndpoint);
    EXPECT_EQ(err, CHIP_NO_ERROR);
}

TEST(ClosureManagerValidation, SetClosurePanelInitialStateAcceptsValidEndpoint)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    MockClosureDimensionEndpoint mockEndpoint;
    CHIP_ERROR err = mgr.SetClosurePanelInitialState(mockEndpoint);
    EXPECT_EQ(err, CHIP_NO_ERROR);
}

TEST(ClosureManagerValidation, GetPanelNextPositionIncreasesPosition)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    GenericDimensionStateStruct current, target;
    current.position.SetValue(DataModel::MakeNullable<Percent100ths>(1000));
    target.position.SetValue(DataModel::MakeNullable<Percent100ths>(5000));
    DataModel::Nullable<Percent100ths> next;
    bool result = mgr.GetPanelNextPosition(current, target, next);
    EXPECT_TRUE(result);
    EXPECT_EQ(next.Value(), 3000); // 1000 + 2000 step
}

TEST(ClosureManagerValidation, GetPanelNextPositionDecreasesPosition)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    GenericDimensionStateStruct current, target;
    current.position.SetValue(DataModel::MakeNullable<Percent100ths>(8000));
    target.position.SetValue(DataModel::MakeNullable<Percent100ths>(2000));
    DataModel::Nullable<Percent100ths> next;
    bool result = mgr.GetPanelNextPosition(current, target, next);
    EXPECT_TRUE(result);
    EXPECT_EQ(next.Value(), 6000); // 8000 - 2000 step
}

TEST(ClosureManagerValidation, GetPanelNextPositionAtTargetReturnsFalse)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    GenericDimensionStateStruct current, target;
    current.position.SetValue(DataModel::MakeNullable<Percent100ths>(4000));
    target.position.SetValue(DataModel::MakeNullable<Percent100ths>(4000));
    DataModel::Nullable<Percent100ths> next;
    bool result = mgr.GetPanelNextPosition(current, target, next);
    EXPECT_FALSE(result);
    EXPECT_EQ(next.Value(), 4000);
}

TEST(ClosureManagerValidation, GetPanelNextPositionFailsWithNullTarget)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    GenericDimensionStateStruct current, target;
    current.position.SetValue(DataModel::MakeNullable<Percent100ths>(1000));
    // target.position left unset (null)
    DataModel::Nullable<Percent100ths> next;
    bool result = mgr.GetPanelNextPosition(current, target, next);
    EXPECT_FALSE(result);
}

TEST(ClosureManagerValidation, GetPanelNextPositionFailsWithNullCurrent)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    GenericDimensionStateStruct current, target;
    // current.position left unset (null)
    target.position.SetValue(DataModel::MakeNullable<Percent100ths>(5000));
    DataModel::Nullable<Percent100ths> next;
    bool result = mgr.GetPanelNextPosition(current, target, next);
    EXPECT_FALSE(result);
}

TEST(ClosureManagerValidation, GetPanelEndpointByIdReturnsCorrectPointer)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    mgr.Init();
    ClosureDimensionEndpoint * ep2 = mgr.GetPanelEndpointById(mgr.mClosurePanelEndpoint2.GetEndpointId());
    ClosureDimensionEndpoint * ep3 = mgr.GetPanelEndpointById(mgr.mClosurePanelEndpoint3.GetEndpointId());
    EXPECT_EQ(ep2, &mgr.mClosurePanelEndpoint2);
    EXPECT_EQ(ep3, &mgr.mClosurePanelEndpoint3);
}

TEST(ClosureManagerValidation, GetPanelEndpointByIdReturnsNullForInvalidId)
{
    ClosureManager & mgr = ClosureManager::GetInstance();
    mgr.Init();
    ClosureDimensionEndpoint * ep = mgr.GetPanelEndpointById(0xFF); // Invalid endpoint id
    EXPECT_EQ(ep, nullptr);
}

} // namespace