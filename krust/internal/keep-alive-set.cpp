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

#include "keep-alive-set.h"

void Krust::KeepAliveSet::Add(const RefObject & obj)
{
  // Check the existing set for a duplicate and add obj only if there isn't one:
  RefObject& obj_alias = const_cast<RefObject&>(obj);
  const bool found = mKeepAlives.end() != std::find_if(mKeepAlives.begin(), mKeepAlives.end(),
    [&obj_alias](RefObjectPtr& other) { return &obj_alias == other.Get(); });
  if (!found)
  {
    mKeepAlives.push_back(RefObjectPtr(obj_alias));
  }
}
