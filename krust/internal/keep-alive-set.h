#ifndef KRUST_KEEP_ALIVE_SET_H
#define KRUST_KEEP_ALIVE_SET_H

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
#include "krust/public-api/intrusive-pointer.h"

// External includes:
#include <vector>
#include <algorithm>

namespace Krust {

using RefObjectPtr = IntrusivePointer<RefObject>;

/**
 * @brief A set of reference counted objects that are kept alive as long as the
 * set is.
 *
 * The lifecycle of this class is very simple. It is created, references are
 * added to it and it is destroyed.
 */
class KeepAliveSet
{
  KeepAliveSet& operator=(const KeepAliveSet& rhs) = delete;
  KeepAliveSet(const KeepAliveSet& rhs) = delete;
public:
  KeepAliveSet(){}

  /**
   * @return Number of objects being kept alive.
   */
  size_t Size() const { return mKeepAlives.size(); } 

  /**
   * @brief Hold a reference to the object passed in and thereby keep it alive.
   */
  void Add(const RefObject& obj);

  /**
   * @brief Allow anything being kept alive to die if no other reference is held to it.
   */
  void Clear();

private:
  std::vector<RefObjectPtr> mKeepAlives;
};

} /* namespace Krust */

#endif /* KRUST_KEEP_ALIVE_SET_H */
