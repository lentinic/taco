/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

namespace taco
{
    /// Utility function for accessing thread_local data in a manner
    /// that is safe for fibers that switch between threads.
    /// If we just declare some global instance of a class as
    /// thread local and then use it in a function the compiler can
    /// generate assembly that just re-uses the same address for the rest
    /// of the function - but with this fiber infrastructure we might
    /// end up suspended in the middle of the function then resume later
    /// on some other thread. i.e. we are solving this issue:
    ///     void foobar()
    ///     {
    ///         printf("%u\n", scheduler_state->threadId); // access thread local data
    ///         Switch();                                  // suspends fiber
    ///         printf("%u\n", scheduler_state->threadId); // resumes on some other thread
    ///                                                    // but still prints the same id
    ///     }
    /// Uses a volatile ptr to the thread local data to prevent optimization
    /// Note this means there is only one global instance of the structure
    /// per thread - so it is intended for global state type things
    /// Tested as working on clang-1300.0.27.3 arm64
    /// May neeed to adjust as I expand testing to other targets
    template<class storage_t>
    storage_t & thread_state()
    {
        static thread_local storage_t data;
        // needs to be volatile to prevent the compiler from optimizing
        storage_t * volatile ptr = &data;
        return *ptr;
    };
}
