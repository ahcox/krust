#ifndef KRUST_PUBLIC_API_INTRUSIVE_POINTER_H_
#define KRUST_PUBLIC_API_INTRUSIVE_POINTER_H_

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

#include "krust/public-api/krust-assertions.h"
#include "krust/public-api/ref-object.h"

namespace Krust {

/**
 * Pull common operations out of template to save code bloat and to so one
 * profile line for refcount inc and dec.
 */
class IntrusivePointerBase
{
public:
  bool operator!() const { return !mRefObject; }

protected:
  IntrusivePointerBase(RefObject& refObject);
  IntrusivePointerBase(RefObject* refObject);
  ~IntrusivePointerBase();
  void Reset(RefObject* other);

  RefObject* mRefObject;
};

/**
 * Smart pointer to classes derived from RefObject to manage their
 * counters
 */
template<class T>
class IntrusivePointer : public IntrusivePointerBase
{
public:
  IntrusivePointer(const IntrusivePointer<T>& counted) : IntrusivePointerBase(counted.Get()) {}
  IntrusivePointer(T* counted) : IntrusivePointerBase(counted) {}
  IntrusivePointer(T& counted) : IntrusivePointerBase(counted) {}
  IntrusivePointer<T>& operator=(const IntrusivePointer<T>& other)
  {
    Reset(other.Get());
    return *this;
  }

  T* Get() const { return reinterpret_cast<T*>(mRefObject); }
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

  void Reset(T* other){ IntrusivePointerBase::Reset(other); }
};

class TestRefObject : public RefObject
{
public:
  TestRefObject() : mInt(32) {}
  ~TestRefObject() {}
  int mInt;
};

namespace
{
inline bool temp_test()
{
  IntrusivePointer<TestRefObject> tester_ptr = new TestRefObject();
  IntrusivePointer<TestRefObject> tester_ptr2 = tester_ptr;
  IntrusivePointer<TestRefObject> tester_ptr3(tester_ptr);
  tester_ptr->mInt = 64;
  TestRefObject * pTester = tester_ptr.Get();
  IntrusivePointer<TestRefObject> tester_ptr4((TestRefObject&) *pTester);
  tester_ptr2.Reset(pTester);
  tester_ptr3 = tester_ptr4;
  bool isNull = (!tester_ptr);
  return isNull;
}
}

} /* namespace Krust */

#endif /* KRUST_PUBLIC_API_INTRUSIVE_POINTER_H_ */
