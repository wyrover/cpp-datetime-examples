#include <stdio.h>  
#include "gtest/gtest.h"    
#include "fun.h"  

TEST(fun, add)
{
	EXPECT_EQ(1, add(2, -1));
	EXPECT_EQ(5, add(2, 3));
}

TEST(fun, fun)
{
	EXPECT_LT(-2, fun(1, 2));
	EXPECT_EQ(-1, fun(1, 2));
	ASSERT_LT(-2, fun(1, 2));
	ASSERT_EQ(-1, fun(1, 2));
}

int main(int argc, wchar_t* argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}