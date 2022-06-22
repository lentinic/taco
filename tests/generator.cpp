#include <basis/unit_test.h>
#include <taco/taco.h>
#include <algorithm>
#include <iostream>

void test_generators();

BASIS_TEST_LIST_BEGIN()
    BASIS_DECLARE_TEST(test_generators)
BASIS_TEST_LIST_END()

void test_generators()
{
    taco::Initialize();

    taco::Schedule([]() -> void {
        auto fibonacci = taco::StartGenerator("fibonacci", []() -> unsigned {
            unsigned a = 0;
            unsigned b = 1;
            for (;;)
            {
                taco::YieldValue(b);
                unsigned tmp = a + b;
                a = b;
                b = tmp;
            }
        });

        std::cout<<"Fibonacci: [ ";
        for (unsigned i = 0; i < 12; i++)
        {
            BASIS_TEST_VERIFY(!fibonacci.completed());
            std::cout<<fibonacci<<" ";
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
