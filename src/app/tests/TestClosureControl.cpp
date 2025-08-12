#include <pw_unit_test/framework.h>
#include "../../../ClosureManager.cpp"

class TestClosureControl : public ::testing::Test {
protected:
    ClosureManager manager;
};

TEST_F(TestClosureControl, InitialStateIsClosed) {
    EXPECT_FALSE(manager.isOpen());
}

TEST_F(TestClosureControl, CanOpenClosure) {
    manager.open();
    EXPECT_TRUE(manager.isOpen());
}

TEST_F(TestClosureControl, CanCloseClosure) {
    manager.open();
    manager.close();
    EXPECT_FALSE(manager.isOpen());
}
