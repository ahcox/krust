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
#include "krust/public-api/krust-assertions.h"

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
  constexpr ConditionalValue(Value value, bool condition) :
    mValue(value), mCondition(condition) {}
  constexpr operator bool() const { return mCondition; }
  constexpr operator Value() const { return mValue; }
  constexpr Value GetValue() { return mValue; }
private:
  Value mValue;
  bool mCondition;
};

constexpr inline void ConditionalValuetester01()
{
  constexpr ConditionalValue<unsigned> zero_true{0u, true};
  constexpr ConditionalValue<unsigned> one_false{1u, false};
  constexpr ConditionalValue<unsigned> zero_false{0u, false};
  constexpr ConditionalValue<unsigned> one_true{1u, true};

  KRUST_COMPILE_ASSERT(zero_true,   "ConditionValue should be true");
  KRUST_COMPILE_ASSERT(!one_false,  "ConditionValue should be false");
  KRUST_COMPILE_ASSERT(!zero_false, "ConditionValue should be false");
  KRUST_COMPILE_ASSERT(one_true,    "ConditionValue should be true");
}

} // namespace Krust

#endif //KRUST_CONDITIONAL_VALUE_H
