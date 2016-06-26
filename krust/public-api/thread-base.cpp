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

#include "krust/public-api/thread-base.h"
#include "krust/public-api/logging.h"

#define KRUST_STACK_TAG "cad05be574afec9421590dc141c205e197a1da23c59d821b1d0192a56610f791"

namespace Krust
{

/**
* Each thread that uses Krust has to have a ThreadBase sitting on its stack
* earlier than stack frames using Krust. That base object is pointed to
* by this pointer.
*/
thread_local ThreadBase* ThreadBase::sThreadBase = 0;

bool ThreadBase::TagValid() const
{
  return strcmp(mTag, KRUST_STACK_TAG) == 0;
}

ThreadBase::ThreadBase(ErrorPolicy * errorPolicy)
{
  KRUST_ASSERT1(sThreadBase == 0, "There should only be one ThreadBase live on each thread stack.");
  KRUST_ASSERT1(errorPolicy != 0, "Null error policy.");

  if (!errorPolicy)
  {
    KRUST_LOG_ERROR << "Attempt to create a ThreadBase with a null error policy." << endlog;
  }

  if (sThreadBase == 0)
  {
    sThreadBase = this;
  }
  else
  {
    KRUST_LOG_ERROR << "Attempt to create a second ThreadBase when there is already one on the stack." << endlog;
  }

  mErrorPolicy = errorPolicy;
  memcpy(&mTag[0], KRUST_STACK_TAG, 64U);
}

ThreadBase::~ThreadBase()
{
  KRUST_ASSERT1(sThreadBase != nullptr, "Null (missing) thread base object.");
  KRUST_ASSERT1(sThreadBase == this, "Wrong thread base object.");
  if (sThreadBase == this)
  {
    sThreadBase = 0;
    memset(&mTag[0], 0xAC, sizeof(mTag));
  }
}

ThreadBase & ThreadBase::Get()
{
  KRUST_ASSERT2(sThreadBase != nullptr, "Null thread base pointer.");
  return *sThreadBase;
}
ErrorPolicy & ThreadBase::GetErrorPolicy()
{
  KRUST_ASSERT1(&mErrorPolicy != nullptr, "Null error policy;");
  return *mErrorPolicy;
}
const ErrorPolicy & ThreadBase::GetErrorPolicy() const
{
  KRUST_ASSERT1(&mErrorPolicy != nullptr, "Null error policy;");
  return *mErrorPolicy;
}

} // namespace Krust
