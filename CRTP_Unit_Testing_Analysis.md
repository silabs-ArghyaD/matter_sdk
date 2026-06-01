# Unit Testing Analysis: `examples/<app-name>/silabs/`

Companion to [`CRTP_Refactor.md`](./CRTP_Refactor.md).

## Scope

This document analyzes **only** source and test code that lives under:

```text
examples/<app-name>/silabs/
```

| In scope | Out of scope (mentioned only when a test must link it) |
| -------- | -------------------------------------------------------- |
| `include/`, `src/`, `tests/`, `BUILD.gn`, `README.md`, `data_model/`, board config under the app’s `silabs/` tree | `examples/platform/silabs/` (`BaseApplication`, `CRTPHelpers.h`, shared `CustomerAppTask`) |
| Per-app `AppTask`, `AppTaskImpl`, managers, UI helpers, `DataModelCallbacks` | `src/platform/silabs/`, host platform drivers |
| Proposed new tests under `examples/<app>/silabs/tests/` | Existing tests under `examples/platform/silabs/tests/` |

Shared platform pieces (`CRTPHelpers.h`, `CustomerAppTask.cpp`) are **dependencies** for CRTP apps, but **unit tests for sample-app behavior should live next to the app** under `examples/<app-name>/silabs/tests/`, not under `examples/platform/silabs/`.

---

## 1. Inventory: all `examples/*/silabs/` trees

There are **25** directories matching `examples/*/silabs/`. Of those, **23** are Matter sample apps with an `include/AppTask.h`. Two are special:

| Path | Role |
| ---- | ---- |
| `examples/platform/silabs/` | Shared platform (not an app) — excluded from per-app analysis |
| `examples/zigbee-matter-light/silabs/`, `examples/zigbee-matter-thermostat/silabs/` | ZCL/config only (no `src/*.cpp`) |

### 1.1 CRTP-refactored apps (have `include/AppTaskImpl.h`)

These four apps define the customer override surface in **`examples/<app>/silabs/include/AppTaskImpl.h`** and set `CHIP_SILABS_APP_USE_CUSTOMER_APP_TASK` in `include/CHIPProjectConfig.h`. They link shared `CustomerAppTask.cpp` from platform via `BUILD.gn` (not stored under the app tree).

| App | `src/` files | `AppTask.cpp` lines | `*Impl()` hooks |
| --- | ------------ | ------------------- | --------------- |
| [lighting-app](examples/lighting-app/silabs/) | `AppTask.cpp` | ~579 | 8 (`AppInit`, `InitLight`, 5 static handlers, `DMPostAttributeChangeCallback`; + `LightControlEventHandler` if RGB LED) |
| [thermostat](examples/thermostat/silabs/) | `AppTask.cpp`, `ThermostatUI.cpp` | ~352 / UI separate | 8 |
| [onoff-plug-app](examples/onoff-plug-app/silabs/) | `AppTask.cpp` | ~245 | 5 |
| [light-switch-app](examples/light-switch-app/silabs/) | `AppTask.cpp`, `ShellCommands.cpp` | ~807 / shell separate | 7 |

**There are currently zero unit tests under any `examples/<app>/silabs/tests/` directory.**

### 1.2 Legacy apps (no `AppTaskImpl.h` yet)

All other sample apps with `silabs/` still use the pre-CRTP layout: `AppTask` owns a file-local or class-static singleton, optional separate `DataModelCallbacks.cpp`, and often a `*Manager.cpp` for device logic.

