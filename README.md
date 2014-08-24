#taco - task coroutines

taco is a C++ multithreaded task processing library built around coroutines. This means that each task is fully capable of suspending itself and then being resumed in the future - with its execution state fully intact from where it left off. This resumption can either be unconditional or dependent on some event. The library provides implementations for a couple synchronization primitives built around this functionality, allowing tasks to, for example, acquire a mutex without any danger of the task locking the thread it is executing on. If the mutex is currently held by some other task, then the acquiring task will simply suspend itself, allowing some other task to execute; when the mutex is released it will take up the suspended task, allowing it to be acquired and for that task's execution to move forward.

Internally, each task is not actually a full blown coroutine. Rather, in the strictest sense of the term there is really only one type of coroutine - the scheduler coroutine. An instance of this coroutine simply spins in a loop and tries to execute as many tasks as it can. It does this using a task stealing algorithm that looks very much like what you would see in any mulithreaded task processing system. If there are no more tasks to execute then this coroutine instance will check and see if any other coroutines are scheduled to run, yielding execution to one of them, and only putting the thread to sleep if there are no other coroutines scheduled.

Starting off from initialization of the library, we spin up a number of threads (by default the same number as there are hardware threads) and each of these threads will enter one of these scheduler coroutines. If from here we just schedule tasks that are never going to suspend themselves, then we will never have any more coroutines then that and it really doesn't look all that different from any other task processing system. 

On the other hand, if a task needs to suspend itself for some reason (e.g. waiting on some event to fire) then that task effectively takes ownership of the coroutine instance running it, keeping it from executing other tasks. In this situation we simply create a new instance of the scheduler coroutine (with some logic to reuse existing idle instances) and yield execution to that. At some future point the event will presumably fire, scheduling the coroutine we just suspended and eventually we will make our way back to resuming its execution.

##Basic Usage Examples
-----
The simplest example of using taco would be to simply initialize the library and schedule some basic task:

```cpp
//...
taco::Initialize([] -> void {
	auto task = taco::Start([] -> void {
		printf("Hello world\n");
	});
	task.await();
});
taco::Shutdown();
//...
```

The Initialize function optionally takes as an argument a function; if this argument is passed in then the Initialize function will schedule the function for execution on the main thread and then immediately yield the main thread into the scheduler coroutine. When the function that was passed in completes then the main thread will yield back to the point where we called Initialize.

If we wanted that could have been written instead like this:

```cpp
//...
taco::Initialize();
taco::Schedule([] -> void {
	auto task = taco::Start()[] -> void {
		printf("Hello world\n");
	});
	task.await();
	taco::ExitMain();
}, 0);
taco::EnterMain();
taco::Shutdown();
//...
```

The last argument to a scheduling function always specifies the thread we require the task to execute on (0 is the main thread). After scheduling the task we call EnterMain - which causes the main thread to yield execution to one of the scheduler coroutines which will in turn execute the task we just scheduled to it. The task itself will do its thing and then call ExitMain, causing the scheduler coroutine executing on the main thread to yield execution back to the point we called EnterMain.

It should be noted that while it is prefectly safe to schedule any task from the main thread when it isn't entered into the scheduler, it is not safe to wait on anything from the main thread (e.g. the "task.await()") unless it is currently entered into the scheduler. This might change in the future if people think it would be useful, but I have avoided it for now as it would require some additional complication (and overhead) in the synchronization primitives.

An example of using the synchronization primitives:

```cpp
//...
int counter = 0;
taco::event completion;
taco::mutex sync;
auto fn = [&]() -> {
	while (counter < 1000)
	{
		std::unique_lock<taco::mutex> lock(sync);
		counter++;
	}
	completion.signal();
}
taco::Schedule(fn);
taco::Schedule(fn);
completion.wait();
//...
```

Getting a little more interesting we can look at using the future class to calculate fibonacci:

```cpp
//...
unsigned fibonacci(unsigned n)
{
	if (n < 2) return 1;

	auto a = taco::Start(fibonacci, n - 1);
	auto b = taco::start(fibonacci, n - 2);

	return a + b;
}
auto r = taco::Start(fibonacci, 11);
printf("fibonacci(11) = %u\n", (unsigned) r);
//...
```

Or printing out the fibonacci sequence using generators:

```cpp
auto fibonacci = taco::StartGenerator([]() -> unsigned) {
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
for (unsigned i=0; i<12; i++)
{
	printf("%u ", (unsigned) fibonacci);
}
//...
```

##Dependencies
-----
The only dependancy taco has is another library of mine - basis. Beyond that it provides some build files for use with the tundra2 build system, but there is small enough amount of code I don't imagine it would be difficult to hook it in with whatever else you wanted to use.

##Future
-----
Right now taco includes a first pass implementation of a profiling interface, providing a signal you can hook your own listener in to in order to listen for profiling events and do with them what you will. It also provides a simple listener class that will spit out a json file of the profiling events it records. I am currently working on a browser based viewer for these JSON files that is just about ready (for a first pass at least).

Beyond that I am looking at improving the scheduler logic and adding support for blocking I/O operations. Within the scheduler, the logic surrounding picking whether to execute a shared versus private task/coroutine is not particularly smart and could potentially be improved. My current plan for blocking I/O is to declare a section of code as "blocking" i.e. adding some functions like "taco::BeginBlocking" "taco::EndBlocking" or something; when we hit one of these blocks the current coroutine will be turned over to a thread or set of threads specifically meant for dealing with these blocks, leaving the main threads free to continue with the non-blocking work.
