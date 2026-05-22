# CRTP-Based Sample Apps: Unit Testing Analysis

This is a follow-on analysis to [`CRTP_Refactor.md`](./CRTP_Refactor.md). It
asks a single question:

> Now that the sample apps under `examples/<app-name>/silabs/` use CRTP, can
> we add meaningful unit tests with the existing GN + Pigweed (`pw_unit_test`)
> infrastructure? If so, how — concretely — do we mock collaborators and
> structure delegates?

The short answer is **yes, the refactor unlocks several useful classes of
tests, but it requires some additional injection points to be added to the
default `AppTask` to be fully testable**. The remainder of this page details
which tests are achievable today, which need small extensions, and gives
concrete templates for the mocks/delegates and `BUILD.gn` wiring.

---

## 1. Existing test infrastructure (what we have to work with)

### 1.1 Framework

- The SDK uses **`pw_unit_test`** (Pigweed's GoogleTest implementation) —
  see [`docs/testing/unit_testing.md`](docs/testing/unit_testing.md).
- Tests are written using `TEST(...)` and `TEST_F(Fixture, ...)` and
  compiled into host-side test binaries via the `chip_test_suite()` GN
  template defined in [`build/chip/chip_test_suite.gni`](build/chip/chip_test_suite.gni).
- Test binaries are built and run with:
  ```bash
  scripts/run_in_build_env.sh "./scripts/build/build_examples.py --target linux-x64-tests-clang --quiet build"
  scripts/run_in_build_env.sh "ninja -C out/linux-x64-tests-clang path/to/test:TestName.run"
  ```

### 1.2 Existing Silabs example tests (what is already wired up)

```
examples/platform/silabs/tests/TestSilabsTestEventTrigger.cpp
examples/platform/silabs/wifi/icd/vendor-handlers/tests/TestVendorHandler.cpp
examples/platform/silabs/wifi/icd/vendor-handlers/tests/TestVendorHandlerFactory.cpp
examples/platform/silabs/wifi/icd/vendor-handlers/tests/TestAppleKeychainHandler.cpp
src/platform/silabs/wifi/icd/tests/...
src/platform/silabs/tracing/tests/...
```

These are aggregated under `chip_test_group("silabs_platform_tests")` in
`src/platform/silabs/tests/BUILD.gn`, gated by `sl_build_unit_tests`.

This is **proof that pw_unit_test code under `examples/platform/silabs/`
already builds and runs in the host test environment** — so adding more
tests there does not require new build infrastructure.

### 1.3 An existing CRTP unit test in the codebase

Highly relevant to this analysis:
[`examples/platform/silabs/wifi/icd/vendor-handlers/tests/TestVendorHandler.cpp`](examples/platform/silabs/wifi/icd/vendor-handlers/tests/TestVendorHandler.cpp)
exercises the `VendorHandler<Derived>` CRTP base by **defining a
`MockVendorHandler` directly in the test translation unit**:

```cpp
class MockVendorHandler : public chip::app::Silabs::VendorHandler<MockVendorHandler>
{
public:
    static bool ProcessVendorCaseImpl(chip::app::SubscriptionsInfoProvider *, chip::FabricTable *)
    { wasProcessVendorCaseImplCalled = true; return false; }

    static bool IsMatchingVendorIDImpl(chip::VendorId)
    { wasIsMatchingVendorIDCalled = true; return false; }
};

TEST_F(TestVendorHandler, ProcessVendorCase)
{
    EXPECT_FALSE(chip::app::Silabs::VendorHandler<MockVendorHandler>::ProcessVendorCase(nullptr, nullptr));
    EXPECT_TRUE(wasProcessVendorCaseImplCalled);
    EXPECT_FALSE(wasIsMatchingVendorIDCalled);
}
```

This is exactly the pattern that the `AppTaskImpl<Derived>` refactor
enables for sample apps. The remainder of this page generalizes from it.

### 1.4 Existing reusable mocks

| Mock                                          | Location                                                                | What it stands in for                          |
| --------------------------------------------- | ----------------------------------------------------------------------- | ---------------------------------------------- |
| `chip::TimerDelegateMock`                     | `src/lib/support/TimerDelegateMock.h`                                   | Drop-in `TimerDelegate` with `AdvanceClock`.   |
| `chip::System::Clock::Internal::MockClock`    | `src/system/SystemClock.h`                                              | Mock monotonic clock (and RAII wrapper).       |
| `TestPersistentStorageDelegate`               | `src/lib/support/TestPersistentStorageDelegate.h`                       | In-memory KVS for persistence tests.           |
| `chip::Testing::ClusterTester`, `TestServerClusterContext` | `src/app/server-cluster/testing/`                          | Drives a `DefaultServerCluster` end-to-end.    |
| `chip::Testing::IsAttributesListEqualTo`, `IsAcceptedCommandsListEqualTo` | `src/app/server-cluster/testing/AttributeTesting.h` | Cluster metadata assertions.                   |

These already cover most non-`AppTask` collaborators (timers, clocks,
storage, cluster servers).

---

## 2. What the CRTP refactor enables

Before the refactor, an app's `AppTask` was:

- A **singleton** declared with `static AppTask sAppTask;` inside the class.
- A class with a hard dependency on **CMSIS-RTOS2**, the **Silabs platform**,
  **LCD**, **LEDs**, **OTA**, and the **PlatformMgr** event loop — all
  initialized inside `AppInit()`.
- Hooked from outside by raw function pointers to `AppTask::ButtonEventHandler`
  etc.

That meant any host-side `TEST_F` that touched `AppTask` immediately pulled
in dozens of platform headers that don't compile on Linux.

After the refactor:

- The override surface is **defined as a class template**
  `AppTaskImpl<Derived>` whose dispatch is purely
  `static_cast<Derived *>(this)->FooImpl(...)`. The template can be
  instantiated with any `Derived` — including a mock — without going
  through the customer singleton.
- The `*Impl()` defaults that forward to `AppTask::*` only execute if no
  override is provided. A test mock can supply its own `*Impl()` and never
  trigger the platform code paths.
- The customer singleton (`CustomerAppTask::sAppTask`) is no longer the
  **only** path to call into `*Impl()` — for instance methods, the test can
  construct a mock instance directly and call the public dispatcher on it.

Concretely, this opens up four classes of tests:

1. **Dispatch tests.** Verify the `AppTaskImpl` template routes each
   public entry to the right `*Impl()` and that signature checking works.
2. **Pure-logic helper tests.** Anything in `AppTask` that does not touch
   the FreeRTOS / hardware layer (e.g. effect-id duration mapping in
   `OnTriggerOffWithEffect`) can be lifted into a free function or an
   instance method and tested directly.
3. **Data-model callback tests.** `DMPostAttributeChangeCallback` is the
   single largest pure-data callback (lighting alone has ~120 lines of
   switch logic across OnOff/LevelControl/ColorControl/Identify). It is
   the highest-value target.
4. **Customer-override regression tests.** Compile-time tests that verify
   a `CustomerAppTask` override has the right signature and is actually
   invoked by the dispatcher (mirroring `TestVendorHandler.cpp`).

It does **not**, by itself, make tests of the FreeRTOS-driven main loop
(`AppTaskMain`, `osMessageQueueGet`, etc.) feasible — those still need
seams to be added. See section 5 ("Limitations").

---

## 3. Achievable tests today, with examples

### 3.1 Dispatch correctness for `AppTaskImpl<Derived>`

This is the cheapest, highest-value test. It validates the CRTP plumbing
the same way `TestVendorHandler.cpp` does for `VendorHandler<Derived>`. It
does **not** require linking the real `AppTask.cpp`; instead, the test
provides a stub base.

**Test layout (proposed)**:

```
examples/lighting-app/silabs/tests/
├── BUILD.gn
├── stubs/AppTask.h           # Minimal stub of AppTask (no platform deps)
├── stubs/AppTask.cpp
└── TestAppTaskImplDispatch.cpp
```

**`stubs/AppTask.h`** — declare only what `AppTaskImpl<Derived>` needs:

```cpp
#pragma once
#include <app/ConcreteAttributePath.h>
#include <lib/core/CHIPError.h>

struct AppEvent;
struct OnOffEffect;

class AppTask
{
public:
    static AppTask & GetAppTask();   // Provided by the test fixture, not by platform code.

    virtual CHIP_ERROR AppInit();
    CHIP_ERROR InitLight();

    static void ButtonEventHandler(uint8_t, uint8_t);
    static void OnTriggerOffWithEffect(OnOffEffect *);
    static void LightActionEventHandler(AppEvent *);
    static void LightTimerEventHandler(void *);
    void DMPostAttributeChangeCallback(const chip::app::ConcreteAttributePath &,
                                       uint8_t, uint16_t, uint8_t *);

    // Counters the test reads to verify the dispatcher fell through to the default.
    static int sAppInitCalls;
    static int sButtonCalls;
    // ...
};
```

**`stubs/AppTask.cpp`** — trivial bodies:

```cpp
#include "AppTask.h"
int AppTask::sAppInitCalls = 0;
int AppTask::sButtonCalls = 0;

CHIP_ERROR AppTask::AppInit()        { ++sAppInitCalls; return CHIP_NO_ERROR; }
CHIP_ERROR AppTask::InitLight()      { return CHIP_NO_ERROR; }
void AppTask::ButtonEventHandler(uint8_t, uint8_t) { ++sButtonCalls; }
void AppTask::OnTriggerOffWithEffect(OnOffEffect *) {}
void AppTask::LightActionEventHandler(AppEvent *) {}
void AppTask::LightTimerEventHandler(void *) {}
void AppTask::DMPostAttributeChangeCallback(const chip::app::ConcreteAttributePath &,
                                            uint8_t, uint16_t, uint8_t *) {}
```

**`TestAppTaskImplDispatch.cpp`** — the actual test:

```cpp
#include <pw_unit_test/framework.h>

#include "AppTask.h"
#include <examples/lighting-app/silabs/include/AppTaskImpl.h>

namespace {
struct MockAppTask : public AppTaskImpl<MockAppTask>
{
    static MockAppTask & GetAppTask() { static MockAppTask s; return s; }

    int appInitCalls = 0;
    int buttonCalls  = 0;
    uint8_t lastButton = 0xFF, lastAction = 0xFF;

private:
    friend class AppTaskImpl<MockAppTask>;

    CHIP_ERROR AppInitImpl() { ++appInitCalls; return CHIP_NO_ERROR; }
    void ButtonEventHandlerImpl(uint8_t button, uint8_t action)
    {
        ++buttonCalls; lastButton = button; lastAction = action;
    }
};

class AppTaskImplDispatchTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MockAppTask::GetAppTask().appInitCalls = 0;
        MockAppTask::GetAppTask().buttonCalls  = 0;
        AppTask::sAppInitCalls = 0;
        AppTask::sButtonCalls  = 0;
    }
};

TEST_F(AppTaskImplDispatchTest, AppInit_RoutesToDerivedImpl_NotBaseDefault)
{
    EXPECT_EQ(MockAppTask::GetAppTask().AppInit(), CHIP_NO_ERROR);
    EXPECT_EQ(MockAppTask::GetAppTask().appInitCalls, 1);
    EXPECT_EQ(AppTask::sAppInitCalls, 0);   // base default must not be hit
}

TEST_F(AppTaskImplDispatchTest, ButtonEventHandler_StaticDispatchToDerived)
{
    MockAppTask::ButtonEventHandler(/*button=*/1, /*action=*/2);
    EXPECT_EQ(MockAppTask::GetAppTask().buttonCalls, 1);
    EXPECT_EQ(MockAppTask::GetAppTask().lastButton, 1);
    EXPECT_EQ(MockAppTask::GetAppTask().lastAction, 2);
}

// A non-overriding mock still exercises the base default.
struct PassThroughMock : public AppTaskImpl<PassThroughMock>
{
    static PassThroughMock & GetAppTask() { static PassThroughMock s; return s; }
private:
    friend class AppTaskImpl<PassThroughMock>;
};

TEST_F(AppTaskImplDispatchTest, NoOverride_FallsThroughToAppTaskDefault)
{
    EXPECT_EQ(PassThroughMock::GetAppTask().AppInit(), CHIP_NO_ERROR);
    EXPECT_EQ(AppTask::sAppInitCalls, 1);
}
} // namespace
```

**Notes on this test:**

- The test compiles `AppTaskImpl.h` directly. There is **no FreeRTOS or
  Silabs platform dependency** — `MockAppTask` does not touch any of it.
  The stub `AppTask.{h,cpp}` is the only translation unit that pretends to
  be the real one.
- `AppTask & AppTask::GetAppTask()` is normally provided by
  `CustomerAppTask.cpp`. The test replaces it with a one-liner returning a
  static `MockAppTask`.
- Because `AppTaskImpl<MockAppTask>::ButtonEventHandler` is `static`, the
  static-dispatch macro routes through `Derived::GetAppTask()` (see
  `CRTP_OPTIONAL_STATIC_DISPATCH` in `CRTPHelpers.h`); we therefore
  exercise both the static and instance dispatch flavors.

The same pattern works for `thermostat`, `onoff-plug-app` and
`light-switch-app` after the lighting-app-style refactor — all four
already define `AppTaskImpl<Derived>`.

### 3.2 Compile-time signature-check tests

`CRTP_CHECK_OPTIONAL_IMPL` is a `static_assert`. We can verify it
compiles for the right shape without actually generating a runtime test —
useful as a guard against accidental signature drift in apps that have
many `*Impl()` hooks (e.g. lighting, thermostat).

The simplest way to do this is to have the test `static_assert` on the
same `mfp_sig` machinery the macro uses:

```cpp
#include <examples/platform/silabs/CRTPHelpers.h>

template <typename A, typename B>
constexpr bool kSameSig = std::is_same<typename mfp_sig<A>::type,
                                        typename mfp_sig<B>::type>::value;

static_assert(kSameSig<decltype(&AppTaskImpl<MockAppTask>::AppInit),
                       decltype(&MockAppTask::AppInitImpl)>);
```

This catches a class of bugs the runtime test cannot — e.g. a customer
declaring `CHIP_ERROR AppInitImpl(int)` would compile fine in isolation
and only break at the dispatch site without this guard.

### 3.3 `DMPostAttributeChangeCallback` (data-model callback) tests

This is the **single highest-leverage target**. The function:

- Has no FreeRTOS or hardware dependency *for most branches*. It mostly
  reads fields from `ConcreteAttributePath` and `value`/`size`, switches
  on cluster IDs, and updates static state.
- Is reached from a single edge — `MatterPostAttributeChangeCallback` in
  `BaseApplication.cpp` — which is straightforward to call from a test.

**Caveats observed in the lighting code:**

- The function calls `BaseApplication::GetLCD().WriteDemoUI(sLightOn)`
  guarded by `#ifdef DISPLAY_ENABLED`. With `DISPLAY_ENABLED` undefined
  (the host-test default), this is a no-op — good.
- The function calls `sLightLED.Set(...)`, where `sLightLED` is a
  file-static `LEDWidget` (or `RGBLEDWidget`). On host these are wrapper
  classes around HAL calls; either:
  - Compile against host-stubbed versions of `LEDWidget` (e.g. an
    in-memory variant exposing `bool isOn`), or
  - Pull the LED state out of file-static storage and into the
    `AppTask` instance, then expose a getter for the test.
- `osTimerIsRunning(sLightTimer)` and `osTimerStop(sLightTimer)` would
  need a CMSIS-RTOS2 host stub or the timer interactions need to be
  abstracted (see section 5.1).

**Recommended approach:** make `DMPostAttributeChangeCallback` testable by
extracting the LED+timer dependencies into a small `LightOutputDelegate`
that the test can substitute. Then the test looks like:

```cpp
class FakeLightOutput : public LightOutputDelegate
{
public:
    void Set(bool on) override            { isOn = on; setCalls++; }
    void SetLevel(uint8_t level) override { lastLevel = level; }
    void SetColorXY(uint16_t, uint16_t) override {}
    bool isOn = false; int setCalls = 0; uint8_t lastLevel = 0;
};

TEST_F(DMCallbackTest, OnOff_SetsLight)
{
    FakeLightOutput out;
    AppTask appTask;
    appTask.SetLightOutput(&out);

    uint8_t value = 1;
    appTask.DMPostAttributeChangeCallback(
        ConcreteAttributePath(LIGHT_ENDPOINT, OnOff::Id, OnOff::Attributes::OnOff::Id),
        ZCL_BOOLEAN_ATTRIBUTE_TYPE, sizeof(value), &value);

    EXPECT_TRUE(out.isOn);
    EXPECT_EQ(out.setCalls, 1);
}
```

This is a small structural change to `AppTask` — replace direct
`sLightLED.Set(...)` calls with `mLightOutput->Set(...)` — but it pays
back **immediately** in test coverage and is independent of the CRTP
work. The CRTP refactor doesn't enable this test by itself, but it makes
it easier to add since `AppTask` is already the right place to own the
delegate.

### 3.4 Pure-logic helper tests (no extraction needed)

Any switch-style mapping in `AppTask.cpp` that consumes plain values and
returns a plain value can be tested today by lifting it into a free
function in an internal header. Example from lighting-app:

```cpp
// In AppTask.cpp, an unnamed-namespace helper:
uint32_t OffEffectDurationMs(EffectIdentifierEnum id, uint8_t variant);
```

If this is moved to a header `examples/lighting-app/silabs/include/OffEffectDuration.h`,
the test becomes a one-liner:

```cpp
TEST(OffEffect, DelayedAllOffFastFade) {
    EXPECT_EQ(OffEffectDurationMs(EffectIdentifierEnum::kDelayedAllOff,
                                   to_underlying(DelayedAllOffEffectVariantEnum::kDelayedOffFastFade)),
              800u);
}
```

These are not "CRTP tests" per se but the refactor (which already
re-shaped the code into `AppTask.cpp`) makes them easier to extract.

### 3.5 Customer-override behavior tests

Because `CustomerAppTask` is just a class derived from
`AppTaskImpl<CustomerAppTask>`, a test can:

1. Define a `TestableCustomerAppTask` with `*Impl()` overrides that record
   their inputs (or short-circuit timing-sensitive parts).
2. Drive the public API (e.g. `TestableCustomerAppTask::ButtonEventHandler(0, 1)`)
   and verify the override observed the right inputs.

This is the same pattern as `TestVendorHandler.cpp` (section 1.3) and
generalizes to every sample app.

---

## 4. How to add the `BUILD.gn`

Following the existing `examples/platform/silabs/wifi/icd/vendor-handlers/tests/BUILD.gn`
template, the GN wiring is small:

```python
# examples/lighting-app/silabs/tests/BUILD.gn
import("//build_overrides/build.gni")
import("//build_overrides/chip.gni")
import("//build_overrides/pigweed.gni")
import("${chip_root}/build/chip/chip_test_group.gni")
import("${chip_root}/build/chip/chip_test_suite.gni")

chip_test_suite("apptask_impl_tests") {
  output_name = "libLightingAppTaskImplTests"

  test_sources = [ "TestAppTaskImplDispatch.cpp" ]

  sources = [
    "stubs/AppTask.cpp",
    "stubs/AppTask.h",
  ]

  public_deps = [
    "${chip_root}/src/lib/core",
    "${chip_root}/src/lib/core:string-builder-adapters",
    "${chip_root}/src/lib/support",
    "${chip_root}/src/lib/support:testing",
  ]

  include_dirs = [
    ".",
    "${chip_root}/examples/lighting-app/silabs/include",
    "${chip_root}/examples/platform/silabs",
  ]

  cflags = [ "-Wconversion" ]
}
```

Then aggregate it into the existing `silabs_platform_tests` group in
`src/platform/silabs/tests/BUILD.gn`:

```python
chip_test_group("silabs_platform_tests") {
  tests = []
  if (sl_build_unit_tests) {
    tests += [
      "${chip_root}/examples/platform/silabs/tests:examples_tests",
      "${chip_root}/examples/platform/silabs/wifi/icd/vendor-handlers/tests:vendor_handlers_tests",
      "${chip_root}/src/platform/silabs/wifi/icd/tests:wifi_icd_tests",
      "${chip_root}/src/platform/silabs/tracing/tests:tracing_tests",
      # New:
      "${chip_root}/examples/lighting-app/silabs/tests:apptask_impl_tests",
    ]
  }
}
```

Run with:

```bash
scripts/run_in_build_env.sh \
  "ninja -C out/linux-x64-tests-clang \
   examples/lighting-app/silabs/tests:apptask_impl_tests.run"
```

(Verification note: I have not built this. The wiring matches the
`vendor-handlers/tests/BUILD.gn` pattern that does build today; the
specific include-dir set may need a small tweak depending on what
`AppTaskImpl.h` transitively pulls in.)

---

## 5. Limitations and what is needed to go further

Some test classes are **not** practical with CRTP alone. For each, the
required structural change is small but real.

### 5.1 The CMSIS-RTOS2 / FreeRTOS surface

`AppTask::AppTaskMain`, `LightTimerEventHandler`, and the various
`osTimer*`/`osMessageQueue*`/`osThread*` calls in `BaseApplication.cpp`
are not host-buildable as-is. Two clean options:

- **Inject a `TimerDelegate`** (we already have `TimerDelegateMock`).
  Replace `osTimerNew/osTimerStart/osTimerStop` calls with
  `mTimerDelegate->Start(...)` etc. The `OnOffCluster` already does this:
  ```cpp
  OnOffCluster mCluster{ kTestEndpointId, { .timerDelegate = mMockTimerDelegate } };
  ```
- **Inject an `EventQueue` interface** for `PostEvent`. The mock can
  capture posted events and verify them, instead of running an actual
  queue.

These changes are independent of CRTP but pair very well with it: each
hook in `AppTaskImpl<Derived>` becomes much easier to test once the
`AppTask` defaults can be invoked without spinning up a real RTOS.

### 5.2 LCD / LED / Display

These are referenced via file-static `LEDWidget`/`RGBLEDWidget` objects
and `BaseApplication::GetLCD()`. Two options, in increasing order of
cost:

- **Keep `#ifdef DISPLAY_ENABLED` guards.** Host tests build with
  `DISPLAY_ENABLED` undefined and the LCD calls vanish. This already
  works for the LCD branch of `DMPostAttributeChangeCallback`.
- **Provide a minimal host `LEDWidget` shim** under
  `examples/platform/silabs/test_stubs/` that records `Set()`/`SetLevel()`
  calls. This is a one-time investment of ~80 lines.

### 5.3 `OnOffServer`, `LevelControl`, `ColorControl` accessors

`AppTask::InitLight()` pulls attribute values via the cluster
`Attributes::*::Get(...)` accessors. Those need a `DataModelProvider`
to be installed. The `chip::Testing::TestServerClusterContext` already
sets one up — see `src/app/server-cluster/testing/`. So a fixture that
includes the right cluster servers and uses `TestServerClusterContext`
gets that for free.

### 5.4 Customer-singleton coupling

`AppTask::GetAppTask()` is **defined** in `CustomerAppTask.cpp` (and
returns `CustomerAppTask::sAppTask`). For a unit test that wants to use
its own derived class as the singleton, it must:

- Not link `examples/platform/silabs/customer/CustomerAppTask.cpp`, and
- Provide its own definition of `AppTask & AppTask::GetAppTask()`
  returning the test's mock.

This is fine in practice (the test BUILD.gn just doesn't add that source),
but it's a constraint to be aware of: you cannot mix-and-match the real
`CustomerAppTask` with a test mock in the same binary.

---

## 6. Recommended priorities

Ranked by cost-benefit:

1. **Dispatch tests for each app's `AppTaskImpl<Derived>`** (section 3.1).
   - Cost: ~100 lines per app, no source changes, reuses an established
     pattern (`TestVendorHandler.cpp`).
   - Benefit: catches dispatcher regressions, signature drift, and
     accidental "didn't actually override" bugs across all 4 currently
     refactored apps. Adds first-class CI coverage of the CRTP layer.
2. **Compile-time signature checks** (section 3.2) — bundle these into
   the same test file as #1; near-zero extra cost.
3. **Pure-logic helper tests** (section 3.4). Low cost per helper; pays
   back over the full lifetime of the app.
4. **`DMPostAttributeChangeCallback` tests** (section 3.3). Highest user-
   facing value, but requires extracting an LED/output delegate from
   `AppTask`. Recommended: do this as part of the per-app refactor for
   the 26Q2-Patch1 apps so it lands together with the CRTP change.
5. **Timer / event-queue injection for `AppTaskMain` testing** (section
   5.1). Largest investment; revisit only if a specific bug class
   justifies it.

If we wanted a single "lighthouse" deliverable to validate the approach,
it would be **(1) + (2) for the lighting app** — that is the smallest
patch that proves out the GN wiring, the stub strategy, and the dispatch
guarantees, and provides a copy-pasteable template for the other apps.

---

## 7. References

- [`CRTP_Refactor.md`](./CRTP_Refactor.md) — companion explanation of the refactor.
- [`docs/testing/unit_testing.md`](docs/testing/unit_testing.md) — pw_unit_test how-to in this SDK.
- Existing CRTP test in tree:
  [`examples/platform/silabs/wifi/icd/vendor-handlers/tests/TestVendorHandler.cpp`](examples/platform/silabs/wifi/icd/vendor-handlers/tests/TestVendorHandler.cpp).
- Existing reusable mocks:
  - `src/lib/support/TimerDelegateMock.h`
  - `src/system/SystemClock.h` (`Internal::MockClock`, `RAIIMockClock`)
  - `src/lib/support/TestPersistentStorageDelegate.h`
  - `src/app/server-cluster/testing/` (cluster test context + helpers)
- BUILD.gn templates:
  - `examples/platform/silabs/wifi/icd/vendor-handlers/tests/BUILD.gn`
  - `examples/platform/silabs/tests/BUILD.gn`
  - `src/platform/silabs/tests/BUILD.gn` (test group aggregator)
- CRTP dispatch macros: `examples/platform/silabs/CRTPHelpers.h`.