| App | `src/*.cpp` count | Typical `silabs/src/` layout |
| --- | ----------------- | ---------------------------- |
| air-quality-sensor-app | 4 | `AppTask`, `DataModelCallbacks`, `SensorManager`, `AirQualitySensorUI` |
| base-platform-app | 1 | `AppTask` only |
| chef | 3 | `AppTask`, `DataModelCallbacks`, `LightingManager` |
| closure-app | 4 | `AppTask`, `ClosureManager`, `ClosureUI`, `DataModelCallbacks` |
| dishwasher-app | 9 | `AppTask`, managers/delegates, `DataModelCallbacks`, helpers |
| evse-app | 1 | `AppTask` |
| fan-control-app | 4 | `AppTask`, `FanControlManager`, `FanControlUI`, `DataModelCallbacks` |
| lit-icd-app | 2 | `AppTask`, `DataModelCallbacks` |
| lock-app | 5 | `AppTask`, `LockManager`, `DataModelCallbacks`, migration, shell |
| multi-sensor-app | 4 | `AppTask`, `SensorManager`, `SensorsUI`, `DataModelCallbacks` |
| oven-app | 6 | `AppTask`, `OvenManager`, binding/UI/delegates, `DataModelCallbacks` |
| pump-app | 3 | `AppTask`, `PumpManager`, `DataModelCallbacks` |
| rangehood-app | 4 | `AppTask`, `RangeHoodManager`, `RangeHoodUI`, `DataModelCallbacks` |
| refrigerator-app | 6 | `AppTask`, `RefrigeratorManager`, UI, shell, mode helper, `DataModelCallbacks` |
| smoke-co-alarm-app | 3 | `AppTask`, `SmokeCoAlarmManager`, `DataModelCallbacks` |
| template | 2 | `AppTask`, `DataModelCallbacks` |
| wake-on-matter | 1 | `AppTask` (+ `iostream_uart.c`) |
| water-heater-app | 1 | `AppTask` |
| window-app | 4 | `AppTask`, `WindowManager`, `LcdPainter`, `DataModelCallbacks` |

Until these apps gain `AppTaskImpl.h` under their own `silabs/include/`, CRTP dispatch tests do not apply; testing targets are the **managers**, **helpers**, and **extractable pure logic** inside `silabs/src/`.

---

## 2. Typical layout under `examples/<app>/silabs/`

```text
examples/<app-name>/silabs/
├── BUILD.gn                 # Firmware target; lists silabs/src + platform deps
├── README.md                # Override docs (CRTP apps)
├── include/
│   ├── AppTask.h            # Default sample behavior API
│   ├── AppTaskImpl.h        # CRTP layer (refactored apps only)
│   ├── AppEvent.h           # Often extends BaseAppEvent from platform
│   ├── AppConfig.h          # Endpoints, feature flags
│   ├── CHIPProjectConfig.h  # CHIP_SILABS_APP_USE_CUSTOMER_APP_TASK (CRTP apps)
│   └── *Manager.h / *UI.h  # App-specific logic (many legacy apps)
├── src/
│   ├── AppTask.cpp          # Main app thread, buttons, cluster sync
│   ├── DataModelCallbacks.cpp # Legacy DM callbacks (being folded into AppTask)
│   └── *Manager.cpp / *UI.cpp
├── data_model/              # ZAP / .matter (not unit-tested here)
└── tests/                   # Proposed — does not exist today
    ├── BUILD.gn
    ├── stubs/               # Minimal AppTask for dispatch-only tests
    └── Test*.cpp
```

**What customers edit after CRTP:** copy `CustomerAppTask` into the app’s `include/` and `src/` (documented in each CRTP app README). Tests should treat **`examples/<app>/silabs/src/AppTask.cpp`** as the reference implementation and **`AppTaskImpl.h`** as the contract.

---

## 3. Framework: `pw_unit_test` + GN (host tests)

- Tests use **`pw_unit_test`** (`#include <pw_unit_test/framework.h>`) and the SDK’s **`chip_test_suite()`** template ([`build/chip/chip_test_suite.gni`](build/chip/chip_test_suite.gni)).
- Run via the linux-x64-tests target, e.g.:
  ```bash
  scripts/run_in_build_env.sh \
    "ninja -C out/linux-x64-tests-clang \
     examples/lighting-app/silabs/tests:apptask_impl_tests.run"
  ```
- Full SDK guidance: [`docs/testing/unit_testing.md`](docs/testing/unit_testing.md).

**Registration:** Today, Silabs example tests are registered from `src/platform/silabs/tests/BUILD.gn` but only for **`examples/platform/silabs/**`**, not for per-app trees. Adding app tests requires either:

