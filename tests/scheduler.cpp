#include <basis/assert.h>
#include <basis/timer.h>
#include <basis/bitset.h>
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

	auto start = basis::GetTimestamp();
	bool timeout = false;

	taco::Schedule([&]() -> void {
		a = 100;
	});

	while (a == 0)
	{
		if (basis::GetTimeElapsedMS(start) > TEST_TIMEOUT_MS)
		{
			timeout = true;
			break;
		}
	}

	taco::Shutdown();

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
	});
}

void test_switch()
{
	taco::Initialize(); 

	basis::bitset did_run(taco::GetThreadCount() + 1);

	// If the switching doesn't work then this task will hog the thread it gets grabbed by
	// So if we schedule more of these tasks than there are threads we'll never see all of them entered
	auto fn = [&](unsigned index) -> void {
		did_run.set_or(index, 1);
		for (;;)
		{
			taco::Switch();
		}
	};

	auto start = basis::GetTimestamp();
	bool timeout = false;

	for (unsigned i=0; i<(taco::GetThreadCount() + 1); i++)
	{
		taco::Schedule(std::bind(fn, i));
	}

	while (!did_run.test_all_one())
	{
		if (basis::GetTimeElapsedMS(start) > TEST_TIMEOUT_MS)
		{
			timeout = true;
			break;
		}
	}

	taco::Shutdown();

	BASIS_ASSERT_SIGNAL(!timeout, [=]() -> bool{
		std::cout<<"Error: \"test_switch\" timed out after at least "<<TEST_TIMEOUT_MS<<" ms.  Not all tasks were entered."<<std::endl;
		ErrorCount++;
		return false;
	});
}

int main()
{
	auto start = basis::GetTimestamp();
	test_initialize_shutdown();
	test_schedule();
	test_switch();

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