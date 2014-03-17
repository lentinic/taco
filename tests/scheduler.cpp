#include <basis/bitvector.h>
#include <basis/unit_test.h>
#include <taco/taco.h>
#include <iostream>
#include <mutex>

#define TEST_TIMEOUT_MS 2000

void test_initialize_shutdown();
void test_schedule();
void test_switch();

BASIS_TEST_LIST_BEGIN()
	BASIS_DECLARE_TEST(test_initialize_shutdown)
	BASIS_DECLARE_TEST(test_schedule)
	BASIS_DECLARE_TEST(test_switch)
BASIS_TEST_LIST_END()

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

	std::function<void(int)> fn = [&](uint32_t expected) -> void {
		uint32_t current = taco::GetSchedulerId();
		BASIS_TEST_VERIFY_MSG(current == expected, "Expected task to run on thread %d; ran on %d",
			expected, current);
	};

	for (uint32_t i=0; i<taco::GetThreadCount(); i++)
	{
		taco::Schedule(std::bind(fn, i), i);
	}

	while (a == 0)
	{
		if (basis::GetTimeElapsedMS(start) > TEST_TIMEOUT_MS)
		{
			timeout = true;
			break;
		}
	}

	taco::Shutdown();

	BASIS_TEST_VERIFY_MSG(!timeout, "Timed out after at least %d ms", TEST_TIMEOUT_MS);
	BASIS_TEST_VERIFY_MSG(timeout || a == 100, "Variable \"a\" set to an unexpected value - %d", a);
}

void test_switch()
{
	taco::Initialize(); 

	std::mutex mutex;
	basis::bitvector did_run(taco::GetThreadCount() + 1);

	// If the switching doesn't work then this task will hog the thread it gets grabbed by
	// So if we schedule more of these tasks than there are threads we'll never see all of them entered
	auto fn = [&](unsigned index) -> void {
		
		{
			std::unique_lock<std::mutex> lock(mutex);
			did_run.set_bit(index, true);
		}

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

	while (!did_run.test_all(true))
	{
		if (basis::GetTimeElapsedMS(start) > TEST_TIMEOUT_MS)
		{
			timeout = true;
			break;
		}
	}

	taco::Shutdown();

	BASIS_TEST_VERIFY_MSG(!timeout, "timeout after at leat %d ms.  Not all tasks were entered.", TEST_TIMEOUT_MS);
}

int main()
{
	BASIS_RUN_TESTS();
	return 0;
}
