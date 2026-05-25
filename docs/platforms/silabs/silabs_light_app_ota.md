# Matter OTA Software Update on Silicon Labs (Light App)

This document explains, in a simple way, how a Matter OTA Software Update works
end-to-end for the Silicon Labs Light example, where the OTA code lives in this
repository, and where you can add traces to find out where an OTA is failing.

It is targeted at the SiWx917 SoC Wi-Fi Light App (matches the logs that
started this investigation: `[SWU] Current software version = 1, expected
software version = 2` and `Failed to confirm image: 3`), but the same flow
applies to EFR32 / NCP variants with a different image processor.

---

## 1. High-level Matter OTA flow

A Matter OTA involves three actors:

1. **OTA Provider**     - typically `chip-ota-provider-app` running on a host.
2. **OTA Requestor**    - the device being updated (the Light App).
3. **Administrator**    - `chip-tool` (or any commissioner) telling the
   Requestor where to find the Provider.

The flow has six major phases:

```
   +------------+   1. QueryImage         +------------+
   |            | ----------------------> |            |
   |  Requestor |   2. QueryImageResponse |  Provider  |
   |  (Device)  | <---------------------- |   (PC)     |
   |            |   3. BDX file transfer  |            |
   |            | <======================>|            |
   |            |   4. ApplyUpdateRequest |            |
   |            | ----------------------> |            |
   |            |   5. ApplyUpdateResp    |            |
   |            | <---------------------- |            |
   |            |   6. NotifyUpdateApplied|            |
   |            | ----------------------> |            |
   +------------+                         +------------+
        |
        | (reboot into new image,
        |  ConfirmCurrentImage)
        v
   running new version
```

| Phase | Description |
|-------|-------------|
| 1. QueryImage         | Requestor periodically (or on a trigger) asks the Provider if a newer image is available. |
| 2. QueryImageResponse | Provider replies with `UpdateAvailable`, the new SoftwareVersion, a BDX URI and an Update Token. |
| 3. BDX transfer       | Requestor opens a BDX session and pulls the image blocks. Each block is fed to the platform `OTAImageProcessor`, which writes it to flash. |
| 4. ApplyUpdateRequest | After the download ends, the Requestor asks the Provider whether it may apply the new image now. |
| 5. ApplyUpdateResponse| Provider answers `Proceed`. The Requestor calls `OTAImageProcessor::Apply()` which, on Silicon Labs, soft-resets the SoC. |
| 6. NotifyUpdateApplied| After the reboot, the Requestor checks that the new version is actually running (`ConfirmCurrentImage`) and notifies the Provider. |

The log line `Failed to confirm image: 3` (where `3` is
`CHIP_ERROR_INCORRECT_STATE`) happens in phase 6: the device rebooted but the
firmware that is now running still reports `SoftwareVersion = 1`, so the
Requestor decides the update did not take effect.

---

## 2. Function-to-function flow (happy path)

Two binaries participate: the **Light app (requestor)** and
**chip-ota-provider-app (provider)**. BDX runs between them after `QueryImage`.
Only the important functions are listed.

### 0. Wiring (once, after Wi-Fi / DNS-SD)

```
BaseApplication::InitOTARequestorHandler()
  └─ OTAConfig::Init()
       ├─ DefaultOTARequestor::Init()
       ├─ DefaultOTARequestorDriver::Init()     ← post-reboot path decided here
       ├─ BDXDownloader + OTAImageProcessorImpl::Init()
       └─ gDownloader.SetImageProcessorDelegate(&imageProcessor)
```

SiWx flash path: `OTAImageProcessorImpl` in
`src/platform/silabs/SiWx/OTAImageProcessorImpl.cpp`.

### 1. Start update (QueryImage)

**Trigger (any of):**

```
DefaultOTARequestorDriver::PeriodicQueryTimerHandler()
  └─ SendQueryImage()

DefaultOTARequestorDriver::ProcessAnnounceOTAProviders()   ← chip-tool announce
  └─ SendQueryImage()

DefaultOTARequestor::TriggerImmediateQueryInternal()
```