1. A new aggregator, e.g. `examples/silabs_sample_app_tests.gni` listing each `examples/<app>/silabs/tests:...`, or  
2. Extending `src/platform/silabs/tests/BUILD.gn` with explicit paths to each app’s `silabs/tests` target.

Keep **sources and `BUILD.gn` under the app’s `silabs/tests/`**; the platform file should only reference them.

### 3.1 Reusable mocks (SDK — not under `examples/`)

Use these as `public_deps` from app `silabs/tests/BUILD.gn` when testing logic that touches Matter clusters or time:

| Mock | Use when testing code in `silabs/src/` that… |
| ---- | --------------------------------------------- |
| `chip::TimerDelegateMock` | Uses timers (effects, deferred persistence) |
| `chip::System::Clock::Internal::MockClock` / `RAIIMockClock` | Depends on monotonic time |
| `chip::Testing::TestServerClusterContext`, `ClusterTester` | Calls `Attributes::*::Get` / `OnOffServer` from `AppTask.cpp` |
| `TestPersistentStorageDelegate` | Touches KVS (e.g. lock migration) |

---

## 4. CRTP apps: what to test inside `silabs/`

For each refactored app, the testable surface under **`examples/<app>/silabs/`** splits into three layers.

### 4.1 Layer A — `AppTaskImpl.h` dispatch (no hardware)

**Goal:** Prove public entry points call `Derived::*Impl()` and that defaults forward to `AppTask::*`.

**Method:** Do **not** compile the real `silabs/src/AppTask.cpp` in the test. Instead:

1. Add `examples/<app>/silabs/tests/stubs/AppTask.{h,cpp}` with trivial bodies and call counters.
2. Include the real `examples/<app>/silabs/include/AppTaskImpl.h`.
3. Define `MockAppTask : public AppTaskImpl<MockAppTask>` in `TestAppTaskImplDispatch.cpp`.
4. Provide `AppTask & AppTask::GetAppTask()` in the test (do **not** link platform `CustomerAppTask.cpp`).

This pattern mirrors how CRTP is tested elsewhere in the tree, but the **test tree stays under the app’s `silabs/` folder**.

**Per-app hook checklist** (copy into each app’s test file):

<details>
<summary><code>lighting-app/silabs</code></summary>

| Public API | `*Impl()` | Static? |
| ---------- | --------- | ------- |
| `AppInit()` | `AppInitImpl` | instance |
| `InitLight()` | `InitLightImpl` | instance |
| `ButtonEventHandler` | `ButtonEventHandlerImpl` | static |
| `OnTriggerOffWithEffect` | `OnTriggerOffWithEffectImpl` | static |
| `LightActionEventHandler` | `LightActionEventHandlerImpl` | static |
| `LightTimerEventHandler` | `LightTimerEventHandlerImpl` | static |
| `LightControlEventHandler` | `LightControlEventHandlerImpl` | static (RGB) |
| `DMPostAttributeChangeCallback` | `DMPostAttributeChangeCallbackImpl` | instance |

</details>

<details>
<summary><code>thermostat/silabs</code></summary>

| Public API | `*Impl()` |
| ---------- | --------- |
| `AppInit` | `AppInitImpl` |
| `InitThermostat` | `InitThermostatImpl` |
| `InitSensor` | `InitSensorImpl` |
| `GetTemperature` | `GetTemperatureImpl` |
| `ButtonEventHandler` | `ButtonEventHandlerImpl` |
| `SensorTimerEventHandler` | `SensorTimerEventHandlerImpl` |
| `TemperatureUpdateEventHandler` | `TemperatureUpdateEventHandlerImpl` |
| `DMPostAttributeChangeCallback` | `DMPostAttributeChangeCallbackImpl` |

`ThermostatUI.cpp` is display-only; host-test with `DISPLAY_ENABLED` off or stub LCD.

</details>

<details>
<summary><code>onoff-plug-app/silabs</code></summary>

| Public API | `*Impl()` |
| ---------- | --------- |
| `AppInit` | `AppInitImpl` |
| `InitPlug` | `InitPlugImpl` |
| `ButtonEventHandler` | `ButtonEventHandlerImpl` |
| `OnOffActionEventHandler` | `OnOffActionEventHandlerImpl` |
| `DMPostAttributeChangeCallback` | `DMPostAttributeChangeCallbackImpl` |

