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

#include <cstring>

#include <lib/support/CHIPMem.h>
#include <pw_unit_test/framework.h>

#include <platform/silabs/multi-ota/OTATlvProcessor.h>

#include <platform/silabs/multi-ota/OTATlvProcessor.cpp> // nogncheck

using namespace chip;
using namespace chip::DeviceLayer::Silabs::MultiOTA;

namespace {

CHIP_ERROR StubProcessDescriptor(void * /* descriptor */)
{
    return CHIP_NO_ERROR;
}

class TestableOTATlvProcessor : public OTATlvProcessor
{
public:
    CHIP_ERROR nextProcessInternalStatus = CHIP_NO_ERROR;
    CHIP_ERROR nextExitActionStatus     = CHIP_NO_ERROR;
    size_t lastProcessInternalBlockSize  = 0;

    using OTATlvProcessor::ClearInternal;
    using OTATlvProcessor::IsError;
    using OTATlvProcessor::mLastBlock;
    using OTATlvProcessor::mLength;
    using OTATlvProcessor::mProcessedLength;

    CHIP_ERROR ProcessInternal(ByteSpan & block) override
    {
        lastProcessInternalBlockSize = block.size();
        return nextProcessInternalStatus;
    }

    CHIP_ERROR ExitAction() override { return nextExitActionStatus; }

    CHIP_ERROR ApplyAction() override { return CHIP_NO_ERROR; }

    CHIP_ERROR FinalizeAction() override { return CHIP_NO_ERROR; }

    void ResetBehavior()
    {
        nextProcessInternalStatus    = CHIP_NO_ERROR;
        nextExitActionStatus         = CHIP_NO_ERROR;
        lastProcessInternalBlockSize = 0;
    }
};

class OTATlvProcessorFixture : public ::testing::Test
{
public:
    static void SetUpTestSuite() { ASSERT_EQ(Platform::MemoryInit(), CHIP_NO_ERROR); }

    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    void SetUp() override
    {
        mProcessor.RegisterDescriptorCallback(StubProcessDescriptor);
        ASSERT_EQ(mProcessor.Init(), CHIP_NO_ERROR);
        mProcessor.ResetBehavior();
    }

protected:
    TestableOTATlvProcessor mProcessor;
};

} // namespace

TEST(OTATlvProcessorIsValidTag, KnownProcessors)
{
    TestableOTATlvProcessor processor;
    EXPECT_TRUE(processor.IsValidTag(OTAProcessorTag::kApplicationProcessor));
    EXPECT_TRUE(processor.IsValidTag(OTAProcessorTag::kBootloaderProcessor));
    EXPECT_TRUE(processor.IsValidTag(OTAProcessorTag::kFactoryDataProcessor));
    EXPECT_TRUE(processor.IsValidTag(OTAProcessorTag::kWiFiTAProcessor));
    EXPECT_TRUE(processor.IsValidTag(OTAProcessorTag::kCustomProcessor1));
    EXPECT_TRUE(processor.IsValidTag(OTAProcessorTag::kCustomProcessor2));
    EXPECT_TRUE(processor.IsValidTag(OTAProcessorTag::kCustomProcessor3));
}

TEST(OTATlvProcessorIsValidTag, MaxValueBoundary)
{
    TestableOTATlvProcessor processor;
    EXPECT_TRUE(processor.IsValidTag(OTAProcessorTag::kMaxValue));
}

TEST(OTATlvProcessorIsValidTag, GapValuesInRange)
{
    TestableOTATlvProcessor processor;
    EXPECT_TRUE(processor.IsValidTag(static_cast<OTAProcessorTag>(5)));
    EXPECT_TRUE(processor.IsValidTag(static_cast<OTAProcessorTag>(6)));
    EXPECT_TRUE(processor.IsValidTag(static_cast<OTAProcessorTag>(7)));
}

TEST(OTATlvProcessorIsValidTag, BelowRange)
{
    TestableOTATlvProcessor processor;
    EXPECT_FALSE(processor.IsValidTag(static_cast<OTAProcessorTag>(0)));
}

