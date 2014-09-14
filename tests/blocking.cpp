#include <basis/unit_test.h>
#include <taco/taco.h>
#include <algorithm>
#include <iostream>

void test_blocking();

BASIS_TEST_LIST_BEGIN()
	BASIS_DECLARE_TEST(test_blocking)
BASIS_TEST_LIST_END()

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define FILE_COUNT 64
#define MAX_FILE_DIM 2048
#define MAX_TASK_COUNT 512

void test_function(bool blocking, unsigned ntasks, unsigned filelen)
{
	taco::future<uint32_t> * tasks = new taco::future<uint32_t>[ntasks];
	for (unsigned i=0; i<ntasks; i++)
	{
			tasks[i] = taco::Start([=]() -> uint32_t {
			uint32_t * data = nullptr;
			if (blocking) { taco::BeginBlocking(); }
			{
				char tmp[255];
				sprintf(tmp, "tmp\\blocking_data%d.dat", i % FILE_COUNT);
				FILE * file = fopen(tmp, "rb");
				data = new uint32_t[filelen];
				fread(data, 4, filelen, file);
				fclose(file);
			}
			if (blocking) { taco::EndBlocking(); }

			taco::future<uint32_t> mins[4];
			for (size_t j = 0; j < 4; j++)
			{
				mins[j] = taco::Start([=]() -> uint32_t {
					uint32_t result = 0xffffffff;
					size_t start = j * (filelen / 4);
					size_t end = (j + 1) * (filelen / 4);
					for (size_t k=start; k<end; k++)
					{
						result = MIN(data[k], result);
					}
					return result;
				});
			}	

			uint32_t r = MIN(MIN(mins[0], mins[1]), MIN(mins[2], mins[3]));
			delete [] data;
			return r;
		});
	}
	
	for (int i=0; i<ntasks; i++)
	{
		BASIS_TEST_VERIFY_MSG(tasks[i] == 0, "Should have equaled 0!");
	}

	delete [] tasks;
}

void test_blocking()
{
	taco::Initialize([&]() -> void {
		printf("Task Count\tFile Dim\tEnabled\tDisabled\tRelative\n");
		for (unsigned tcount=128; tcount<=MAX_TASK_COUNT; tcount+=64)
		{
			for (unsigned dim=128; dim<=MAX_FILE_DIM; dim+=64)
			{
				auto start = basis::GetTimestamp();
				test_function(true, tcount, dim*dim);
				auto enabled = basis::GetTimeDeltaMS(start, basis::GetTimestamp());

				start = basis::GetTimestamp();
				test_function(false, tcount, dim*dim);
				auto disabled = basis::GetTimeDeltaMS(start, basis::GetTimestamp());

				printf("%u\t%u\t%u\t%u\t%.2f\n", tcount, dim, enabled, disabled, enabled / (float) disabled);
			}
		}
	});

	taco::Shutdown();
}

int main(int argc, char * argv[])
{
	BASIS_UNUSED(argc, argv);

	uint32_t * data = new uint32_t[MAX_FILE_DIM * MAX_FILE_DIM];
	for (uint32_t i=0; i<(MAX_FILE_DIM*MAX_FILE_DIM); i++)
	{
		data[i] = i;
	}
	for (int i=0; i<FILE_COUNT; i++)
	{
		char tmp[256];
		sprintf(tmp, "tmp\\blocking_data%d.dat", i);
		FILE * dummy = fopen(tmp, "wb");
		fwrite(data, 4, MAX_FILE_DIM*MAX_FILE_DIM, dummy);
		fclose(dummy);
	}
	delete [] data;

	BASIS_RUN_TESTS();

	return 0;
}