**Requestor → provider:**

```
DefaultOTARequestor::ConnectToProvider(kQueryImage)
  └─ CASESessionManager::FindOrEstablishSession()
       └─ DefaultOTARequestor::OnConnected()
            └─ DefaultOTARequestor::SendQueryImageRequest()
```

**Provider:**

```
OTAProviderCluster::InvokeCommand()          // cluster 0x29
  └─ OTAProviderExample::HandleQueryImage()
       └─ OTAProviderExample::SendQueryImageResponse()
            └─ BdxOtaSender::PrepareForTransfer()   // arms BDX sender
```

**Requestor handles answer:**

```
DefaultOTARequestor::OnQueryImageResponse()
  └─ (if kUpdateAvailable && version > current)
       DefaultOTARequestorDriver::UpdateAvailable()
```

### 2. Download (BDX)

**Requestor starts download:**

```
DefaultOTARequestorDriver::DownloadUpdateTimerHandler()
  └─ DefaultOTARequestor::DownloadUpdate()
       └─ ConnectToProvider(kDownload)
            └─ OnConnected()
                 └─ DefaultOTARequestor::StartDownload()
                      └─ BDXDownloader::BeginPrepareDownload()
```

**Device writes image (SiWx platform):**

```
OTAImageProcessorImpl::PrepareDownload()
  └─ HandlePrepareDownload()
       └─ BDXDownloader::OnPreparedForDownload()
            └─ BDXDownloader::PollTransferSession()    // BDX state machine loop
```

**Per block:**

```
BDXDownloader::HandleBdxEvent(kBlockReceived)
  └─ OTAImageProcessorImpl::ProcessBlock()
       └─ HandleProcessBlock()
            ├─ ProcessHeader()              // first blocks: Matter OTA header
            └─ sl_si91x_fwup_start() / sl_si91x_fwup_load()   // RPS → flash
       └─ BDXDownloader::FetchNextData()
```

**Provider sends blocks (in parallel):**

```
BdxOtaSender::HandleTransferSessionOutput()
  └─ (reads .ota file, sends BDX Block / BlockEOF)
```

**End of file:**

```
BDXDownloader::HandleBdxEvent(EOF)
  └─ OTAImageProcessorImpl::Finalize()
       └─ HandleFinalize()                   // last partial chunk + fwup_load
  └─ (BlockAckEOF sent) → state kComplete
       └─ DefaultOTARequestor::OnDownloadStateChanged(kComplete)
            └─ DefaultOTARequestorDriver::UpdateDownloaded()
                 └─ DefaultOTARequestor::ApplyUpdate()
```

### 3. Apply (pre-reboot)

**Ask provider permission:**

```
DefaultOTARequestor::ApplyUpdate()
  ├─ RecordNewUpdateState(kApplying)
  ├─ StoreCurrentUpdateInfo()              // KVS: target version, provider, token
  └─ ConnectToProvider(kApplyUpdate)
       └─ OnConnected()
            └─ SendApplyUpdateRequest()
```

**Provider:**

```
OTAProviderCluster → OTAProviderExample::HandleApplyUpdateRequest()
  └─ commandObj->AddResponse(ApplyUpdateResponse)   // usually action = Proceed
```

**Requestor after Proceed:**

```
DefaultOTARequestor::OnApplyUpdateResponse(kProceed)
  └─ DefaultOTARequestorDriver::UpdateConfirmed(delay)
       └─ ApplyTimerHandler()                 // after delayedActionTime (often 0)
            └─ OTAImageProcessorImpl::Apply()
                 └─ HandleApply()
                      ├─ SilabsConfig::WriteConfigValue(MatterUpdateReboot)
                      └─ GetPlatform().SoftwareReset()   // reboot into new image
```

### 4. Post-reboot confirm and notify

**Boot:**

```
BaseApplication (startup)
  └─ logs CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION   // [SVR] Current Software Version: N
```

