#ifndef KRUST_CONDITIONAL_VALUE_H
#define KRUST_CONDITIONAL_VALUE_H

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

namespace Krust
{

/**
 * @brief A value and a bool indicating whether the value is valid.
 *
 * Used to return Success/Fail to a caller as well as the value it wants.
 */
template <typename Value>
class ConditionalValue
{
public:
  ConditionalValue(Value value, bool condition) :
    mValue(value), mCondition(condition) {}
  operator bool() const { return mCondition; }
  Value GetValue() { return mValue; }
private:
  Value mValue;
  bool mCondition;
};

} // namespace Krust

#endif //KRUST_CONDITIONAL_VALUE_H