Smallest CRTP app (~245 lines in `AppTask.cpp`) — best second lighthouse after lighting.

</details>

<details>
<summary><code>light-switch-app/silabs</code></summary>

| Public API | `*Impl()` |
| ---------- | --------- |
| `AppInit` | `AppInitImpl` |
| `InitLightSwitch` | `InitLightSwitchImpl` |
| `ButtonEventHandler` | `ButtonEventHandlerImpl` |
| `AppEventHandler` | `AppEventHandlerImpl` |
| `InitBindingHandler` | `InitBindingHandlerImpl` |
| `LightSwitchChangedHandler` | `LightSwitchChangedHandlerImpl` |
| `DMPostAttributeChangeCallback` | `DMPostAttributeChangeCallbackImpl` |

`ShellCommands.cpp` is a separate test target if shell parsing is worth covering.

</details>

**Compile-time guard:** Include `CRTPHelpers.h` (platform) only for `static_assert` on `mfp_sig` matching `AppInitImpl` vs default — catches customer signature drift in `CustomerAppTask` copies stored under the app’s `include/`.

### 4.2 Layer B — Logic inside `silabs/src/AppTask.cpp`

These tests **do** compile selected parts of the real `AppTask.cpp` (or extracted headers from it). Highest value per app:

| App | Function / area in `AppTask.cpp` | Testability | Blocker in `silabs/src` |
| --- | -------------------------------- | ----------- | ------------------------ |
| lighting-app | `DMPostAttributeChangeCallback` | Medium | File-static `LEDWidget`, `osTimer*` |
| lighting-app | `OnTriggerOffWithEffect` duration mapping | High if extracted | Currently inline switch → extract to `include/OffEffectDuration.h` |
| lighting-app | `LightActionEventHandler` | Low on host | `PlatformMgr`, cluster accessors |
| thermostat | `DMPostAttributeChangeCallback`, `GetTemperature` | Medium | Sensor HAL in `InitSensor` |
| onoff-plug-app | `DMPostAttributeChangeCallback`, `OnOffActionEventHandler` | Medium | `LEDWidget`, `OnOffServer` |
| light-switch-app | Binding/switch handlers | Low | `OperationalDeviceProxy`, network |

**Delegate pattern (recommended change, scoped to app tree):**

Add under `examples/<app>/silabs/include/`, e.g. `AppOutputDelegate.h`:

```cpp
class AppOutputDelegate {
public:
    virtual void SetLightOn(bool on) = 0;
    virtual ~AppOutputDelegate() = default;
};
```

Wire `AppTask` to `mOutput` instead of file-static `sLightLED`. Tests under `silabs/tests/` supply `FakeAppOutput`. This keeps production code and tests **inside the same app directory**.

### 4.3 Layer C — Other `silabs/src/*.cpp` (non-CRTP-specific)

| File pattern | Example apps | Unit test approach |
| ------------ | -------------- | ------------------ |
| `*Manager.cpp` | lock-app, rangehood-app, closure-app | Test state machine / action APIs with mocked HAL interfaces declared in `silabs/include/*Manager.h` |
| `DataModelCallbacks.cpp` | lock-app, rangehood-app, … (legacy) | Test callback bodies in isolation until merged into `AppTask.cpp` on CRTP migration |
| `*UI.cpp` | thermostat, rangehood-app | Low priority on host; mostly LCD drawing |
| `LockMigration.cpp` | lock-app | High value with `TestPersistentStorageDelegate` |
| `ShellCommands.cpp` | light-switch-app | Parse/execute commands with mocked `AppTask` if shell is stabilized |

---

## 5. Legacy apps: testing before CRTP migration

For apps **without** `silabs/include/AppTaskImpl.h`, prioritize code that already lives in **`silabs/`** and does not require the CRTP layer.

### 5.1 Example: `lock-app/silabs/`

