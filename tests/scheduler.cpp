#include <basis/assert.h>
#include <basis/timer.h>
#include <taco/taco.h>
#include <iostream>


#define TEST_TIMEOUT_MS 2000

unsigned ErrorCount = 0;

void test_initialize_shutdown()
{
	taco::Initialize();
	taco::Shutdown();
}

void test_schedule()
{
	taco::Initialize();
	int a = 0;
	taco::Schedule([&]() -> void {
		a = 5;
	});

	auto start = basis::GetTimestamp();
	bool timeout = false;
	while (a == 0)
	{
		if (basis::GetTimeElapsedMS(start) > TEST_TIMEOUT_MS)
		{
			timeout = true;
			break;
		}
	}

	BASIS_ASSERT_SIGNAL(a == 100, [=]() -> bool{
		if (timeout)
		{
			std::cout<<"Error: \"test_schedule\" timed out after at least "<<TEST_TIMEOUT_MS<<" ms"<<std::endl;
		}
		else
		{
			std::cout<<"Error: \"test_schedule\", the task ran but set variable \"a\" to an unexpected value ("<<a<<")"<<std::endl;
		}
		ErrorCount++;
		return false;
	}());
}

int main()
{
	auto start = basis::GetTimestamp();
	test_initialize_shutdown();
	test_schedule();

	if (ErrorCount > 0)
	{
		std::cout<<"Scheduler tests completed with "<<ErrorCount<<" error(s) in "<<(basis::GetTimeElapsedMS(start) / 1000.f)<<" seconds."<<std::endl;
	}
	else
	{
		std::cout<<"Scheduler tests completed succesfully in "<<(basis::GetTimeElapsedMS(start) / 1000.0f)<<" seconds."<<std::endl;
	}

	return 0;
}