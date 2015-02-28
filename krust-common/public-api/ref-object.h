#ifndef KRUST_PUBLIC_API_REF_OBJECT_H_
#define KRUST_PUBLIC_API_REF_OBJECT_H_

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

// Internal includes:
#include "krust-common/public-api/krust-assertions.h"

// External includes:
#include <atomic>
#include <cstddef>


namespace Krust {

/**
 * @brief Base class for reference counted objects.
 *
 * Allows reasonably efficient lockless cross-thread sharing using atomics to
 * maintain the reference count internally.
 * @note Use virtual inheritance to derive from it if used in a multiple
 * inheritance.
 */
class RefObject
{
public:
  RefObject();

  /**
   * Increment the counter of references to this object.
   */
  void Inc() const;

  /**
   * Decrement and delete this if there are no more references to it.
   */
  void Dec() const;

protected:
  virtual ~RefObject();

private:

  /**
   * Number of references to this object.
   **/
  typedef size_t Counter;
  mutable std::atomic<Counter> mCount;
};

} /* namespace Krust */

#endif /* KRUST_PUBLIC_API_REF_OBJECT_H_ */