```text
examples/lock-app/silabs/
├── include/AppTask.h, LockManager.h, ...
└── src/
    ├── AppTask.cpp          (~404 lines) — RTOS, buttons, cluster schedule
    ├── LockManager.cpp      — lock state machine (strong test candidate)
    ├── DataModelCallbacks.cpp
    ├── LockMigration.cpp    — KVS migration (strong test candidate)
    └── EventHandlerLibShell.cpp
```

| Target | Location | Suggested test |
| ------ | -------- | -------------- |
| Lock state transitions | `LockManager.cpp` | `examples/lock-app/silabs/tests/TestLockManager.cpp` with fake bolt driver |
| Migration paths | `LockMigration.cpp` | `TestLockMigration.cpp` + `TestPersistentStorageDelegate` |
| `AppTask` button → action | `AppTask.cpp` | After CRTP: dispatch tests; before: extract `ActionRequest` validation |

`AppTask` still uses `static AppTask sAppTask` in the header — host tests must not link full `AppTask.cpp` until CRTP lands or stubs are provided.

### 5.2 Example: `rangehood-app/silabs/`

```text
src/AppTask.cpp, RangeHoodManager.cpp, RangeHoodUI.cpp, DataModelCallbacks.cpp
```

Test **`RangeHoodManager`** logic under `silabs/tests/` first; keep `RangeHoodUI.cpp` out of host build. When this app is refactored for 26Q2, add `AppTaskImpl.h` beside existing headers and move DM logic from `DataModelCallbacks.cpp` into `AppTask.cpp` (same pattern as lighting).

### 5.3 Manager-heavy apps (dishwasher, oven, refrigerator, closure)

These have **more testable surface in `*Manager.cpp` / delegate `.cpp`** than in `AppTask.cpp`:

- `examples/dishwasher-app/silabs/src/DishwasherManager.cpp`, energy delegates  
- `examples/oven-app/silabs/src/OvenManager.cpp`, `OvenBindingHandler.cpp`  
- `examples/closure-app/silabs/src/ClosureManager.cpp` (noted in rollout plan as needing **two** CRTP layers — plan separate `ClosureManagerImpl` under `silabs/include/` when migrating)

---

## 6. Proposed `BUILD.gn` (per app, under `silabs/tests/`)

Template for **lighting-app** (other CRTP apps: duplicate and change paths/hook stubs):

```python
# examples/lighting-app/silabs/tests/BUILD.gn
import("//build_overrides/chip.gni")
import("${chip_root}/build/chip/chip_test_suite.gni")

chip_test_suite("apptask_impl_tests") {
  output_name = "libLightingAppSilabsTests"

  test_sources = [ "TestAppTaskImplDispatch.cpp" ]

  sources = [
    "stubs/AppTask.cpp",
    "stubs/AppTask.h",
  ]

  include_dirs = [
    ".",
    "../include",                                    # AppTaskImpl.h
    "${chip_root}/examples/platform/silabs",         # CRTPHelpers.h only
  ]

  public_deps = [
    "${chip_root}/src/lib/core",
    "${chip_root}/src/lib/support:testing",
  ]

  cflags = [ "-Wconversion" ]
}
```

**Important:** `../include` pulls in **`examples/lighting-app/silabs/include` only**. Platform headers are include-path dependencies, not “the code under test.”

Optional second target for manager logic:

```python
chip_test_suite("lock_manager_tests") {
  output_name = "libLockAppSilabsTests"
  test_sources = [ "TestLockManager.cpp" ]
  public_deps = [
    "${chip_root}/examples/lock-app/silabs:lock_manager",  # if split into source_set
    ...
  ]
}
```

Today each app’s logic is only a `silabs_executable` source list — you may need a small `source_set("app_logic")` in `examples/<app>/silabs/BUILD.gn` exporting `LockManager.cpp` etc. for tests **without** linking the firmware ELF.

---

## 7. Limitations (within `examples/<app>/silabs/`)