TEST(OTATlvProcessorIsValidTag, AboveRange)
{
    TestableOTATlvProcessor processor;
    EXPECT_FALSE(processor.IsValidTag(static_cast<OTAProcessorTag>(12)));
}

TEST_F(OTATlvProcessorFixture, IsErrorSuccess)
{
    CHIP_ERROR status = CHIP_NO_ERROR;
    EXPECT_FALSE(mProcessor.IsError(status));
}

TEST_F(OTATlvProcessorFixture, IsErrorBufferTooSmall)
{
    CHIP_ERROR status = CHIP_ERROR_BUFFER_TOO_SMALL;
    EXPECT_FALSE(mProcessor.IsError(status));
}

TEST_F(OTATlvProcessorFixture, IsErrorFetchAlreadyScheduled)
{
    CHIP_ERROR status = CHIP_OTA_FETCH_ALREADY_SCHEDULED;
    EXPECT_FALSE(mProcessor.IsError(status));
}

TEST_F(OTATlvProcessorFixture, IsErrorGenericFailure)
{
    CHIP_ERROR status = CHIP_ERROR_INTERNAL;
    EXPECT_TRUE(mProcessor.IsError(status));
}

TEST_F(OTATlvProcessorFixture, IsErrorCustomOtaError)
{
    CHIP_ERROR status = CHIP_OTA_CHANGE_PROCESSOR;
    EXPECT_TRUE(mProcessor.IsError(status));
}

TEST(OTADataAccumulator, InitSetsThreshold)
{
    OTADataAccumulator accumulator;
    accumulator.Init(64);
    EXPECT_EQ(accumulator.GetThreshold(), 64u);
    EXPECT_NE(accumulator.data(), nullptr);
    accumulator.Clear();
}

TEST(OTADataAccumulator, ClearResetsState)
{
    OTADataAccumulator accumulator;
    accumulator.Init(8);

    uint8_t input[] = { 0x01, 0x02, 0x03 };
    ByteSpan block(input);
    EXPECT_EQ(accumulator.Accumulate(block), CHIP_ERROR_BUFFER_TOO_SMALL);

    accumulator.Clear();
    EXPECT_EQ(accumulator.GetThreshold(), 0u);
}

TEST(OTADataAccumulator, ReInitAfterClear)
{
    OTADataAccumulator accumulator;
    accumulator.Init(32);
    accumulator.Clear();
    accumulator.Init(64);
    EXPECT_EQ(accumulator.GetThreshold(), 64u);
    EXPECT_NE(accumulator.data(), nullptr);
    accumulator.Clear();
}

TEST(OTADataAccumulator, SingleChunkExact)
{
    OTADataAccumulator accumulator;
    accumulator.Init(4);

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04 };
    ByteSpan block(input);
    EXPECT_EQ(accumulator.Accumulate(block), CHIP_NO_ERROR);
    EXPECT_TRUE(block.empty());
    EXPECT_EQ(accumulator.data()[0], 0x01);
    EXPECT_EQ(accumulator.data()[3], 0x04);
    accumulator.Clear();
}

TEST(OTADataAccumulator, MultiChunkPartial)
{
    OTADataAccumulator accumulator;
    accumulator.Init(8);

    uint8_t chunk1[] = { 0x01, 0x02, 0x03 };
    ByteSpan block1(chunk1);
    EXPECT_EQ(accumulator.Accumulate(block1), CHIP_ERROR_BUFFER_TOO_SMALL);
    EXPECT_TRUE(block1.empty());

    uint8_t chunk2[] = { 0x04, 0x05, 0x06 };
    ByteSpan block2(chunk2);
    EXPECT_EQ(accumulator.Accumulate(block2), CHIP_ERROR_BUFFER_TOO_SMALL);
    EXPECT_TRUE(block2.empty());
    accumulator.Clear();
}

