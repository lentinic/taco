/*
Chris Lentini
http://divergentcoder.io

Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to use, 
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
Software, and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANT*IES OF MERCHANTABILITY, FITNESS 
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#define PUBLIC_TASKQ_CHUNK_SIZE 512
#define PRIVATE_TASKQ_CHUNK_SIZE 512
#define PUBLIC_FIBERQ_CHUNK_SIZE 512
#define PRIVATE_FIBERQ_CHUNK_SIZE 512

#define BLOCKING_FIBERQ_CHUNK_SIZE 128
#define BLOCKING_THREAD_LIMIT 32

#define MUTEX_SPIN_COUNT 50

#define FIBER_STACK_SIZE 16384