| Limitation | Affects | Mitigation (keep changes under app `silabs/`) |
| ---------- | ------- | --------------------------------------------- |
| `AppTaskMain` + `osMessageQueueGet` | All apps | Do not host-test main loop; test posted handlers via mock queue in `silabs/tests/stubs/` |
| File-static LEDs/timers in `AppTask.cpp` | lighting, onoff-plug, … | `AppOutputDelegate` in `silabs/include/` |
| `DataModelCallbacks.cpp` still separate | lock, rangehood, window, … | Test file as-is; delete when merged into `AppTask.cpp` on CRTP port |
| `CustomerAppTask.cpp` linked from platform | CRTP apps’ `BUILD.gn` | Test binary omits it; test provides `GetAppTask()` |
| Cluster `Attributes::*::Get` in `Init*` | Most `AppTask.cpp` | `TestServerClusterContext` in test deps, or extract init behind interface |
| `ShellCommands.cpp` / `*UI.cpp` | light-switch, many legacy | Separate low-priority targets or skip on host |

---

## 8. Recommended priorities (by app directory)

### CRTP apps (`silabs/include/AppTaskImpl.h` exists)

| Priority | App | Action under `examples/<app>/silabs/tests/` |
| -------- | --- | ------------------------------------------- |
| P0 | lighting-app | `TestAppTaskImplDispatch.cpp` + stubs |
| P0 | onoff-plug-app | Same (smallest `AppTask.cpp`) |
| P1 | thermostat | Dispatch + optional `GetTemperatureImpl` with mocked sensor |
| P1 | light-switch-app | Dispatch; defer binding tests |
| P2 | All four | Extract pure helpers to `silabs/include/` + unit tests |
| P2 | All four | `DMPostAttributeChangeCallback` after output delegate |

### Legacy apps (no `AppTaskImpl.h` yet)

| Priority | App | Action under `silabs/` |
| -------- | --- | ---------------------- |
| P0 | lock-app | `tests/TestLockManager.cpp`, `tests/TestLockMigration.cpp` |
| P1 | rangehood-app, fan-control-app | `tests/Test*Manager.cpp` |
| P1 | closure-app, oven-app, dishwasher-app | Manager/delegate tests before dual-CRTP design |
| P2 | template, lit-icd-app | Minimal `AppTask` tests after CRTP port |

### When porting an app to CRTP (add under same `silabs/` tree)

1. Add `include/AppTaskImpl.h` (hook list from app README).  
2. Fold `src/DataModelCallbacks.cpp` into `src/AppTask.cpp` if present.  
3. Add `CHIP_SILABS_APP_USE_CUSTOMER_APP_TASK` to `include/CHIPProjectConfig.h`.  
4. Add `silabs/tests/` dispatch suite **before** merging to main.  
5. Update app `silabs/README.md` override table.

---

## 9. Summary

| Question | Answer for `examples/<app>/silabs/` only |
| -------- | ---------------------------------------- |
| Are there tests today? | **No** under any `examples/<app>/silabs/tests/`. |
| Which apps support CRTP dispatch tests? | **4:** lighting, thermostat, onoff-plug, light-switch. |
| What is the highest-value first test? | Per-app `TestAppTaskImplDispatch.cpp` using stubs in `silabs/tests/stubs/`. |
| What is the highest-value legacy test? | `*Manager.cpp` / `LockMigration.cpp` under each app’s `silabs/src/`. |
| Where should tests live? | **`examples/<app-name>/silabs/tests/`**, registered from a top-level GN group. |
| What stays out of scope? | Platform/shared code except as include/link dependency (`CRTPHelpers.h`, `CustomerAppTask` for firmware only). |

---

## 10. References (in-repo)

| Resource | Path |
| -------- | ---- |
| CRTP refactor overview | [`CRTP_Refactor.md`](./CRTP_Refactor.md) |
| Unit testing guide | [`docs/testing/unit_testing.md`](docs/testing/unit_testing.md) |
| GN test template | [`build/chip/chip_test_suite.gni`](build/chip/chip_test_suite.gni) |
| CRTP app READMEs | `examples/{lighting-app,thermostat,onoff-plug-app,light-switch-app}/silabs/README.md` |
| Dispatch macros (dependency) | `examples/platform/silabs/CRTPHelpers.h` |
| Per-app sources | `examples/<app-name>/silabs/include/`, `examples/<app-name>/silabs/src/` |