**OTA driver on init after apply:**

```
DefaultOTARequestorDriver::Init()
  └─ OTAImageProcessorImpl::IsFirstImageRun()   // update state == kApplying
       └─ ConfirmCurrentImage()
            └─ ConfigurationMgr::GetSoftwareVersion()  // must == GetTargetVersion()
       └─ DefaultOTARequestor::NotifyUpdateApplied()
            └─ ConnectToProvider(kNotifyUpdateApplied)
                 └─ OnConnected()
                      └─ SendNotifyUpdateAppliedRequest()
                           └─ Reset()            // OTA complete, back to idle
```

**Provider:**

```
OTAProviderExample::HandleNotifyUpdateApplied()
  └─ AddStatus(Success)    // [ZCL] OTA Provider received NotifyUpdateApplied
```

### 5. Log line to function map

| Log line | Function |
|----------|----------|
| `ApplyUpdate: current=1 target=2` | `DefaultOTARequestor::ApplyUpdate()` |
| Provider `ApplyUpdateRequest` / `New Version: 2` | `OTAProviderExample::HandleApplyUpdateRequest()` |
| `=====SL-Light starting=====` + `Current Software Version: 2` | Reboot after `HandleApply()` → `SoftwareReset()` |
| `IsFirstImageRun=1` / `ConfirmCurrentImage: OK` | `DefaultOTARequestorDriver::Init()` → `ConfirmCurrentImage()` |
| Provider `NotifyUpdateApplied` / `Software Version: 2` | `SendNotifyUpdateAppliedRequest()` → `HandleNotifyUpdateApplied()` |
| `Failed to confirm image` | `ConfirmCurrentImage()` version mismatch |
| Provider `Transfer completed, got AckEOF` | `BdxOtaSender` — transfer done, not yet confirmed on device |

### 6. Related functions (not on happy path)

- `DefaultOTARequestor::HandleAnnounceOTAProvider` — commissioner announce path
- `BDXDownloader::EndDownload` / `CleanupOnError` — failures and abort
- `DefaultOTARequestor::Reset()` — failed confirm or cancel
- Provider busy/retry paths in `HandleQueryImage`
- EFR32 `OTAImageProcessorImpl` (Gecko bootloader) instead of SiWx `sl_si91x_fwup_*` on other boards

---

## 3. Where the OTA code lives

Paths under `src/`, `examples/`, and `docs/` are relative to the Matter SDK
repository root. Paths under `slc/` refer to the Silicon Labs Matter extension
repository (for example `matter_extension`) used for SLC-based example builds.
CI OTA version overrides are in that extension under `.github/silabs-builds-*.json`.

### 3.1 Common (cross-platform) Matter OTA layer

These files are the same for every platform and implement the Matter cluster
logic, the state machine and BDX glue.

| File | Role |
|------|------|
| `src/app/clusters/ota-requestor/OTARequestorCluster.cpp` | Server side of the OTA Requestor cluster on the device (handles `AnnounceOTAProvider`, etc.). |
| `src/app/clusters/ota-requestor/DefaultOTARequestor.cpp` | Core state machine. Drives `QueryImage`, BDX init, `ApplyUpdateRequest`, `NotifyUpdateApplied`. |
| `src/app/clusters/ota-requestor/DefaultOTARequestorDriver.cpp` | Default driver, owns the periodic query timer and calls `ConfirmCurrentImage` after boot. This is where `Failed to confirm image: ...` is logged (around line 81). |
| `src/app/clusters/ota-requestor/BDXDownloader.cpp` | BDX file transfer client, feeds blocks to the image processor. |
| `src/app/clusters/ota-provider/OTAProviderCluster.cpp` | Provider-side cluster (only relevant on the provider). |

### 3.2 Silicon Labs platform glue

