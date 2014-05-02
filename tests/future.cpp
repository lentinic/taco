#include <basis/bitvector.h>
#include <basis/unit_test.h>
#include <taco/taco.h>
#include <algorithm>
#include <iostream>

void test_futures();

BASIS_TEST_LIST_BEGIN()
	BASIS_DECLARE_TEST(test_futures)
BASIS_TEST_LIST_END()

int odd(int);
int even(int);

int fibonacci(int n)
{
	if (n < 2) return n;

	auto a = taco::Start(fibonacci, n - 1);
	auto b = taco::Start(fibonacci, n - 2);

	return a + b;
}

void test_futures()
{
	taco::Initialize();

	taco::Schedule([]() -> void {
		std::vector<taco::future<int>> results;
		for (int i=1; i<12; i++)
		{
			results.push_back(taco::Start(fibonacci, i));
		}
		int k = 1;
		for (auto r : results)
		{
			std::cout<<"Fibonacci("<<k<<"):\t"<<r<<std::endl;
			k++;
		}
		taco::ExitMain();
	}, 0);

	taco::EnterMain();
	taco::Shutdown();
}

int main()
{
	BASIS_RUN_TESTS();
	return 0;
}
