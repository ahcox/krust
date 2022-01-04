#ifndef KRUST_SCOPED_TEMP_ARRAY_H_
#define KRUST_SCOPED_TEMP_ARRAY_H_

// Copyright (c) 2016,2021 Andrew Helge Cox
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
#include "krust-kernel/public-api/debug.h"

/// @file On-stack buffer with fallback to use heap.
///
/// @todo Use raw memory for on-stack buffer and default construct only if
/// heap allocation is not done, and only `mSize` elements of it.
///

namespace Krust {

/// The number of bytes to use on the stack before alocation falls-back to the
/// heap. Although a typical page size is used, this is aritrary as nothing is
/// done to align the allocation to a page boundary.
constexpr size_t DEFAULT_MAX_LOCAL_BUFFER_BYTES = 4096ul;

/**
* @brief A temporary which is cleaned up when it goes out of scope and has
* an optimization to avoid dynamic memory allocations for shorter arrays which
* can live on the stack.
*/
template<typename T, unsigned MAX_LOCAL_BUFFER_BYTES>
class ScopedTempArray {
public:
  constexpr static size_t NUM_LOCAL_ELEMS = MAX_LOCAL_BUFFER_BYTES / sizeof(T);
  KRUST_COMPILE_ASSERT(NUM_LOCAL_ELEMS > 0, "Insufficient space reserved in stack local memory.");

  ScopedTempArray(const size_t size) :
      mSize(size)
  {
    if (size <= NUM_LOCAL_ELEMS)
    {
      mArray = &mLocalOptimisation[0];
    }
    else
    {
      mArray = new T[size];
      KRUST_BEGIN_DEBUG_BLOCK
      KRUST_LOG_INFO << "Allocating on heap for ScopedTempArray<" << typeid(T).name() <<">. Size: " << size << "." << endlog;
      KRUST_END_DEBUG_BLOCK
    }

  }

  ~ScopedTempArray()
  {
    if (mArray != &mLocalOptimisation[0])
    {
      delete[] mArray;
    }
  }

  T* Get() {
    return mArray;
  }

  /// Break naming convention to look like a standard container.
  constexpr size_t size() const {
    return mSize;
  }

private:
  // No copying allowed:
  ScopedTempArray(const ScopedTempArray<T, MAX_LOCAL_BUFFER_BYTES>& other) = delete;
  ScopedTempArray& operator=(const ScopedTempArray<T, MAX_LOCAL_BUFFER_BYTES>& other) = delete;

  const size_t mSize;
  /// The memory to free when this goes out of scope:
  T* mArray;
  /// The fixed-size allocation to avoid new/delete for short arrays.
  T mLocalOptimisation[NUM_LOCAL_ELEMS];
};

} /* namespace Krust */

#endif /* KRUST_SCOPED_TEMP_ARRAY_H_ */
