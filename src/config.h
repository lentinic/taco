/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
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