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

#include <lib/core/StringBuilderAdapters.h>
#include <pw_unit_test/framework.h>

#include <app/clusters/closure-control-server/closure-control-cluster-logic.h>

using chip::app::Clusters::ClosureControl::ClusterConformance;
using chip::app::Clusters::ClosureControl::Feature;

/*
    ClusterConformance Valid Function Test Case
*/

TEST(TestClosureControlConformance, ValidWhenPositioningEnabled)
{
    ClusterConformance conformance;
    conformance.FeatureMap().Set(Feature::kPositioning);

    EXPECT_TRUE(conformance.Valid());
}

TEST(TestClosureControlConformance, ValidWhenMotionLatchingEnabled)
{
    ClusterConformance conformance;
    conformance.FeatureMap().Set(Feature::kMotionLatching);

    EXPECT_TRUE(conformance.Valid());
}

TEST(TestClosureControlConformance, NoPositioningOrMotionLatching_ReturnsFalse)
{
    ClusterConformance c;
    EXPECT_FALSE(c.Valid());
}

TEST(TestClosureControlConformance, SpeedWithoutPositioning_ReturnsFalse)
{
    ClusterConformance c;
    c.FeatureMap().Set(Feature::kSpeed);
    EXPECT_FALSE(c.Valid());
}

TEST(TestClosureControlConformance, SpeedWithInstantaneous_ReturnsFalse)
{
    ClusterConformance c;
    c.FeatureMap().Set(Feature::kPositioning);
    c.FeatureMap().Set(Feature::kSpeed);
    c.FeatureMap().Set(Feature::kInstantaneous); // Invalid with Speed
    EXPECT_FALSE(c.Valid());
}

TEST(TestClosureControlConformance, SpeedWithPositioningNoInstantaneous_ReturnsTrue)
{
    ClusterConformance c;
    c.FeatureMap().Set(Feature::kPositioning);
    c.FeatureMap().Set(Feature::kSpeed);
    EXPECT_TRUE(c.Valid());
}

TEST(TestClosureControlConformance, VentilationWithoutPositioning_ReturnsFalse)
{
    ClusterConformance c;
    c.FeatureMap().Set(Feature::kVentilation);
    EXPECT_FALSE(c.Valid());
}

