#ifndef KRUST_ERRORS_H_INCLUDED_E26EF
#define KRUST_ERRORS_H_INCLUDED_E26EF

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
#include "krust/public-api/logging.h"
#include "krust/public-api/krust-assertions.h"
#include "krust/public-api/vulkan_types_and_macros.h"

// External includes:

namespace Krust
{

/**
 * @brief Errors which can be reported through the configured ErrorPolicy back
 * to the application in addition to Vulkan API errors.
 */
enum class Errors
{
  /// No error, the default.
  NoError = 0,
  /// A function parameter passed into Krust is not valid (e.g. null pointer or
  /// out of range integer)
  IllegalArgument,
  /// Krust has gotten it's internals messed up - my bad, sorry :'(.
  IllegalState,
  // Maybe add later:
  // ArrayIndexOutOfBounds,
  // UnsupportedOperation
  // Reserve some space to grow into and enforce a minimum size:
  MaxError = 0x7FFFFFFF
};

/**
 * @brief Convert the error enum passed in to a textual representation.
 */
const char* ErrorToString(const Errors& error);

/**
 * @brief  Output textual representation of error to log.
 */
inline LogBuilder& operator << (LogBuilder& logBuilder, const Errors& error)
{
  logBuilder << ErrorToString(error);
  return logBuilder;
}

/**
 * Specialization of the LogBuilder template << operator for Errors.
 */
template<>
inline LogBuilder & LogBuilder::operator<< <Errors>(const Errors &t)
{
  mChannel.GetStream() << ErrorToString(t);
  return *this;
}

/**
  * Used by Krust internally to report errors as they happen and defer the policy
  * for error handling to the library user.
  *
  * Reasonable reporting strategies for the using module (user) include:
  * - Throw an exception for the user to catch.
  * - Log error and terminate (AKA there are no errors after release).
  *
  * Unreasonable or fragile strategies include:
  * - Set a sticky error flag that the user can query later.
  * - Record all errors since the user last checked for it to query later.
  * - Log errors and try to keep going.
  *
  * These can't work for parts of the library which use other parts which report
  * errors since the intermediate components would need to know the strategy in
  * order to check for errors and avoid doing things that would lead to hard crashes
  * like dereferencing null pointers.
  *
  * ## References on error Handling
  * - http://web.archive.org/web/20160305130913/http://accu.org/index.php/journals/546
  * - http://c2.com/cgi/wiki?AvoidExceptionsWheneverPossible
  */
class ErrorPolicy
{
public:
  /**
   * @brief Report an error returned by a Vulkan API call.
   *
   * The user should be prepared to receive `VK_ERROR_DEVICE_LOST` at any time.
   * Once it has received that error it should throw away all Vulkan object
   * wrappers it holds and repeat Vulkan initialization from the creation of a new
   * logical device.
   *
   * @param[in] apiCalled The name of the Vulkan function which returned the error.
   * @param[in] result The error code returned from Vulkan.
   * @param[in] file The Krust source file in which the call was made.
   * @param[in] line The line in the Krust source file at which the call was made.
   */
  virtual void VulkanError(const char* apiCalled, VkResult result, const char* msg, const char * function, const char * file, unsigned line) = 0;
  /**
   * @brief Report that a vulkan call without a result code output an unexpected
   * value such as a null handle. 
    */
  virtual void VulkanUnexpected(const char* apiCalled, const char* msg, const char * function, const char * file, unsigned line) = 0;
  /**
   * @brief Report the occurrence of an error.
   * @param[in] function The function in which the error was detected.
   */
  virtual void Error(Errors error, const char* msg, const char * function, const char * file, unsigned line)  = 0;
  /**
   * @brief Whether an error remains to be cleared.
   */
  virtual bool ErrorFlagged() const = 0;
};

/**
 * @brief The base of the exception classes thrown by Krust if the user choses
 * to use the error policy ExceptionsErrorPolicy.
 */
class KrustException
{
public:
  //KrustException() {}
  KrustException(const char* msg, const char * function, const char * file, unsigned line) :
    mMsg(msg ? msg : ""),
    mFunction(function ? function : ""),
    mFile(file ? file : ""),
    mLine(line ? line : 0U)
  {}
  /**
   * Dump to log in human-readable form. 
   */
  virtual LogBuilder& Log(LogBuilder& log) const;
  const char* mMsg = "";
  const char * mFunction = "";
  const char * mFile = "";
  unsigned mLine = 0;
};

/**
 * @brief The exception class thrown by Krust in response to incorrect usage or
 * when some Krust-internal error occurs.
 *
 * Only used if the user chooses to use the error policy ExceptionsErrorPolicy.
 * @sa KrustVulkanErrorException for errors reported by Vulkan.
 * @sa KrustVulkanUnexpectedException for unexpected return values like null
 *     pointers from Vulkan calls.
 */
class KrustErrorException : public KrustException
{
public:
  KrustErrorException(Errors error, const char* msg, const char * function, const char * file, unsigned line) :
    KrustException(msg, function, file, line),
    mError(error)
  {}
  /**
   * Dump to log in human-readable form.
   */
  virtual LogBuilder& Log(LogBuilder& log) const;
  Errors mError = Errors::NoError;
};

/**
* @brief The exception class thrown by Krust when Vulkan returns an error code.
*
* Only used if the user choses to use the error policy ExceptionsErrorPolicy.
*/
class KrustVulkanErrorException : public KrustException
{
public:
  KrustVulkanErrorException(const char* apiCalled, VkResult result, const char* msg, const char * function, const char * file, unsigned line) :
    KrustException(msg, function, file, line),
    mApiCalled(apiCalled ? apiCalled : mApiCalled),
    mResult(result)
  {}
  /**
  * Dump to log in human-readable form.
  */
  virtual LogBuilder& Log(LogBuilder& log) const;
  const char* mApiCalled = "";
  VkResult mResult = VK_SUCCESS;
};

/**
* @brief The exception class thrown by Krust when a Vulkan API call returns
* something unexpected such as a null pointer.
*
* Only used if the user choses to use the error policy ExceptionsErrorPolicy.
*/
class KrustVulkanUnexpectedException : public KrustException
{
public:
  KrustVulkanUnexpectedException(const char* apiCalled, const char* msg, const char * function, const char * file, unsigned line) :
    KrustException(msg, function, file, line),
    mApiCalled(apiCalled ? apiCalled : mApiCalled)
  {}
  /**
  * Dump to log in human-readable form.
  */
  virtual LogBuilder& Log(LogBuilder& log) const;
  const char* mApiCalled = "";
};

/**
 * When an error is reported this policy throws a KrustException.
 */
class ExceptionsErrorPolicy : public ErrorPolicy
{
public:
  void VulkanError(const char* apiCalled, VkResult result, const char* msg, const char * function, const char * file, unsigned line) override;
  void VulkanUnexpected(const char* apiCalled, const char* msg, const char * function, const char * file, unsigned line) override;
  void Error(Errors error, const char* msg, const char * function, const char * file, unsigned line) override;
  bool ErrorFlagged() const override {
    // We throw when an error occurs and so are never in a persistent error state:
    return false;
  }
};

///@}

}

#endif // #ifndef KRUST_ERRORS_H_INCLUDED_E26EF
