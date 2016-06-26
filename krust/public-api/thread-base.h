#ifndef KRUST_THREAD_BASE_H_INCLUDED_E26EF
#define KRUST_THREAD_BASE_H_INCLUDED_E26EF

// Copyright (c) 2016 Andrew Helge Cox
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Internal includes
#include "krust/public-api/krust-errors.h"

namespace Krust
{
/**
 * @brief A per-thread coordination point for Krust.
 *
 * Higher level facilities of Krust which are thread-aware require an instance
 * of this to exist on the stack when they are called.
 * This is lightweight enough to be constructed on each invocation of a task
 * in a task-parallel job scheduling system or C++ 11 async call without
 * having to have it be persistent on the stacks of the underlying
 * worker threads running the tasks.
 */
class ThreadBase
{
public:

  ThreadBase(ErrorPolicy * errorPolicy);
  ~ThreadBase();
  
  static ThreadBase& Get();
  ErrorPolicy& GetErrorPolicy();
  const ErrorPolicy& GetErrorPolicy() const;
  bool TagValid() const;
private:
  static thread_local ThreadBase* sThreadBase;
  union {
    long double aligner;
    char mTag[64];
  };
  ErrorPolicy* mErrorPolicy;
};

}
#endif // #ifndef KRUST_THREAD_BASE_H_INCLUDED_E26EF