TEST(OTADataAccumulator, MultiChunkComplete)
{
    OTADataAccumulator accumulator;
    accumulator.Init(8);

    uint8_t chunk1[] = { 0x01, 0x02, 0x03 };
    ByteSpan block1(chunk1);
    EXPECT_EQ(accumulator.Accumulate(block1), CHIP_ERROR_BUFFER_TOO_SMALL);

    uint8_t chunk2[] = { 0x04, 0x05, 0x06 };
    ByteSpan block2(chunk2);
    EXPECT_EQ(accumulator.Accumulate(block2), CHIP_ERROR_BUFFER_TOO_SMALL);

    uint8_t chunk3[] = { 0x07, 0x08 };
    ByteSpan block3(chunk3);
    EXPECT_EQ(accumulator.Accumulate(block3), CHIP_NO_ERROR);
    EXPECT_TRUE(block3.empty());
    accumulator.Clear();
}

TEST(OTADataAccumulator, OversizedInput)
{
    OTADataAccumulator accumulator;
    accumulator.Init(4);

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
    ByteSpan block(input);
    EXPECT_EQ(accumulator.Accumulate(block), CHIP_NO_ERROR);
    EXPECT_EQ(block.size(), 6u);
    accumulator.Clear();
}

TEST(OTADataAccumulator, ZeroThreshold)
{
    OTADataAccumulator accumulator;
    accumulator.Init(0);

    uint8_t input[] = { 0x01, 0x02 };
    ByteSpan block(input);
    EXPECT_EQ(accumulator.Accumulate(block), CHIP_NO_ERROR);
    EXPECT_EQ(block.size(), 2u);
    accumulator.Clear();
}

TEST(OTADataAccumulator, EmptyBlock)
{
    OTADataAccumulator accumulator;
    accumulator.Init(4);

    uint8_t input[] = { 0x01 };
    ByteSpan block(input);
    block = block.SubSpan(0, 0);
    EXPECT_EQ(accumulator.Accumulate(block), CHIP_ERROR_BUFFER_TOO_SMALL);
    accumulator.Clear();
}

TEST(OTADataAccumulator, BlockSpanUpdated)
{
    OTADataAccumulator accumulator;
    accumulator.Init(4);

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    ByteSpan block(input);
    EXPECT_EQ(accumulator.Accumulate(block), CHIP_NO_ERROR);
    EXPECT_EQ(block.size(), 2u);
    accumulator.Clear();
}

TEST(OTATlvProcessorInitClear, RequiresDescriptorCallback)
{
    TestableOTATlvProcessor processor;
    EXPECT_EQ(processor.Init(), CHIP_OTA_PROCESSOR_CB_NOT_REGISTERED);
}

TEST(OTATlvProcessorInitClear, SuccessWithCallback)
{
    TestableOTATlvProcessor processor;
    processor.RegisterDescriptorCallback(StubProcessDescriptor);
    EXPECT_EQ(processor.Init(), CHIP_NO_ERROR);
}

TEST(OTATlvProcessorInitClear, ClearResetsProcessorState)
{
    TestableOTATlvProcessor processor;
    processor.RegisterDescriptorCallback(StubProcessDescriptor);
    ASSERT_EQ(processor.Init(), CHIP_NO_ERROR);

    processor.SetLength(100);
    processor.mProcessedLength = 50;
    processor.SetWasSelected(true);
    EXPECT_EQ(processor.Clear(), CHIP_NO_ERROR);
    EXPECT_EQ(processor.mLength, 0u);
    EXPECT_EQ(processor.mProcessedLength, 0u);
    EXPECT_FALSE(processor.WasSelected());
}

TEST_F(OTATlvProcessorFixture, ProcessSingleChunkExactLength)
{
    mProcessor.SetLength(4);

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_OTA_CHANGE_PROCESSOR);
    EXPECT_EQ(mProcessor.mProcessedLength, 4u);
    EXPECT_TRUE(block.empty());
    EXPECT_EQ(mProcessor.lastProcessInternalBlockSize, 4u);
}

