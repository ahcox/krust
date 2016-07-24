#ifndef KRUST_SCOPED_TEMP_ARRAY_H_
#define KRUST_SCOPED_TEMP_ARRAY_H_

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

namespace Krust {

/**
* @brief A temporary which is cleaned up when it goes out of scope and has
* an optimization to avoid dynamic memory allocations for shorter arrays which
* can live on the stack.
*/
template<typename T, unsigned MAX_LOCAL_BUFFER_BYTES>
class ScopedTempArray {
public:
  constexpr static size_t NUM_LOCAL_ELEMS = MAX_LOCAL_BUFFER_BYTES / sizeof(T);
  
  ScopedTempArray(const size_t size)
  {
    if (size <= NUM_LOCAL_ELEMS)
    {
      mArray = &mLocalOptimisation[0];
    }
    else
    {
      mArray = new T[size];
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

private:
  // No copying allowed:
  ScopedTempArray(const ScopedTempArray<T, MAX_LOCAL_BUFFER_BYTES>& other) = delete;
  ScopedTempArray& operator=(const ScopedTempArray<T, MAX_LOCAL_BUFFER_BYTES>& other) = delete;
  
  /// The memory to free when this goes out of scope:
  T* mArray;
  /// The fixed-size allocation to avoid new/delete for short arrays.
  T mLocalOptimisation[NUM_LOCAL_ELEMS];
};

} /* namespace Krust */

#endif /* KRUST_SCOPED_TEMP_ARRAY_H_ */