| File | Role |
|------|------|
| `examples/platform/silabs/OTAConfig.cpp` | Wires up `DefaultOTARequestor`, `DefaultOTARequestorDriver`, `BDXDownloader` and the platform `OTAImageProcessorImpl`. Entry point: `OTAConfig::Init()`. |
| `examples/platform/silabs/BaseApplication.cpp` (`InitOTARequestorHandler`) | Schedules `OTAConfig::Init()` once DNS-SD is up. |
| `src/platform/silabs/SiWx/OTAImageProcessorImpl.cpp` | **SiWx917 SoC Wi-Fi image processor** - writes the downloaded RPS bytes to the SiWx flash through `sl_si91x_fwup_start` / `sl_si91x_fwup_load`, then triggers the soft reset. This is the file that matters for the SoC Light App. |
| `src/platform/silabs/efr32/OTAImageProcessorImpl.cpp` | EFR32 (Gecko bootloader) variant, used for OpenThread / NCP host targets. |
| `src/platform/silabs/multi-ota/*` | Multi-image OTA flow (used when `SL_MATTER_ENABLE_MULTI_OTA_REQUESTOR` is set, e.g. NCP host + RCP firmware bundles). |
| `slc/component/matter-platform/ota/matter_multi_image_ota.slcc` | SLC component that turns the multi-image OTA path on. |

### 3.3 Application configuration that affects OTA

| File | Why it matters |
|------|----------------|
| `slc/config/sl_matter_config.h` | Default `CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION` (1) and `CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING` for SLC Matter apps. Included via `slc/inc/platform/CHIPDeviceBuildConfig.h`. |
| SLC `--configuration` | Override at build time, e.g. `CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION:2`. Used in CI (`.github/silabs-builds-*.json`). |
| `examples/lighting-app/silabs/include/CHIPProjectConfig.h` | Vendor/product ID and other project overrides; does **not** define software version for the Wi-Fi Light SLC build. |

The running version reported after reboot comes from the **OTA firmware build**, not from the `.ota` file header alone. Both must match the target (e.g. 2).

---

## 4. Reading the logs you posted

```
[SWU] Current software version = 1, expected software version = 2
[SWU] Failed to confirm image: 3
```

These two lines come from:

- `OTAImageProcessorImpl::ConfirmCurrentImage()` in
  `src/platform/silabs/SiWx/OTAImageProcessorImpl.cpp`
  around line 135, and
- `DefaultOTARequestorDriver::Init()` in
  `src/app/clusters/ota-requestor/DefaultOTARequestorDriver.cpp`
  around line 81.

The provider logs show that BDX completed (`AckEOFReceived`,
`Transfer completed, got AckEOF`) and `ApplyUpdateRequest` was answered, so
phases 1-5 succeeded. The failure is in phase 6: after the soft reset the
firmware running on the device still reports version `1`, not `2`.

Three things can cause that:

1. **The image header says v2 but the firmware inside is still v1.** Most
   common cause: building the OTA `.ota` file with
   `--vn 2 --vs "2.0"` while the OTA **firmware** was still built with
   `CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION` = 1 (see `slc/config/sl_matter_config.h`
   or SLC `--configuration`). The header parser in
   `ProcessHeader()` will happily accept it, the SiWx bootloader will swap
   slots, but the new image still hard-codes `1` as its software version.

2. **The bootloader did not actually swap the image.** `sl_si91x_fwup_load`
   never returned `SL_STATUS_SI91X_FW_UPDATE_DONE`, so `mReset` stayed
   `false`, `HandleApply` skipped `GetPlatform().SoftwareReset()` and the
   device just re-ran the old image. The traces below will tell you whether
   this happened.

3. **`ConfigurationMgr().GetSoftwareVersion()` returned the wrong value.**
   Less common; it normally reads from
   `CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION`.

---

## 5. Where to add traces

All traces should use the `ChipLog*` macros (preferred over `printf` because
they respect the log level / category) - usually with the `SoftwareUpdate`
category, which already appears as `[SWU]` in the logs.

### 5.1 SiWx917 image processor - the most useful place