TEST_F(OTATlvProcessorFixture, ProcessSingleChunkWithRemainder)
{
    mProcessor.SetLength(4);

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_OTA_CHANGE_PROCESSOR);
    EXPECT_EQ(mProcessor.mProcessedLength, 4u);
    EXPECT_EQ(block.size(), 4u);
}

TEST_F(OTATlvProcessorFixture, ProcessSingleChunkExitActionFails)
{
    mProcessor.SetLength(4);
    mProcessor.nextExitActionStatus = CHIP_ERROR_INTERNAL;

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_ERROR_INTERNAL);
}

TEST_F(OTATlvProcessorFixture, ProcessMultiChunkUntilComplete)
{
    mProcessor.SetLength(10);

    uint8_t chunk1[] = { 0x01, 0x02, 0x03 };
    ByteSpan block1(chunk1);
    EXPECT_EQ(mProcessor.Process(block1), CHIP_NO_ERROR);
    EXPECT_EQ(mProcessor.mProcessedLength, 3u);

    uint8_t chunk2[] = { 0x04, 0x05, 0x06 };
    ByteSpan block2(chunk2);
    EXPECT_EQ(mProcessor.Process(block2), CHIP_NO_ERROR);
    EXPECT_EQ(mProcessor.mProcessedLength, 6u);

    uint8_t chunk3[] = { 0x07, 0x08, 0x09, 0x0A };
    ByteSpan block3(chunk3);
    EXPECT_EQ(mProcessor.Process(block3), CHIP_OTA_CHANGE_PROCESSOR);
    EXPECT_EQ(mProcessor.mProcessedLength, 10u);
}

TEST_F(OTATlvProcessorFixture, ProcessPartialChunkSize)
{
    mProcessor.SetLength(100);

    uint8_t input[1024] = { 0 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_NO_ERROR);
    EXPECT_EQ(mProcessor.mProcessedLength, 100u);
    EXPECT_EQ(block.size(), 924u);
    EXPECT_EQ(mProcessor.lastProcessInternalBlockSize, 100u);
}

TEST_F(OTATlvProcessorFixture, ProcessBufferTooSmallAdvances)
{
    mProcessor.SetLength(8);
    mProcessor.nextProcessInternalStatus = CHIP_ERROR_BUFFER_TOO_SMALL;

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_ERROR_BUFFER_TOO_SMALL);
    EXPECT_EQ(mProcessor.mProcessedLength, 4u);
    EXPECT_TRUE(block.empty());
}

TEST_F(OTATlvProcessorFixture, ProcessFetchScheduledAdvances)
{
    mProcessor.SetLength(8);
    mProcessor.nextProcessInternalStatus = CHIP_OTA_FETCH_ALREADY_SCHEDULED;

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_OTA_FETCH_ALREADY_SCHEDULED);
    EXPECT_EQ(mProcessor.mProcessedLength, 4u);
    EXPECT_TRUE(block.empty());
}

TEST_F(OTATlvProcessorFixture, ProcessFatalErrorNoAdvance)
{
    mProcessor.SetLength(8);
    mProcessor.nextProcessInternalStatus = CHIP_ERROR_INTERNAL;

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_ERROR_INTERNAL);
    EXPECT_EQ(mProcessor.mProcessedLength, 0u);
    EXPECT_EQ(block.size(), 4u);
}

TEST_F(OTATlvProcessorFixture, ProcessFatalErrorMidStream)
{
    mProcessor.SetLength(12);

    uint8_t chunk1[] = { 0x01, 0x02, 0x03, 0x04 };
    ByteSpan block1(chunk1);
    EXPECT_EQ(mProcessor.Process(block1), CHIP_NO_ERROR);
    EXPECT_EQ(mProcessor.mProcessedLength, 4u);

    mProcessor.nextProcessInternalStatus = CHIP_ERROR_INTERNAL;
    uint8_t chunk2[] = { 0x05, 0x06, 0x07, 0x08 };
    ByteSpan block2(chunk2);
    EXPECT_EQ(mProcessor.Process(block2), CHIP_ERROR_INTERNAL);
    EXPECT_EQ(mProcessor.mProcessedLength, 4u);
}

