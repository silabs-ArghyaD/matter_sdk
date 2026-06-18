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

#include <lib/support/BufferReader.h>
#include <platform/silabs/multi-ota/OTAMultiImageProcessorImpl.h>

namespace chip {
namespace DeviceLayer {
namespace Silabs {
namespace MultiOTA {

void OTAMultiImageProcessorImpl::Clear()
{
    mHeaderParser.Clear();
    mAccumulator.Clear();
    mParams.totalFileBytes  = 0;
    mParams.downloadedBytes = 0;
    mCurrentProcessor       = nullptr;
    VerifyOrReturn(ReleaseBlock() == CHIP_NO_ERROR, ChipLogError(SoftwareUpdate, "Release block failed while Clearing"));
}

CHIP_ERROR OTAMultiImageProcessorImpl::ReleaseBlock()
{
    if (mBlock.data() != nullptr)
    {
        chip::Platform::MemoryFree(mBlock.data());
    }

    mBlock = MutableByteSpan();
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAMultiImageProcessorImpl::SelectProcessor(ByteSpan & block)
{
    OTATlvHeader header;
    Encoding::LittleEndian::Reader reader(block.data(), sizeof(header));

    ReturnErrorOnFailure(reader.Read32(&header.tag).StatusCode());
    ReturnErrorOnFailure(reader.Read32(&header.length).StatusCode());

    auto pair = mProcessorMap.find(static_cast<OTAProcessorTag>(header.tag));
    if (pair == mProcessorMap.end())
    {
        ChipLogError(SoftwareUpdate, "There is no registered processor for tag: %u", static_cast<unsigned>(header.tag));
        return CHIP_OTA_PROCESSOR_NOT_REGISTERED;
    }

    ChipLogProgress(SoftwareUpdate, "Selected processor with tag: %u", static_cast<unsigned>(pair->first));
    mCurrentProcessor = pair->second;
    mCurrentProcessor->SetLength(header.length);
    mCurrentProcessor->SetWasSelected(true);

    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAMultiImageProcessorImpl::RegisterProcessor(OTAProcessorTag tag, OTATlvProcessor * processor)
{
    VerifyOrReturnError(processor->IsValidTag(tag), CHIP_ERROR_INVALID_ARGUMENT,
                        ChipLogError(SoftwareUpdate, "Invalid processor tag: %u", static_cast<unsigned>(tag)));
    auto pair = mProcessorMap.find(tag);
    if (pair != mProcessorMap.end())
    {
        ChipLogError(SoftwareUpdate, "A processor for tag %u is already registered.", static_cast<unsigned>(tag));
        return CHIP_OTA_PROCESSOR_ALREADY_REGISTERED;
    }

    mProcessorMap.insert({ tag, processor });

    return CHIP_NO_ERROR;
}

} // namespace MultiOTA
} // namespace Silabs
} // namespace DeviceLayer
} // namespace chip
