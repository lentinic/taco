#include <basis/unit_test.h>
#include <basis/thread_util.h>
#include <taco/taco.h>
#include <algorithm>
#include <iostream>

void test_independent();
void test_dependent();

BASIS_TEST_LIST_BEGIN()
	//BASIS_DECLARE_TEST(test_independent)
	BASIS_DECLARE_TEST(test_dependent)
BASIS_TEST_LIST_END()

std::atomic<uint32_t> timewaiting[2];

void dependent(bool blocking, unsigned ntasks, unsigned sleeptime, unsigned cputime)
{
	taco::future<uint32_t> * tasks = new taco::future<uint32_t>[ntasks];
	for (unsigned i=0; i<ntasks; i++)
	{
		tasks[i] = taco::Start([=]() -> uint32_t {
			if (blocking) { taco::BeginBlocking(); }

			auto t0 = basis::GetTimestamp();
			std::this_thread::sleep_for(std::chrono::microseconds(sleeptime));
			timewaiting[blocking ? 0 : 1] += basis::GetTimestamp() - t0;
			if (blocking) { taco::EndBlocking(); }

			unsigned steps = 0;
			for (unsigned j=0; j<cputime; j++)
			{
				unsigned n = 837799;
				while (n != 1)
				{
					n = (n & 1) ? ((3 * n) + 1) : (n / 2);
					steps++;
				}
			}
			return steps;
		});
	}
	
	for (unsigned i=1; i<ntasks; i++)
	{
		BASIS_TEST_VERIFY_MSG(tasks[i] == tasks[i - 1], "Mismatch between %u and %u (%u vs %u)", 
			i, i - 1, (uint32_t)tasks[i], (uint32_t)tasks[i - 1]);
	}

	delete [] tasks;
}

void independent(bool blocking, unsigned ntasks, unsigned sleeptime, unsigned cputime)
{
	taco::future<void> * blockers = new taco::future<void>[ntasks];
	for (unsigned i=0; i<ntasks; i++)
	{
		blockers[i] = taco::Start([=]() -> void {
			if (blocking) { taco::BeginBlocking(); }

			auto t0 = basis::GetTimestamp();
			std::this_thread::sleep_for(std::chrono::microseconds(sleeptime));
			timewaiting[blocking ? 0 : 1] += basis::GetTimestamp() - t0;
			if (blocking) { taco::EndBlocking(); }
		});
	}
 
	taco::future<unsigned> * calcers = new taco::future<unsigned>[ntasks];
	for (unsigned i=0; i<ntasks; i++)
	{
		calcers[i] = taco::Start([=]() -> unsigned {
			unsigned steps = 0;
			for (unsigned j=0; j<cputime; j++)
			{
				unsigned n = 837799;
				while (n != 1)
				{
					n = (n & 1) ? ((3 * n) + 1) : (n / 2);
					steps++;
				}
			}
			return steps;
		});
	}

	blockers[0].await();
	for (unsigned i=1; i<ntasks; i++)
	{
		blockers[i].await();
		if (calcers[i].await() != calcers[i - 1].await())
		{ 
			printf("Hash mismatch between %u and %u!", i, i - 1);
		}
	}

	delete [] blockers;
	delete [] calcers;
}

void test_dependent()
{
	taco::Initialize([&]() -> void {
		timewaiting[0] = timewaiting[1] = 0;
		printf("Task Count\tcpu\tsleep\tenabled\tdisabled\trelative\n");
		for (unsigned tasks=64; tasks<=512; tasks*=2)
		{
			for (unsigned cpu=64; cpu<=2048; cpu*=4)
			{	
				for (unsigned sleep=16384; sleep<=(1<<18); sleep*=2)
				{
					auto start = basis::GetTimestamp();
					dependent(true, tasks, sleep, cpu);
					auto enabled = basis::GetTimeDeltaMS(start, basis::GetTimestamp());

					start = basis::GetTimestamp();
					dependent(false, tasks, sleep, cpu);
					auto disabled = basis::GetTimeDeltaMS(start, basis::GetTimestamp());

					printf("%u\t%u\t%u\t%u\t%u\t%.2f\n", tasks, cpu, sleep, enabled, disabled, disabled / (float) enabled);
				}
			}
		}
	});

	taco::Shutdown();
}

void test_independent()
{
	taco::Initialize([&]() -> void {
		timewaiting[0] = timewaiting[1] = 0;
		printf("Task Count\tcpu\tsleep\tenabled\tdisabled\trelative\n");
		for (unsigned tasks=64; tasks<=512; tasks*=2)
		{
			for (unsigned cpu=64; cpu<=512; cpu*=2)
			{	
				for (unsigned sleep=1024; sleep<32768; sleep*=2)
				{
					auto start = basis::GetTimestamp();
					independent(true, tasks, sleep, cpu);
					auto enabled = basis::GetTimeDeltaMS(start, basis::GetTimestamp());

					start = basis::GetTimestamp();
					independent(false, tasks, sleep, cpu);
					auto disabled = basis::GetTimeDeltaMS(start, basis::GetTimestamp());

					printf("%u\t%u\t%u\t%u\t%u\t%.2f\n", tasks, cpu, sleep, enabled, disabled, disabled / (float) enabled);
					timewaiting[0] = timewaiting[1] = 0;
				}
			}
		}
	});

	taco::Shutdown();
}

int main(int argc, char * argv[])
{
	BASIS_UNUSED(argc, argv);
	BASIS_RUN_TESTS();
	return 0;
}
