/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
 *    All rights reserved.
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

#include <lib/support/BufferWriter.h>
#include <lib/support/CHIPMem.h>
#include <pw_unit_test/framework.h>

#include <platform/silabs/multi-ota/OTAMultiImageProcessorImpl.h>

using namespace chip;
using namespace chip::DeviceLayer::Silabs::MultiOTA;

namespace {

CHIP_ERROR StubProcessDescriptor(void * /* descriptor */)
{
    return CHIP_NO_ERROR;
}

class FakeOTATlvProcessor : public OTATlvProcessor
{
public:
    using OTATlvProcessor::mLength;

    CHIP_ERROR ProcessInternal(ByteSpan & /* block */) override { return CHIP_NO_ERROR; }
    CHIP_ERROR ApplyAction() override { return CHIP_NO_ERROR; }
    CHIP_ERROR FinalizeAction() override { return CHIP_NO_ERROR; }
};

ByteSpan MakeTlvHeaderSpan(OTAProcessorTag tag, uint32_t length, uint8_t * storage)
{
    chip::Encoding::LittleEndian::BufferWriter writer(storage, sizeof(OTATlvHeader));
    writer.Put32(static_cast<uint32_t>(tag));
    writer.Put32(length);
    return ByteSpan(storage, sizeof(OTATlvHeader));
}

class OTAMultiImageProcessorFixture : public ::testing::Test
{
public:
    static void SetUpTestSuite() { ASSERT_EQ(Platform::MemoryInit(), CHIP_NO_ERROR); }

    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    void SetUp() override
    {
        mProcessor.Clear();
        mFakeProcessor.RegisterDescriptorCallback(StubProcessDescriptor);
        ASSERT_EQ(mFakeProcessor.Init(), CHIP_NO_ERROR);
    }

protected:
    OTAMultiImageProcessorImpl mProcessor;
    FakeOTATlvProcessor mFakeProcessor;
};

} // namespace

TEST(OTAMultiImageProcessorRegisterProcessor, RejectsInvalidTag)
{
    OTAMultiImageProcessorImpl processor;
    FakeOTATlvProcessor fakeProcessor;

    EXPECT_EQ(processor.RegisterProcessor(static_cast<OTAProcessorTag>(0), &fakeProcessor), CHIP_ERROR_INVALID_ARGUMENT);
}

TEST(OTAMultiImageProcessorRegisterProcessor, RejectsDuplicateRegistration)
{
    OTAMultiImageProcessorImpl processor;
    FakeOTATlvProcessor fakeProcessor;

    EXPECT_EQ(processor.RegisterProcessor(OTAProcessorTag::kApplicationProcessor, &fakeProcessor), CHIP_NO_ERROR);
    EXPECT_EQ(processor.RegisterProcessor(OTAProcessorTag::kApplicationProcessor, &fakeProcessor),
              CHIP_OTA_PROCESSOR_ALREADY_REGISTERED);
}

TEST_F(OTAMultiImageProcessorFixture, RegisterProcessorAcceptsValidProcessor)
{
    EXPECT_EQ(mProcessor.RegisterProcessor(OTAProcessorTag::kApplicationProcessor, &mFakeProcessor), CHIP_NO_ERROR);
}

TEST_F(OTAMultiImageProcessorFixture, SelectProcessorPicksRegisteredProcessor)
{
    ASSERT_EQ(mProcessor.RegisterProcessor(OTAProcessorTag::kApplicationProcessor, &mFakeProcessor), CHIP_NO_ERROR);

    uint8_t headerBytes[sizeof(OTATlvHeader)] = {};
    ByteSpan tlvHeader                      = MakeTlvHeaderSpan(OTAProcessorTag::kApplicationProcessor, 512, headerBytes);

    EXPECT_EQ(mProcessor.SelectProcessor(tlvHeader), CHIP_NO_ERROR);
    EXPECT_TRUE(mFakeProcessor.WasSelected());
    EXPECT_EQ(mFakeProcessor.mLength, 512u);
}

TEST_F(OTAMultiImageProcessorFixture, SelectProcessorReturnsNotRegisteredForUnknownTag)
{
    uint8_t headerBytes[sizeof(OTATlvHeader)] = {};
    ByteSpan tlvHeader                      = MakeTlvHeaderSpan(OTAProcessorTag::kFactoryDataProcessor, 128, headerBytes);

    EXPECT_EQ(mProcessor.SelectProcessor(tlvHeader), CHIP_OTA_PROCESSOR_NOT_REGISTERED);
    EXPECT_FALSE(mFakeProcessor.WasSelected());
}