File: `src/platform/silabs/SiWx/OTAImageProcessorImpl.cpp`

Suggested traces (add one at a time, do not commit the noisy ones):

- **`HandlePrepareDownload`** (start of a new transfer): already logs
  `HandlePrepareDownload`. Add the expected file size when it is known.

- **`ProcessHeader`** (line ~339): already prints the image header version
  and payload size. **Confirm this prints `software version: 2`**, otherwise
  the image you generated is mis-versioned.

- **`HandleProcessBlock`** (line ~257): add periodic progress so you can see
  if the transfer stalls. Example:

  ```cpp
  if ((imageProcessor->mParams.downloadedBytes % (16 * 1024)) == 0)
  {
      ChipLogProgress(SoftwareUpdate, "Downloaded %lu / %lu bytes",
                      (unsigned long) imageProcessor->mParams.downloadedBytes,
                      (unsigned long) imageProcessor->mParams.totalFileBytes);
  }
  ```

  Also log the status from `sl_si91x_fwup_load` when it is non-zero - the
  existing code only logs on error other than `SL_STATUS_SI91X_FW_UPDATE_DONE`.
  For diagnosis it is worth logging every non-`SL_STATUS_OK` value too:

  ```cpp
  if (status != SL_STATUS_OK)
  {
      ChipLogProgress(SoftwareUpdate, "fwup_load status=0x%lx mReset=%d", status, mReset);
  }
  ```

- **`HandleFinalize`** (line ~176): add a single line after the last chunk
  is loaded so you can clearly see whether `mReset` ended up `true`:

  ```cpp
  ChipLogProgress(SoftwareUpdate, "Finalize complete, totalBytes=%lu mReset=%d",
                  (unsigned long) imageProcessor->mParams.downloadedBytes, mReset);
  ```

- **`HandleApply`** (line ~215): trace right before the soft reset so you
  know the device actually got there:

  ```cpp
  ChipLogProgress(SoftwareUpdate, "HandleApply: mReset=%d, about to reset", mReset);
  ```

  If you see `HandleApply: mReset=0` in the logs the firmware was never told
  the update is done -> the new image was not written.

- **`ConfirmCurrentImage`** (line ~121): print both sides of the comparison
  *before* deciding it failed:

  ```cpp
  ChipLogProgress(SoftwareUpdate,
      "ConfirmCurrentImage: running=%lu, target=%lu",
      (unsigned long) currentVersion, (unsigned long) targetVersion);
  ```

### 5.2 Default driver - to see which boot path is taken

File: `src/app/clusters/ota-requestor/DefaultOTARequestorDriver.cpp`,
`DefaultOTARequestorDriver::Init` (line ~67).

Trace which branch is taken on boot (`IsFirstImageRun` vs idle vs other):

```cpp
ChipLogProgress(SoftwareUpdate,
    "OTA driver Init: IsFirstImageRun=%d, state=%d",
    mImageProcessor->IsFirstImageRun(),
    (int) mRequestor->GetCurrentUpdateState());
```

If `IsFirstImageRun` is `false` after a reboot that was supposed to apply an
update, then the device never actually entered the `kApplying` state before
the reset, which means `Apply()` was never called - look upstream in
`DefaultOTARequestor.cpp`.

### 5.3 Default requestor - to see why a query failed

File: `src/app/clusters/ota-requestor/DefaultOTARequestor.cpp`.

Useful spots (search for the existing `ChipLogError` lines and add
`ChipLogProgress` next to them):

- `OnQueryImageResponse` - prints the offered version, URI and update token.
- `StartDownload` / `BDXMessageReceived` / `OnDownloadTimeout`.
- `ApplyUpdate` and `OnApplyUpdateResponse` - prints what the Provider
  decided (`Proceed` / `AwaitNextAction` / `Discontinue`).

### 5.4 Increase the existing log level

The CHIP `[SWU]` category is `SoftwareUpdate`. Make sure detail logs are
enabled. In `slc/config/sl_matter_config.h` (or via the SLC `--configuration` build flag), set:

```cpp
#define CHIP_CONFIG_LOG_MODULE_SoftwareUpdate 1
#define CHIP_DETAIL_LOGGING                   1
#define CHIP_PROGRESS_LOGGING                 1
```

That alone often surfaces calls like `ChipLogDetail(SoftwareUpdate, ...)`
that are silently dropped by default.

### 5.5 Provider-side traces

Running the Linux provider with `--trace-decode 1` and a higher log level
gives a per-message dump:

```bash
chip-ota-provider-app \
    --discriminator 22 --secured-device-port 5540 \
    --KVS /tmp/chip_kvs_provider \
    --filepath ./light-app.ota \
    --trace-decode 1
```

You can also temporarily add traces in:

- `examples/ota-provider-app/ota-provider-common/OTAProviderExample.cpp`
  (`HandleQueryImage`, `HandleApplyUpdateRequest`).
- `examples/ota-provider-app/ota-provider-common/BdxOtaSender.cpp`
  for per-block BDX status.

---

## 6. Suggested diagnosis order for this failure

Given the logs (`Current software version = 1, expected = 2`):

1. **Check the image header version**

   After adding the trace to `ProcessHeader` you should see (during the BDX
   download, in the device log):
   `Image Header software version: 2 payload size: ...`
   If you see `1`, the OTA file was generated with the wrong `--vn`.

2. **Check that the new firmware actually has version 2 baked in**

   In `slc/config/sl_matter_config.h` or via SLC `--configuration`, make sure
   the build that produced the OTA firmware used version 2, for example:

   ```bash
   --configuration CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION:2,CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING:\"2\"
   ``` The boot log line
   `[SVR] Current Software Version: 1` confirms what the device currently
   thinks it is - if after the OTA you still see `Current Software
   Version: 1`, the running firmware is unchanged.

3. **Check that `Apply()` actually rebooted the device**

   With the trace in `HandleApply`, look for `HandleApply: mReset=1, about to
   reset`. If `mReset=0`, the last chunk of the image never returned
   `SL_STATUS_SI91X_FW_UPDATE_DONE`, which means the SiWx firmware update
   engine was not happy with the image (typically: wrong RPS format,
   truncated payload, signature mismatch).

4. **Check the provider's image**

   ```bash
   src/app/ota_image_tool.py show ./light-app.ota
   ```

   The reported `Software Version` should be the same as the one printed by
   the `ProcessHeader` trace and the same as `CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION`
   inside the OTA binary.

---

## 7. Quick reference - building and serving an OTA for the Wi-Fi Light App

1. Bump the version in `slc/config/sl_matter_config.h` or pass SLC
   `--configuration CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION:2,...` (e.g. from 1 to 2).
2. Rebuild the Light App. The build produces an `.rps` (SiWx917 SoC) or
   `.gbl` (EFR32) image.
3. Wrap it in a Matter `.ota` file:

   ```bash
   src/app/ota_image_tool.py create \
       -v 0xFFF1 -p 0x8005 \
       -vn 2 -vs "2.0" \
       -da sha256 \
       light-app.rps light-app.ota
   ```

4. Start the provider on the same network:

   ```bash
   chip-ota-provider-app --discriminator 22 \
       --secured-device-port 5540 \
       --KVS /tmp/chip_kvs_provider \
       --filepath ./light-app.ota
   ```

5. Commission the provider with `chip-tool`, then announce it to the
   Requestor:

   ```bash
   chip-tool pairing onnetwork 1 20202021
   chip-tool otasoftwareupdaterequestor announce-ota-provider \
       1 0 0 0 <requestor-node-id> 0
   ```

6. Watch the device log for the `[SWU]` lines. The end state should be:

   ```
   [SWU] HandleApply: mReset=1, about to reset
   ... reboot ...
   [SVR] Current Software Version: 2
   [SWU] ConfirmCurrentImage: running=2, target=2
   ```
