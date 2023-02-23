#include <basis/unit_test.h>
#include <taco/taco.h>
#include <algorithm>
#include <iostream>
#include <vector>

void test_fib();
void test_fibcollatz();

BASIS_TEST_LIST_BEGIN()
    BASIS_DECLARE_TEST(test_fibcollatz)
    BASIS_DECLARE_TEST(test_fib)
BASIS_TEST_LIST_END()

void test_fibcollatz()
{
    taco::Initialize();

    taco::future<uint32_t> f = taco::Start("fib-collatz", [=](){
        uint32_t p[2] = { 1, 1 };
        uint32_t steps = 12;
        for (int i=0; i<10; i++) {
            for (int i=0; i<steps; i++)
            {
                uint32_t temp = p[0] + p[1];
                p[0] = p[1];
                p[1] = temp;
            }
            for (steps=0; p[1] != 1; steps++)
            {
                p[0] = p[1];
                p[1] = (p[1] & 1) ? (p[1] * 3 + 1) : (p[1] / 2);
            }
        }
        return steps;
    });

    taco::Schedule([&]() -> void {
        BASIS_TEST_VERIFY(f == 104);
        taco::ExitMain();
    }, 0);

    taco::EnterMain();
    taco::Shutdown();
}

unsigned fibonacci(unsigned n)
{
    if (n < 2) return 1;

    auto a = taco::Start("fibonacci", [=](){ return fibonacci(n - 1); });
    auto b = taco::Start("fibonacci", [=](){ return fibonacci(n - 2); });

    return a + b;
}

void test_fib()
{
    taco::Initialize();

    taco::Schedule([]() -> void {
        std::vector<taco::future<uint32_t>> results;
        for (uint32_t i=0; i<12; i++)
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
