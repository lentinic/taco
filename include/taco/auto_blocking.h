/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include "scheduler.h"

namespace taco
{
    class auto_blocking
    {
    public:
        auto_blocking()
        {
            BeginBlocking();
        }

        ~auto_blocking()
        {
            EndBlocking();
        }

        auto_blocking(const auto_blocking &) = delete;
        auto_blocking & operator = (const auto_blocking &) = delete;
    };
}
