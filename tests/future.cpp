#include <basis/unit_test.h>
#include <taco/taco.h>
#include <algorithm>
#include <iostream>

void test_futures();

BASIS_TEST_LIST_BEGIN()
    BASIS_DECLARE_TEST(test_futures)
BASIS_TEST_LIST_END()

unsigned fibonacci(unsigned n)
{
    if (n < 2) return 1;

    auto a = taco::Start("fibonacci", [=](){ return fibonacci(n - 1); });
     auto b = taco::Start("fibonacci", [=](){ return fibonacci(n - 2); });

    return a + b;
}

void test_futures()
{
    taco::Initialize();

    taco::Schedule([]() -> void {
        std::vector<taco::future<unsigned>> results;
        for (unsigned i=0; i<12; i++)
        {
            results.push_back(taco::Start("fibonacci", [=](){ return fibonacci(i); }));
        }

        std::cout<<"Fibonacci: [ ";
        for (auto r : results)
        {
            std::cout<<r<<" ";
        }
        std::cout<<"]"<<std::endl;

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
