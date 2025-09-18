// filepath: matter_unit_test_template.md
# Matter App Unit Test Template

## Code Template

```cpp
// filepath: src/app/tests/Test{YourComponent}.cpp

#include <lib/core/StringBuilderAdapters.h>
#include <pw_unit_test/framework.h>
#include <app/data-model/Nullable.h>

// Include your component's header file
#include <app/clusters/{your-cluster}/{your-component}.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters::{YourCluster};

// Test Suite: {YourComponent}Validation
TEST({YourComponent}Validation, InitialStateIsCorrect)
{
    {YourComponent} component;
    
    // Verify initial state
    EXPECT_FALSE(component.IsActive());
    EXPECT_EQ(component.GetState(), {YourComponent}::State::kIdle);
}

TEST({YourComponent}Validation, ConfigurationAcceptsValidParameters)
{
    {YourComponent} component;
    
    // Test valid configuration
    auto result = component.Configure(validParam1, validParam2);
    
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_TRUE(component.IsConfigured());
}

TEST({YourComponent}Validation, ConfigurationRejectsInvalidParameters)
{
    {YourComponent} component;
    
    // Test invalid configuration
    auto result = component.Configure(invalidParam1, invalidParam2);
    
    EXPECT_FALSE(result.IsSuccess());
    EXPECT_EQ(result.GetError(), CHIP_ERROR_INVALID_ARGUMENT);
}

TEST({YourComponent}Validation, FeatureEnablementWorksCorrectly)
{
    {YourComponent} component;
    
    // Enable feature
    component.EnableFeature({YourComponent}::Feature::kAdvanced);
    
    EXPECT_TRUE(component.IsFeatureEnabled({YourComponent}::Feature::kAdvanced));
}

TEST({YourComponent}Validation, FeatureDisablementWorksCorrectly)
{
    {YourComponent} component;
    
    // Disable feature
    component.DisableFeature({YourComponent}::Feature::kAdvanced);
    
    EXPECT_FALSE(component.IsFeatureEnabled({YourComponent}::Feature::kAdvanced));
}

TEST({YourComponent}Validation, StateTransitionsAreValid)
{
    {YourComponent} component;
    
    // Test state transition
    component.SetState({YourComponent}::State::kActive);
    
    EXPECT_EQ(component.GetState(), {YourComponent}::State::kActive);
    EXPECT_TRUE(component.IsActive());
}

TEST({YourComponent}Validation, ErrorHandlingIsRobust)
{
    {YourComponent} component;
    
    // Simulate error condition
    auto result = component.PerformOperation();
    
    if (result.IsFailure())
    {
        EXPECT_NE(result.GetError(), CHIP_NO_ERROR);
        EXPECT_FALSE(component.IsOperationComplete());
    }
}
```

## Template Explanation

### Key Components

#### 1. **Standard Headers**
```cpp
#include <lib/core/StringBuilderAdapters.h>
#include <pw_unit_test/framework.h>
#include <app/data-model/Nullable.h>
```
- Essential Matter framework headers for testing
- `pw_unit_test` provides the testing framework
- `StringBuilderAdapters` for string handling utilities

#### 2. **Namespace Usage**
```cpp
using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
```
- Simplifies code by avoiding repetitive namespace prefixes
- Standard pattern across Matter codebase

#### 3. **Test Structure Pattern**
```cpp
TEST({ComponentName}Validation, SpecificTestCase)
```
- **Test Suite**: `{ComponentName}Validation` groups related tests
- **Test Case**: Descriptive name explaining what's being tested

#### 4. **Essential Test Categories**

##### **Initialization Tests**
- Verify default constructor behavior
- Check initial state values
- Ensure proper object creation

##### **Configuration Tests**
- Test valid parameter acceptance
- Verify invalid parameter rejection
- Check configuration state changes

##### **Feature Management Tests**
- Test feature enablement/disablement
- Verify feature state queries
- Check feature interaction logic

##### **State Management Tests**
- Test valid state transitions
- Verify state query methods
- Check state consistency

##### **Error Handling Tests**
- Test error condition responses
- Verify error codes returned
- Check system recovery mechanisms

### Best Practices

1. **One Concept Per Test**: Each test should verify one specific behavior
2. **Clear Setup**: Initialize test objects in a predictable state
3. **Meaningful Names**: Test names should describe the expected behavior
4. **Comprehensive Coverage**: Include positive, negative, and edge cases
5. **Isolated Tests**: Tests should not depend on each