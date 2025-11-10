/**
 *
 *    Copyright (c) 2025 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <app/clusters/closure-dimension-server/closure-dimension-cluster-logic.h>
#include <app/clusters/closure-dimension-server/closure-dimension-cluster-objects.h>
#include <app/clusters/closure-dimension-server/closure-dimension-delegate.h>
#include <app/clusters/closure-dimension-server/closure-dimension-matter-context.h>

#include <lib/core/Optional.h>
#include <lib/support/BitFlags.h>
#include <lib/support/CHIPMem.h>
#include <pw_unit_test/framework.h>
#include <system/SystemClock.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <vector>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters::ClosureDimension;
using chip::MakeOptional;
using chip::Protocols::InteractionModel::Status;

namespace {

class TestClock : public System::Clock::Internal::MockClock
{
public:
    void Advance(System::Clock::Milliseconds64 delta) { SetMonotonic(GetMonotonicMilliseconds64() + delta); }
};

class MockDelegate : public DelegateBase
{
public:
    Status HandleSetTarget(const Optional<Percent100ths> & position, const Optional<bool> & latch,
                           const Optional<Globals::ThreeLevelAutoEnum> & speed) override
    {
        lastSetTargetPosition = position;
        lastSetTargetLatch    = latch;
        lastSetTargetSpeed    = speed;
        setTargetCallCount++;
        return nextSetTargetStatus;
    }

    Status HandleStep(const StepDirectionEnum & direction, const uint16_t & numberOfSteps,
                      const Optional<Globals::ThreeLevelAutoEnum> & speed) override
    {
        lastStepDirection = direction;
        lastStepCount     = numberOfSteps;
        lastStepSpeed     = speed;
        stepCallCount++;
        return nextStepStatus;
    }

    void Reset()
    {
        nextSetTargetStatus = Status::Success;
        nextStepStatus      = Status::Success;
        setTargetCallCount  = 0;
        stepCallCount       = 0;
        lastSetTargetPosition.ClearValue();
        lastSetTargetLatch.ClearValue();
        lastSetTargetSpeed.ClearValue();
        lastStepSpeed.ClearValue();
        lastStepDirection = StepDirectionEnum::kUnknownEnumValue;
        lastStepCount     = 0;
    }

    Status nextSetTargetStatus = Status::Success;
    Status nextStepStatus      = Status::Success;
    uint16_t setTargetCallCount = 0;
    uint16_t stepCallCount      = 0;
    Optional<Percent100ths> lastSetTargetPosition;
    Optional<bool> lastSetTargetLatch;
    Optional<Globals::ThreeLevelAutoEnum> lastSetTargetSpeed;
    Optional<Globals::ThreeLevelAutoEnum> lastStepSpeed;
    StepDirectionEnum lastStepDirection = StepDirectionEnum::kUnknownEnumValue;
    uint16_t lastStepCount              = 0;
};

class MockMatterContext : public MatterContext
{
public:
    MockMatterContext() : MatterContext(kEndpointId) {}

    void MarkDirty(AttributeId attributeId) override { mDirtyAttributes.push_back(attributeId); }

    void Reset() { mDirtyAttributes.clear(); }

    bool WasAttributeMarked(AttributeId attributeId) const
    {
        return std::find(mDirtyAttributes.begin(), mDirtyAttributes.end(), attributeId) != mDirtyAttributes.end();
    }

    bool HasDirty() const { return !mDirtyAttributes.empty(); }

    size_t DirtyCount() const { return mDirtyAttributes.size(); }

private:
    static constexpr EndpointId kEndpointId = 1;
    std::vector<AttributeId> mDirtyAttributes;
};

Optional<DataModel::Nullable<Percent100ths>> MakePosition(Percent100ths value)
{
    return Optional<DataModel::Nullable<Percent100ths>>(DataModel::MakeNullable(value));
}

Optional<DataModel::Nullable<bool>> MakeLatch(bool value)
{
    return Optional<DataModel::Nullable<bool>>(DataModel::MakeNullable(value));
}

DataModel::Nullable<GenericDimensionStateStruct> MakeState(
    Optional<DataModel::Nullable<Percent100ths>> position = NullOptional,
    Optional<DataModel::Nullable<bool>> latch             = NullOptional,
    Optional<Globals::ThreeLevelAutoEnum> speed           = NullOptional)
{
    DataModel::Nullable<GenericDimensionStateStruct> state;
    state.SetNonNull(GenericDimensionStateStruct(position, latch, speed));
    return state;
}

class TestClosureDimensionClusterLogic : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ASSERT_EQ(Platform::MemoryInit(), CHIP_NO_ERROR);
        sSavedClock = &System::SystemClock();
        System::Clock::Internal::SetSystemClockForTesting(&sClock);
    }

    static void TearDownTestSuite()
    {
        System::Clock::Internal::SetSystemClockForTesting(sSavedClock);
        Platform::MemoryShutdown();
    }

    void SetUp() override
    {
        mockDelegate.Reset();
        mockContext.Reset();
        conformance = ClusterConformance();
        initParams  = ClusterInitParameters();
        logic       = std::make_unique<ClusterLogic>(mockDelegate, mockContext);
        sClock.SetMonotonic(System::Clock::Milliseconds64(0));
    }

    void TearDown() override { logic.reset(); }

    void InitWithFeatures(std::initializer_list<Feature> features)
    {
        conformance = ClusterConformance();
        for (Feature feature : features)
        {
            conformance.FeatureMap().Set(feature);
        }
        ASSERT_EQ(logic->Init(conformance, initParams), CHIP_NO_ERROR);
    }

    static TestClock sClock;
    static System::Clock::ClockBase * sSavedClock;

    MockDelegate mockDelegate;
    MockMatterContext mockContext;
    ClusterConformance conformance;
    ClusterInitParameters initParams;
    std::unique_ptr<ClusterLogic> logic;
};

TestClock TestClosureDimensionClusterLogic::sClock;
System::Clock::ClockBase * TestClosureDimensionClusterLogic::sSavedClock = nullptr;

} // namespace

TEST_F(TestClosureDimensionClusterLogic, InitSetsTranslationDirection)
{
    initParams.translationDirection = TranslationDirectionEnum::kForward;
    InitWithFeatures({ Feature::kPositioning, Feature::kTranslation });

    TranslationDirectionEnum translationDirection = TranslationDirectionEnum::kUnknownEnumValue;
    EXPECT_EQ(logic->GetTranslationDirection(translationDirection), CHIP_NO_ERROR);
    EXPECT_EQ(translationDirection, TranslationDirectionEnum::kForward);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::TranslationDirection::Id));
}

TEST_F(TestClosureDimensionClusterLogic, InitReturnsErrorWhenConformanceInvalid)
{
    conformance.FeatureMap().Set(Feature::kTranslation).Set(Feature::kRotation);
    EXPECT_EQ(logic->Init(conformance, initParams), CHIP_ERROR_INVALID_DEVICE_DESCRIPTOR);
    EXPECT_FALSE(mockContext.HasDirty());
}

TEST_F(TestClosureDimensionClusterLogic, SetCurrentStateRequiresInitialization)
{
    DataModel::Nullable<GenericDimensionStateStruct> state;
    EXPECT_EQ(logic->SetCurrentState(state), CHIP_ERROR_INCORRECT_STATE);
    EXPECT_FALSE(mockContext.HasDirty());
}

TEST_F(TestClosureDimensionClusterLogic, SetCurrentStatePositionRequiresPositioningFeature)
{
    InitWithFeatures({ Feature::kMotionLatching });
    mockContext.Reset();

    auto state = MakeState(MakePosition(static_cast<Percent100ths>(1000)));
    EXPECT_EQ(logic->SetCurrentState(state), CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);
    EXPECT_FALSE(mockContext.HasDirty());
}

TEST_F(TestClosureDimensionClusterLogic, SetCurrentStateMarksDirtyWhenTargetReached)
{
    InitWithFeatures({ Feature::kPositioning });

    auto target = MakeState(MakePosition(static_cast<Percent100ths>(5000)));
    EXPECT_EQ(logic->SetTargetState(target), CHIP_NO_ERROR);

    mockContext.Reset();
    auto current = MakeState(MakePosition(static_cast<Percent100ths>(5000)));
    EXPECT_EQ(logic->SetCurrentState(current), CHIP_NO_ERROR);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::CurrentState::Id));
}

TEST_F(TestClosureDimensionClusterLogic, SetCurrentStateMarksDirtyWhenLatchChanges)
{
    InitWithFeatures({ Feature::kPositioning, Feature::kMotionLatching });

    auto initial = MakeState(MakePosition(static_cast<Percent100ths>(2000)), MakeLatch(false));
    EXPECT_EQ(logic->SetCurrentState(initial), CHIP_NO_ERROR);

    mockContext.Reset();
    auto updated = MakeState(MakePosition(static_cast<Percent100ths>(2000)), MakeLatch(true));
    EXPECT_EQ(logic->SetCurrentState(updated), CHIP_NO_ERROR);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::CurrentState::Id));
}

TEST_F(TestClosureDimensionClusterLogic, SetTargetStateRequiresResolutionAlignment)
{
    InitWithFeatures({ Feature::kPositioning });
    EXPECT_EQ(logic->SetResolution(static_cast<Percent100ths>(250)), CHIP_NO_ERROR);
    mockContext.Reset();

    auto target = MakeState(MakePosition(static_cast<Percent100ths>(375)));
    EXPECT_EQ(logic->SetTargetState(target), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(mockContext.HasDirty());
}

TEST_F(TestClosureDimensionClusterLogic, SetStepValueRequiresMultipleOfResolution)
{
    InitWithFeatures({ Feature::kPositioning });
    EXPECT_EQ(logic->SetResolution(static_cast<Percent100ths>(250)), CHIP_NO_ERROR);
    mockContext.Reset();

    EXPECT_EQ(logic->SetStepValue(static_cast<Percent100ths>(300)), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(mockContext.HasDirty());

    mockContext.Reset();
    EXPECT_EQ(logic->SetStepValue(static_cast<Percent100ths>(500)), CHIP_NO_ERROR);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::StepValue::Id));

    Percent100ths value = 0;
    EXPECT_EQ(logic->GetStepValue(value), CHIP_NO_ERROR);
    EXPECT_EQ(value, static_cast<Percent100ths>(500));
}

TEST_F(TestClosureDimensionClusterLogic, SetLimitRangeRequiresMultiplesOfResolution)
{
    InitWithFeatures({ Feature::kPositioning, Feature::kLimitation });
    EXPECT_EQ(logic->SetResolution(static_cast<Percent100ths>(500)), CHIP_NO_ERROR);
    mockContext.Reset();

    Structs::RangePercent100thsStruct::Type invalidRange;
    invalidRange.min = static_cast<Percent100ths>(0);
    invalidRange.max = static_cast<Percent100ths>(1600);
    EXPECT_EQ(logic->SetLimitRange(invalidRange), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(mockContext.HasDirty());

    Structs::RangePercent100thsStruct::Type validRange;
    validRange.min = static_cast<Percent100ths>(500);
    validRange.max = static_cast<Percent100ths>(9500);
    mockContext.Reset();
    EXPECT_EQ(logic->SetLimitRange(validRange), CHIP_NO_ERROR);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::LimitRange::Id));
}

TEST_F(TestClosureDimensionClusterLogic, HandleSetTargetCommandRoundsAndClampsPosition)
{
    InitWithFeatures({ Feature::kPositioning, Feature::kLimitation, Feature::kSpeed });
    EXPECT_EQ(logic->SetResolution(static_cast<Percent100ths>(500)), CHIP_NO_ERROR);

    Structs::RangePercent100thsStruct::Type limitRange;
    limitRange.min = static_cast<Percent100ths>(1000);
    limitRange.max = static_cast<Percent100ths>(9000);
    EXPECT_EQ(logic->SetLimitRange(limitRange), CHIP_NO_ERROR);

    auto current = MakeState(MakePosition(static_cast<Percent100ths>(2000)));
    EXPECT_EQ(logic->SetCurrentState(current), CHIP_NO_ERROR);

    mockContext.Reset();
    auto position = MakeOptional<Percent100ths>(static_cast<Percent100ths>(9733));
    auto speed    = MakeOptional<Globals::ThreeLevelAutoEnum>(Globals::ThreeLevelAutoEnum::kHigh);
    EXPECT_EQ(logic->HandleSetTargetCommand(position, NullOptional, speed), Status::Success);

    EXPECT_EQ(mockDelegate.setTargetCallCount, 1u);
    EXPECT_TRUE(mockDelegate.lastSetTargetPosition.HasValue());
    EXPECT_EQ(mockDelegate.lastSetTargetPosition.Value(), static_cast<Percent100ths>(9000));
    EXPECT_TRUE(mockDelegate.lastSetTargetSpeed.HasValue());
    EXPECT_EQ(mockDelegate.lastSetTargetSpeed.Value(), Globals::ThreeLevelAutoEnum::kHigh);

    DataModel::Nullable<GenericDimensionStateStruct> target;
    EXPECT_EQ(logic->GetTargetState(target), CHIP_NO_ERROR);
    EXPECT_FALSE(target.IsNull());
    EXPECT_TRUE(target.Value().position.HasValue());
    EXPECT_FALSE(target.Value().position.Value().IsNull());
    EXPECT_EQ(target.Value().position.Value().Value(), static_cast<Percent100ths>(9000));
    EXPECT_TRUE(target.Value().speed.HasValue());
    EXPECT_EQ(target.Value().speed.Value(), Globals::ThreeLevelAutoEnum::kHigh);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::TargetState::Id));
}

TEST_F(TestClosureDimensionClusterLogic, HandleSetTargetCommandFailsWhenLatchedAndLatchNotCleared)
{
    InitWithFeatures({ Feature::kPositioning, Feature::kMotionLatching });

    BitFlags<LatchControlModesBitmap> modes;
    modes.Set(LatchControlModesBitmap::kRemoteLatching).Set(LatchControlModesBitmap::kRemoteUnlatching);
    EXPECT_EQ(logic->SetLatchControlModes(modes), CHIP_NO_ERROR);

    auto current = MakeState(MakePosition(static_cast<Percent100ths>(2000)), MakeLatch(true));
    EXPECT_EQ(logic->SetCurrentState(current), CHIP_NO_ERROR);

    mockContext.Reset();
    auto position = MakeOptional<Percent100ths>(static_cast<Percent100ths>(2500));
    EXPECT_EQ(logic->HandleSetTargetCommand(position, NullOptional, NullOptional), Status::InvalidInState);
    EXPECT_EQ(mockDelegate.setTargetCallCount, 0u);
    EXPECT_FALSE(mockContext.HasDirty());
}

TEST_F(TestClosureDimensionClusterLogic, HandleSetTargetCommandRequiresKnownCurrentPosition)
{
    InitWithFeatures({ Feature::kPositioning });
    mockContext.Reset();

    auto position = MakeOptional<Percent100ths>(static_cast<Percent100ths>(1000));
    EXPECT_EQ(logic->HandleSetTargetCommand(position, NullOptional, NullOptional), Status::InvalidInState);
    EXPECT_EQ(mockDelegate.setTargetCallCount, 0u);
    EXPECT_FALSE(mockContext.HasDirty());
}

TEST_F(TestClosureDimensionClusterLogic, HandleStepCommandIncreasesWithinLimits)
{
    InitWithFeatures({ Feature::kPositioning, Feature::kLimitation });
    EXPECT_EQ(logic->SetResolution(static_cast<Percent100ths>(500)), CHIP_NO_ERROR);
    EXPECT_EQ(logic->SetStepValue(static_cast<Percent100ths>(500)), CHIP_NO_ERROR);

    Structs::RangePercent100thsStruct::Type limitRange;
    limitRange.min = static_cast<Percent100ths>(0);
    limitRange.max = static_cast<Percent100ths>(8000);
    EXPECT_EQ(logic->SetLimitRange(limitRange), CHIP_NO_ERROR);

    auto current = MakeState(MakePosition(static_cast<Percent100ths>(3000)));
    EXPECT_EQ(logic->SetCurrentState(current), CHIP_NO_ERROR);

    mockContext.Reset();
    EXPECT_EQ(logic->HandleStepCommand(StepDirectionEnum::kIncrease, 3, NullOptional), Status::Success);

    EXPECT_EQ(mockDelegate.stepCallCount, 1u);
    EXPECT_EQ(mockDelegate.lastStepDirection, StepDirectionEnum::kIncrease);
    EXPECT_EQ(mockDelegate.lastStepCount, static_cast<uint16_t>(3));

    DataModel::Nullable<GenericDimensionStateStruct> target;
    EXPECT_EQ(logic->GetTargetState(target), CHIP_NO_ERROR);
    EXPECT_FALSE(target.IsNull());
    EXPECT_TRUE(target.Value().position.HasValue());
    EXPECT_FALSE(target.Value().position.Value().IsNull());
    EXPECT_EQ(target.Value().position.Value().Value(), static_cast<Percent100ths>(4500));
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::TargetState::Id));
}

TEST_F(TestClosureDimensionClusterLogic, HandleStepCommandDecreasesAndClampsToLimit)
{
    InitWithFeatures({ Feature::kPositioning, Feature::kLimitation });
    EXPECT_EQ(logic->SetResolution(static_cast<Percent100ths>(400)), CHIP_NO_ERROR);
    EXPECT_EQ(logic->SetStepValue(static_cast<Percent100ths>(400)), CHIP_NO_ERROR);

    Structs::RangePercent100thsStruct::Type limitRange;
    limitRange.min = static_cast<Percent100ths>(600);
    limitRange.max = static_cast<Percent100ths>(8000);
    EXPECT_EQ(logic->SetLimitRange(limitRange), CHIP_NO_ERROR);

    auto current = MakeState(MakePosition(static_cast<Percent100ths>(900)));
    EXPECT_EQ(logic->SetCurrentState(current), CHIP_NO_ERROR);

    mockContext.Reset();
    EXPECT_EQ(logic->HandleStepCommand(StepDirectionEnum::kDecrease, 5, NullOptional), Status::Success);

    DataModel::Nullable<GenericDimensionStateStruct> target;
    EXPECT_EQ(logic->GetTargetState(target), CHIP_NO_ERROR);
    EXPECT_FALSE(target.IsNull());
    EXPECT_TRUE(target.Value().position.HasValue());
    EXPECT_FALSE(target.Value().position.Value().IsNull());
    EXPECT_EQ(target.Value().position.Value().Value(), static_cast<Percent100ths>(600));
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::TargetState::Id));
}

TEST_F(TestClosureDimensionClusterLogic, HandleStepCommandUnsupportedWithoutPositioningFeature)
{
    InitWithFeatures({ Feature::kMotionLatching });
    mockContext.Reset();

    EXPECT_EQ(logic->HandleStepCommand(StepDirectionEnum::kIncrease, 1, NullOptional), Status::UnsupportedCommand);
    EXPECT_EQ(mockDelegate.stepCallCount, 0u);
    EXPECT_FALSE(mockContext.HasDirty());
}

TEST_F(TestClosureDimensionClusterLogic, SetLatchControlModesMarksAttributeDirty)
{
    InitWithFeatures({ Feature::kMotionLatching });
    mockContext.Reset();

    BitFlags<LatchControlModesBitmap> modes;
    modes.Set(LatchControlModesBitmap::kRemoteLatching);
    EXPECT_EQ(logic->SetLatchControlModes(modes), CHIP_NO_ERROR);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::LatchControlModes::Id));

    BitFlags<LatchControlModesBitmap> readModes;
    EXPECT_EQ(logic->GetLatchControlModes(readModes), CHIP_NO_ERROR);
    EXPECT_TRUE(readModes.Has(LatchControlModesBitmap::kRemoteLatching));
}

TEST_F(TestClosureDimensionClusterLogic, SetUnitRangeValidatesMillimeterBounds)
{
    InitWithFeatures({ Feature::kPositioning, Feature::kUnit });
    EXPECT_EQ(logic->SetUnit(ClosureUnitEnum::kMillimeter), CHIP_NO_ERROR);
    mockContext.Reset();

    Structs::UnitRangeStruct::Type invalidRange;
    invalidRange.min = static_cast<int16_t>(-1);
    invalidRange.max = static_cast<int16_t>(100);
    DataModel::Nullable<Structs::UnitRangeStruct::Type> unitRange;
    unitRange.SetNonNull(invalidRange);
    EXPECT_EQ(logic->SetUnitRange(unitRange), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(mockContext.HasDirty());

    Structs::UnitRangeStruct::Type validRange;
    validRange.min = static_cast<int16_t>(0);
    validRange.max = static_cast<int16_t>(3000);
    unitRange.SetNonNull(validRange);
    mockContext.Reset();
    EXPECT_EQ(logic->SetUnitRange(unitRange), CHIP_NO_ERROR);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::UnitRange::Id));
}

TEST_F(TestClosureDimensionClusterLogic, SetUnitRangeValidatesDegreeSpan)
{
    InitWithFeatures({ Feature::kPositioning, Feature::kUnit });
    EXPECT_EQ(logic->SetUnit(ClosureUnitEnum::kDegree), CHIP_NO_ERROR);
    mockContext.Reset();

    Structs::UnitRangeStruct::Type invalidRange;
    invalidRange.min = static_cast<int16_t>(-400);
    invalidRange.max = static_cast<int16_t>(400);
    DataModel::Nullable<Structs::UnitRangeStruct::Type> unitRange;
    unitRange.SetNonNull(invalidRange);
    EXPECT_EQ(logic->SetUnitRange(unitRange), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(mockContext.HasDirty());

    Structs::UnitRangeStruct::Type validRange;
    validRange.min = static_cast<int16_t>(-200);
    validRange.max = static_cast<int16_t>(200);
    unitRange.SetNonNull(validRange);
    mockContext.Reset();
    EXPECT_EQ(logic->SetUnitRange(unitRange), CHIP_NO_ERROR);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::UnitRange::Id));
}

TEST_F(TestClosureDimensionClusterLogic, SetOverflowValidatesForCenteredAxis)
{
    initParams.rotationAxis = RotationAxisEnum::kCenteredVertical;
    InitWithFeatures({ Feature::kPositioning, Feature::kRotation });

    mockContext.Reset();
    EXPECT_EQ(logic->SetOverflow(OverflowEnum::kInside), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(mockContext.HasDirty());

    mockContext.Reset();
    EXPECT_EQ(logic->SetOverflow(OverflowEnum::kTopInside), CHIP_NO_ERROR);
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::Overflow::Id));
}

TEST_F(TestClosureDimensionClusterLogic, HandleSetTargetCommandValidatesLatchModes)
{
    InitWithFeatures({ Feature::kMotionLatching });

    auto current = MakeState(NullOptional, MakeLatch(false));
    EXPECT_EQ(logic->SetCurrentState(current), CHIP_NO_ERROR);

    mockContext.Reset();
    auto latch = MakeOptional<bool>(true);
    EXPECT_EQ(logic->HandleSetTargetCommand(NullOptional, latch, NullOptional), Status::InvalidInState);
    EXPECT_EQ(mockDelegate.setTargetCallCount, 0u);
    EXPECT_FALSE(mockContext.HasDirty());

    BitFlags<LatchControlModesBitmap> modes;
    modes.Set(LatchControlModesBitmap::kRemoteLatching).Set(LatchControlModesBitmap::kRemoteUnlatching);
    EXPECT_EQ(logic->SetLatchControlModes(modes), CHIP_NO_ERROR);

    mockContext.Reset();
    EXPECT_EQ(logic->HandleSetTargetCommand(NullOptional, latch, NullOptional), Status::Success);
    EXPECT_EQ(mockDelegate.setTargetCallCount, 1u);

    DataModel::Nullable<GenericDimensionStateStruct> target;
    EXPECT_EQ(logic->GetTargetState(target), CHIP_NO_ERROR);
    EXPECT_FALSE(target.IsNull());
    EXPECT_TRUE(target.Value().latch.HasValue());
    EXPECT_FALSE(target.Value().latch.Value().IsNull());
    EXPECT_TRUE(target.Value().latch.Value().Value());
    EXPECT_TRUE(mockContext.WasAttributeMarked(Attributes::TargetState::Id));
}