TEST_F(OTATlvProcessorFixture, ProcessLastBlockSetOnFinalChunk)
{
    mProcessor.SetLength(8);

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_OTA_CHANGE_PROCESSOR);
    EXPECT_TRUE(mProcessor.mLastBlock);
}

TEST_F(OTATlvProcessorFixture, ProcessLastBlockNotSetOnPartial)
{
    mProcessor.SetLength(8);

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04 };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_NO_ERROR);
    EXPECT_FALSE(mProcessor.mLastBlock);
}

TEST_F(OTATlvProcessorFixture, ProcessPassesCorrectSubSpan)
{
    mProcessor.SetLength(6);

    uint8_t input[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
    ByteSpan block(input);
    EXPECT_EQ(mProcessor.Process(block), CHIP_OTA_CHANGE_PROCESSOR);
    EXPECT_EQ(mProcessor.lastProcessInternalBlockSize, 6u);
}

#if defined(SL_MATTER_ENABLE_OTA_ENCRYPTION) && SL_MATTER_ENABLE_OTA_ENCRYPTION

TEST_F(OTATlvProcessorFixture, RemovePaddingSingleByteBlock)
{
    uint8_t buffer[] = { 0xAA, 0x01 };
    MutableByteSpan block(buffer);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_NO_ERROR);
    EXPECT_EQ(block.size(), 1u);
    EXPECT_EQ(block.data()[0], 0xAA);
}

TEST_F(OTATlvProcessorFixture, RemovePaddingPadLengthFour)
{
    uint8_t buffer[] = { 0x01, 0x02, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04 };
    MutableByteSpan block(buffer);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_NO_ERROR);
    EXPECT_EQ(block.size(), 4u);
}

TEST_F(OTATlvProcessorFixture, RemovePaddingFullBlockPad)
{
    uint8_t buffer[16];
    memset(buffer, 0x10, sizeof(buffer));
    MutableByteSpan block(buffer);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_NO_ERROR);
    EXPECT_EQ(block.size(), 0u);
}

TEST_F(OTATlvProcessorFixture, RemovePaddingEmptyBlock)
{
    uint8_t buffer[] = { 0x00 };
    MutableByteSpan block(buffer);
    block.reduce_size(0);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(block.size(), 0u);
}

TEST_F(OTATlvProcessorFixture, RemovePaddingZeroPadByte)
{
    uint8_t buffer[] = { 0x01, 0x02, 0x00 };
    MutableByteSpan block(buffer);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(block.size(), 3u);
}

TEST_F(OTATlvProcessorFixture, RemovePaddingPadLengthExceedsBlock)
{
    uint8_t buffer[] = { 0x01, 0x02, 0x04 };
    MutableByteSpan block(buffer);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(block.size(), 3u);
}

TEST_F(OTATlvProcessorFixture, RemovePaddingWrongPadByte)
{
    uint8_t buffer[] = { 0x01, 0x02, 0x03, 0x02, 0x03, 0x03 };
    MutableByteSpan block(buffer);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(block.size(), 6u);
}

TEST_F(OTATlvProcessorFixture, RemovePaddingMismatchAtFirstPadByte)
{
    uint8_t buffer[] = { 0x01, 0x02, 0x01, 0x03, 0x03, 0x03 };
    MutableByteSpan block(buffer);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_ERROR_INVALID_ARGUMENT);
}

TEST_F(OTATlvProcessorFixture, RemovePaddingMismatchAtLastPadByte)
{
    uint8_t buffer[] = { 0x01, 0x02, 0x03, 0x03, 0x03, 0x02 };
    MutableByteSpan block(buffer);
    EXPECT_EQ(mProcessor.RemovePadding(block), CHIP_ERROR_INVALID_ARGUMENT);
}

#endif // SL_MATTER_ENABLE_OTA_ENCRYPTION
