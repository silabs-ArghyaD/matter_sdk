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

#include <platform/silabs/multi-ota/OTAMultiImageProcessorImpl.h>

namespace chip {
namespace DeviceLayer {
namespace Silabs {
namespace MultiOTA {

CHIP_ERROR OTAMultiImageProcessorImpl::PrepareDownload()
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR OTAMultiImageProcessorImpl::Finalize()
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR OTAMultiImageProcessorImpl::Apply()
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR OTAMultiImageProcessorImpl::Abort()
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR OTAMultiImageProcessorImpl::ProcessBlock(ByteSpan & /* block */)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

bool OTAMultiImageProcessorImpl::IsFirstImageRun()
{
    return false;
}

CHIP_ERROR OTAMultiImageProcessorImpl::ConfirmCurrentImage()
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace MultiOTA
} // namespace Silabs
} // namespace DeviceLayer
} // namespace chip
