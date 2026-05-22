# Matter Sample App CRTP Refactor (26Q2)

This page explains the CRTP-based refactor introduced in
[Matter Project Upgrades: 26Q2 Scope & Rollout Plan](https://confluence.silabs.com/pages/viewpage.action?pageId=831337686)
and walks through the concrete changes already merged in
`examples/<app-name>/silabs/` and `examples/platform/silabs/`.

It is intended for engineers reviewing the refactor or porting an app to the
new pattern. It is **not** a customer-facing override guide; for that, see
each app's `examples/<app-name>/silabs/README.md`.

---

## 1. Background: why a refactor was needed

The original 26Q2 scope was project upgrade support for the
[Matter Sample Apps](https://confluence.silabs.com/pages/viewpage.action?pageId=831337686)
so that customer projects could be upgraded reliably without manual source
file migration.

While designing project upgrades, the team observed that today's sample apps
mix four distinct kinds of code:

- Legacy code,
- Open-source CSA framework logic,
- Simulation-specific code,
- Silicon Labs hardware-specific sample app logic.

Exposing every existing function as an override-point would create a large,
unstable customer API surface — and most of those functions are not the kind
of thing customers should be overriding long-term.

The decision was therefore to **refactor the sample apps first**, exposing
only the minimum required hooks, and to use the **Curiously Recurring
Template Pattern (CRTP)** as the override mechanism.

26Q2 scope was reduced to 8 of 15 apps (the highest-impact ones based on
Studio + usage data); the remaining 7 are scheduled for 26Q2-Patch1.

| 26Q2 (refactored)  | 26Q2-Patch1 (planned) |
| ------------------ | --------------------- |
| Lighting           | Multi-sensor          |
| CMP                | Closure (needs 2 CRTPs) |
| On-Off Plug        | Fan Control           |
| Thermostat         | Refrigerator          |
| Lock               | EVSE                  |
| Light Switch       | Oven                  |
| Rangehood          |                       |
| Platform Template  |                       |
| Air Quality (stretch) |                    |

Refactor + project-upgrade support are merged together per app, validated
with SQA smoke testing before merging, since merges are happening through
RC1 / RC2.

---

## 2. CRTP in 60 seconds

CRTP is a static-polymorphism pattern where a base class is parameterised
by its derived class:

```cpp
template <typename Derived>
class Base
{
public:
    void DoThing() { static_cast<Derived *>(this)->DoThingImpl(); }
};

class Derived : public Base<Derived>
{
private:
    friend class Base<Derived>;
    void DoThingImpl() { /* customer behavior */ }
};
```

Calls go `Base::DoThing -> Derived::DoThingImpl` resolved **at compile time**.
Compared to virtual functions, CRTP gives:

- No vtable, no indirect call — relevant on flash-constrained MCUs.
- No runtime cost when no override is provided.
- Compile-time signature checking of overrides (we use a `static_assert` to
  enforce this — see `CRTP_CHECK_OPTIONAL_IMPL` below).

The downsides — only one derived class per base, no runtime dispatch — fit
this use case perfectly: there is exactly one customer `AppTask` per build.

---

## 3. The pattern as implemented in the SDK

The refactor introduces three artifacts plus a small set of dispatch macros.

### 3.1 Three-class layering

For each refactored sample app under `examples/<app-name>/silabs/`:

| Class                             | Lives in                                              | Owns                                                                      |
| --------------------------------- | ----------------------------------------------------- | ------------------------------------------------------------------------- |
| `BaseApplication`                 | `examples/platform/silabs/BaseApplication.{h,cpp}`    | Cross-app behavior (event queue, status LED, factory reset, LCD, etc.).   |
| `AppTask` *(per app)*             | `examples/<app>/silabs/include/AppTask.h`, `src/AppTask.cpp` | Silicon Labs default sample-app behavior for that device type.            |
| `AppTaskImpl<Derived>` *(per app)*| `examples/<app>/silabs/include/AppTaskImpl.h`         | CRTP dispatch layer. Each public entry point dispatches to `Derived::*Impl()`. |
| `CustomerAppTask`                 | `examples/platform/silabs/customer/CustomerAppTask.{h,cpp}` | Single, shared template for the customer's overrides. Derives from `AppTaskImpl<CustomerAppTask>`. |

The relationship is:

```
BaseApplication  <-  AppTask  <-  AppTaskImpl<CustomerAppTask>  <-  CustomerAppTask
```

### 3.2 The dispatch helper

All apps share `examples/platform/silabs/CRTPHelpers.h`. It provides:

- `CRTP_OPTIONAL_DISPATCH(Base, Derived, func)` — instance method, returns a value, no args.
- `CRTP_OPTIONAL_DISPATCH_ARGS(Base, Derived, func, ...)` — instance method, returns a value, forwarded args.
- `CRTP_OPTIONAL_VOID_DISPATCH(Base, Derived, func, ...)` — instance method, void, forwarded args.
- `CRTP_OPTIONAL_STATIC_DISPATCH(Base, Derived, func, ...)` — static method, void, forwarded args.
- `CRTP_OPTIONAL_CONST_DISPATCH_ARGS(Base, Derived, func, ...)` — const instance method, returns a value, forwarded args.
- `CRTP_REQUIRED_DISPATCH(Base, Derived, func)` — same idea, but no base default; missing override is a compile error.

Each macro:

1. Statically asserts that the `Derived::func` signature matches `Base::func`
   (`CRTP_CHECK_OPTIONAL_IMPL`), so a typo or wrong type produces a clear
   compiler error rather than silently falling back to the default.
2. Forwards to `Derived` via `static_cast<Derived *>(this)` (instance) or
   `static_cast<Derived &>(Derived::GetAppTask())` (static).

`CRTPHelpers.h` is the single place this dispatch logic lives — every
refactored app reuses it.

### 3.3 Concrete example: lighting app

**`examples/lighting-app/silabs/include/AppTask.h`** — declares the default
implementation as plain (non-virtual, with one `override` for `AppInit`):

```cpp
class AppTask : public BaseApplication
{
public:
    static AppTask & GetAppTask();
    static void AppTaskMain(void * pvParameter);
    CHIP_ERROR StartAppTask();

    static void ButtonEventHandler(uint8_t button, uint8_t btnAction);
    static void OnTriggerOffWithEffect(OnOffEffect * effect);
    static void LightActionEventHandler(AppEvent * aEvent);
    static void LightTimerEventHandler(void * timerCbArg);
    void DMPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath,
                                       uint8_t type, uint16_t size, uint8_t * value);
    // ...

protected:
    CHIP_ERROR AppInit() override;   // BaseApplication has AppInit() = 0
    CHIP_ERROR InitLight();
};
```

**`examples/lighting-app/silabs/include/AppTaskImpl.h`** — the CRTP layer.
Public entry points dispatch to `Derived::*Impl()`; private defaults forward
to `AppTask::*`:

```cpp
template <typename Derived>
class AppTaskImpl : public AppTask
{
public:
    CHIP_ERROR AppInit() override { CRTP_OPTIONAL_DISPATCH(AppTaskImpl, Derived, AppInitImpl); }
    CHIP_ERROR InitLight()        { CRTP_OPTIONAL_DISPATCH(AppTaskImpl, Derived, InitLightImpl); }

    static void ButtonEventHandler(uint8_t button, uint8_t btnAction)
    {
        CRTP_OPTIONAL_STATIC_DISPATCH(AppTaskImpl, Derived, ButtonEventHandlerImpl, button, btnAction);
    }
    static void OnTriggerOffWithEffect(OnOffEffect * effect)
    {
        CRTP_OPTIONAL_STATIC_DISPATCH(AppTaskImpl, Derived, OnTriggerOffWithEffectImpl, effect);
    }
    // ... other entry points elided ...

    void DMPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath,
                                       uint8_t type, uint16_t size, uint8_t * value)
    {
        CRTP_OPTIONAL_VOID_DISPATCH(AppTaskImpl, Derived, DMPostAttributeChangeCallbackImpl,
                                    attributePath, type, size, value);
    }

private:
    friend Derived;

    CHIP_ERROR AppInitImpl()   { return AppTask::AppInit(); }
    CHIP_ERROR InitLightImpl() { return AppTask::InitLight(); }
    void ButtonEventHandlerImpl(uint8_t button, uint8_t btnAction)
    {
        AppTask::ButtonEventHandler(button, btnAction);
    }
    void OnTriggerOffWithEffectImpl(OnOffEffect * effect) { AppTask::OnTriggerOffWithEffect(effect); }
    void DMPostAttributeChangeCallbackImpl(const chip::app::ConcreteAttributePath & attributePath,
                                           uint8_t type, uint16_t size, uint8_t * value)
    {
        AppTask::DMPostAttributeChangeCallback(attributePath, type, size, value);
    }
    // ...
};
```

**`examples/platform/silabs/customer/CustomerAppTask.h`** — the customer's
empty derived class. Single shared file across all refactored apps:

```cpp
#include "AppTaskImpl.h"

class CustomerAppTask : public AppTaskImpl<CustomerAppTask>
{
public:
    static CustomerAppTask & GetAppTask() { return sAppTask; }

private:
    friend class AppTaskImpl<CustomerAppTask>;
    static CustomerAppTask sAppTask;
};
```

**`examples/platform/silabs/customer/CustomerAppTask.cpp`** — defines the
singleton and resolves the static `AppTask::GetAppTask()` to the customer
instance:

```cpp
CustomerAppTask CustomerAppTask::sAppTask;

AppTask & AppTask::GetAppTask()
{
    return CustomerAppTask::GetAppTask();
}
```

When the customer wants to override `ButtonEventHandler`, they:

1. Copy the template `CustomerAppTask.{h,cpp}` into their app's `include/`
   and `src/` folders.
2. Add `void ButtonEventHandlerImpl(uint8_t, uint8_t);` under `private:` in
   `CustomerAppTask.h`.
3. Implement it in `CustomerAppTask.cpp`.
4. Update paths in the app's `BUILD.gn` — that is the only build change.

Any `*Impl()` that is not overridden keeps the SiLabs default by virtue of
the private default forwarding to `AppTask::*`.

---

## 4. What changed concretely (lighting app reference commit)

Reference commit: `b3a541ac` — _"[Silabs] : Lighting App CRTP refactor (#71752)"_.

| File                                                            | Change                                                                                              |
| --------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| `examples/platform/silabs/CRTPHelpers.h`                        | New. Dispatch macros + signature-check `static_assert`.                                             |
| `examples/platform/silabs/customer/CustomerAppTask.{h,cpp}`     | New. Shared empty `CustomerAppTask` template + `AppTask::GetAppTask` definition.                    |
| `examples/platform/silabs/BaseApplication.cpp`                  | Adds `MatterPostAttributeChangeCallback` that routes to `CustomerAppTask::GetAppTask().DMPost...` when `CHIP_SILABS_APP_USE_CUSTOMER_APP_TASK` is defined. |
| `examples/platform/silabs/Rpc.cpp`                              | Pigweed RPC button entry point routes through `CustomerAppTask::ButtonEventHandler` instead of `AppTask::ButtonEventHandler` when CRTP is enabled. |
| `examples/lighting-app/silabs/include/AppTask.h`                | Stripped down. Removed singleton `static AppTask sAppTask`; `GetAppTask()` is now a free function defined in `CustomerAppTask.cpp`. |
| `examples/lighting-app/silabs/include/AppTaskImpl.h`            | New. CRTP layer for lighting (10 hooks).                                                            |
| `examples/lighting-app/silabs/src/AppTask.cpp`                  | Absorbed `DataModelCallbacks.cpp` (via `DMPostAttributeChangeCallback`). Removed `LightingManager`; light state now lives in unnamed-namespace statics in `AppTask.cpp`. Static handlers now invoke `CustomerAppTask::*` for indirection points (button callback, OnOff effect, etc.). |
| `examples/lighting-app/silabs/src/DataModelCallbacks.cpp`       | Deleted. Logic moved into `AppTask::DMPostAttributeChangeCallback`.                                 |
| `examples/lighting-app/silabs/src/LightingManager.{h,cpp}`      | Deleted (~500 lines). Subsumed into `AppTask`.                                                      |
| `examples/lighting-app/silabs/include/CHIPProjectConfig.h`      | Adds `#define CHIP_SILABS_APP_USE_CUSTOMER_APP_TASK` to opt the app into the CRTP path.             |
| `examples/lighting-app/silabs/BUILD.gn`                         | Adds `CustomerAppTask.cpp` from the platform-shared `customer/` folder; adds include path.          |
| `examples/lighting-app/silabs/README.md`                        | New "Extending Base App Implementation" section: how to override APIs, list of `*Impl()` hooks.     |

Net diff: roughly +922 / -881 lines, but most of the deletion is the old
`LightingManager` indirection that the refactor made unnecessary.

The same shape applies to the four apps already merged with CRTP:

```
examples/lighting-app/silabs/include/AppTaskImpl.h
examples/thermostat/silabs/include/AppTaskImpl.h
examples/onoff-plug-app/silabs/include/AppTaskImpl.h
examples/light-switch-app/silabs/include/AppTaskImpl.h
```

The lock app, refrigerator app, oven app, etc. (where `AppTaskImpl.h` does
not yet exist under `examples/<app-name>/silabs/include/`) are still on the
legacy direct-`AppTask` model — matching the 26Q2-Patch1 schedule.

---

## 5. Override surface per app

Each app's `AppTaskImpl<Derived>` declares only the methods that make sense
for that device type. For example:

| App           | `*Impl()` hooks (subset)                                                                                                                                    |
| ------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| lighting      | `AppInitImpl`, `InitLightImpl`, `ButtonEventHandlerImpl`, `OnTriggerOffWithEffectImpl`, `LightActionEventHandlerImpl`, `LightTimerEventHandlerImpl`, `LightControlEventHandlerImpl` (RGB), `DMPostAttributeChangeCallbackImpl` |
| thermostat    | `AppInitImpl`, `InitThermostatImpl`, `InitSensorImpl`, `GetTemperatureImpl`, `ButtonEventHandlerImpl`, `SensorTimerEventHandlerImpl`, `TemperatureUpdateEventHandlerImpl`, `DMPostAttributeChangeCallbackImpl` |
| onoff-plug    | `AppInitImpl`, `InitPlugImpl`, `ButtonEventHandlerImpl`, `OnOffActionEventHandlerImpl`, `DMPostAttributeChangeCallbackImpl`                                   |
| light-switch  | `AppInitImpl`, `InitLightSwitchImpl`, `ButtonEventHandlerImpl`, `AppEventHandlerImpl`, `InitBindingHandlerImpl`, `LightSwitchChangedHandlerImpl`, `DMPostAttributeChangeCallbackImpl` |

Internal helpers (e.g. lighting's `UpdateOnOffClusterState`,
`PostLightControlColorEvent`) intentionally do **not** appear in
`AppTaskImpl` — they are not part of the customer override contract. This
is the API-surface reduction the refactor was designed to enable.

---

## 6. Notes / gotchas observed in the code

- **Singleton ownership moved.** Previously each app declared
  `static AppTask sAppTask;` privately in its own `AppTask`. After the
  refactor, the singleton is `CustomerAppTask::sAppTask`, and
  `AppTask::GetAppTask()` is defined in `CustomerAppTask.cpp` and returns it.
  This means any tool/test that takes a `static AppTask sAppTask` directly
  will break — callers must go through `AppTask::GetAppTask()` /
  `CustomerAppTask::GetAppTask()`.

- **Static handlers use `CustomerAppTask::Foo`, not `AppTask::Foo`.** Look
  at `AppTask.cpp` after the refactor — places like:
  ```cpp
  chip::DeviceLayer::Silabs::GetPlatform().SetButtonsCb(&CustomerAppTask::ButtonEventHandler);
  ```
  This is intentional: the registered callback must be the CRTP entry point
  so the customer's `ButtonEventHandlerImpl` can override behavior.
  Registering `&AppTask::ButtonEventHandler` would silently bypass the
  override. The same pattern applies to `OnOffEffect.callback`, posted
  `AppEvent::Handler` pointers, RPC button handlers, etc.

- **`MatterPostAttributeChangeCallback` is gated** on
  `CHIP_SILABS_APP_USE_CUSTOMER_APP_TASK`. The macro is set in each
  refactored app's `CHIPProjectConfig.h`; legacy apps keep their existing
  `DataModelCallbacks.cpp` definition.

- **Signature mismatch is a compile error**, not a silent miss. The
  `static_assert` inside `CRTP_CHECK_OPTIONAL_IMPL` compares
  `decltype(&Derived::func)` against `decltype(&Base::func)` and emits a
  message containing the offending function. This is a meaningful
  ergonomic improvement over typical CRTP code.

- **`StartAppTask()` is *not* a hook.** The README explicitly notes that
  platform code (e.g. `MatterConfig`) calls
  `AppTask::GetAppTask().StartAppTask()` with static type `AppTask &`, so
  `StartAppTask` runs the implementation in `AppTask.cpp`, not anything in
  `CustomerAppTask`. To change startup behavior, customers edit
  `BaseApplication::StartAppTask` or `AppTask::StartAppTask`.

---

## 7. Migration checklist (for the 26Q2-Patch1 apps)

Based on the lighting app commit, the per-app migration looks like:

1. Identify what should be a customer hook vs. internal helper. Internal
   helpers stay in unnamed namespaces inside `AppTask.cpp`.
2. Add `examples/<app>/silabs/include/AppTaskImpl.h` with one `*Impl()`
   per public hook, using the existing `CRTPHelpers.h` macros.
3. Drop the `static AppTask sAppTask;` member from `AppTask`. Define
   `AppTask::GetAppTask()` only in `CustomerAppTask.cpp`.
4. In `AppTask.cpp`, change every static-handler reference from
   `&AppTask::Foo` to `&CustomerAppTask::Foo` for the entries that go
   through CRTP. Keep `BaseApplication::*` references unchanged where the
   intent is to bypass the override (e.g. function-button -> base
   `ButtonHandler`).
5. Fold `DataModelCallbacks.cpp` into
   `AppTask::DMPostAttributeChangeCallback`. Delete the old file.
6. Add `#define CHIP_SILABS_APP_USE_CUSTOMER_APP_TASK` to the app's
   `CHIPProjectConfig.h`.
7. In `BUILD.gn`, add `examples/platform/silabs/customer/CustomerAppTask.cpp`
   to `sources` and `examples/platform/silabs/customer` to include dirs.
8. Update the README with the standard "Extending Base App Implementation"
   section copied from the lighting app.
9. Validate via SQA smoke test before merging (per the 26Q2 RC1/RC2
   policy).

The closure app is explicitly called out as needing **two** CRTP layers
(presumably `AppTask` plus a closure-specific manager), which is why it is
deferred to Patch1. The pattern there will likely look like
`ClosureManagerImpl<Derived>` derived from `ClosureManager`, mirroring
`AppTaskImpl<Derived>`.

---

## 8. Customer-impact summary (from the Confluence page)

| Customer scenario                                                  | Outcome                                                                               |
| ------------------------------------------------------------------ | ------------------------------------------------------------------------------------- |
| Project created from a **refactored** sample app in 26Q2           | Project upgrades cleanly going forward.                                               |
| Project created from a **non-refactored** sample app in 26Q2       | Plain SLC project upgrade to 26Q2-Patch1 will not be supported. Manual migration only. |
| New project in 26Q2-Patch1                                         | Uses the refactored structure, upgradable going forward.                              |

Release notes will list, per app, when project upgrade support became
available (26Q2 vs. 26Q2-Patch1).

---

## 9. References

- Confluence: [Matter Project Upgrades: 26Q2 Scope & Rollout Plan](https://confluence.silabs.com/pages/viewpage.action?pageId=831337686)
- Code:
  - `examples/platform/silabs/CRTPHelpers.h`
  - `examples/platform/silabs/customer/CustomerAppTask.{h,cpp}`
  - `examples/lighting-app/silabs/include/AppTaskImpl.h`
  - `examples/lighting-app/silabs/src/AppTask.cpp`
  - `examples/lighting-app/silabs/README.md` ("Extending Base App Implementation")
- Reference commit: `b3a541ac` — _[Silabs] : Lighting App CRTP refactor (#71752)_
- Related per-app refactor commits: `7ad1766c70` (thermostat),
  `298b07d603` (on-off plug), `6e189aefa8` (light switch).
