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

#include "krust-errors.h"
#include "krust/public-api/logging.h"
#include "krust/public-api/vulkan-utils.h"

namespace Krust
{

  const char * ErrorToString(const Errors & error)
  {
    const char * str = "<<UNKNOWN>>";
    switch (error)
    {
    case Errors::NoError:
      str = "NoError";
      break;
    case Errors::IllegalArgument:
      str = "IllegalArgument";
      break;
    case Errors::IllegalState:
      str = "IllegalState";
      break;
    case Errors::MaxError:
      // Default to UNKNOWN
      break;
    }
    return str;
  }

  LogBuilder& KrustException::Log(LogBuilder& logBuilder) const
  {
    logBuilder << "[msg = " << mMsg << "] [function = " << mFunction << "] [file = " << mFile << "] [line = " << mLine << "]";
    return logBuilder;
  }

  LogBuilder& KrustErrorException::Log(LogBuilder& logBuilder) const
  {
    logBuilder << "KrustErrorException: [error = " << mError << "] ";
    KrustException::Log(logBuilder);
    return logBuilder;
  }

  LogBuilder& KrustVulkanErrorException::Log(LogBuilder& logBuilder) const
  {
    logBuilder << "KrustVulkanErrorException: [called = " << mApiCalled << "] [result = " << ResultToString(mResult) << "] ";
    KrustException::Log(logBuilder);
    return logBuilder;
  }

  LogBuilder& KrustVulkanUnexpectedException::Log(LogBuilder& logBuilder) const
  {
    logBuilder << "KrustVulkanUnexpectedException: [called = " << mApiCalled << "] ";
    KrustException::Log(logBuilder);
    return logBuilder;
  }

  void ExceptionsErrorPolicy::VulkanError(const char * apiCalled, VkResult result, const char *  msg, const char * function, const char * file, unsigned line)
  {
    KRUST_LOG_ERROR << "Vulkan error (" << ResultToString(result) << ") (msg: \"" << (msg ? msg : "") << "\") reported by " << apiCalled << " in function " << function << " at: " << file << ":" << line << endlog;
    throw KrustVulkanErrorException(apiCalled, result, msg, function, file, line);
  }

  void ExceptionsErrorPolicy::VulkanUnexpected(const char* apiCalled, const char* msg, const char * function, const char * file, unsigned line) {
    KRUST_LOG_ERROR << "Vulkan error (msg: \"" << (msg ? msg : "") << "\") reported by " << apiCalled << " in function " << function << " at: " << file << ":" << line << endlog;
    throw KrustVulkanUnexpectedException(apiCalled, msg, function, file, line);
  }

  void ExceptionsErrorPolicy::Error(Errors error, const char * msg, const char * function, const char * file, unsigned line)
  {
    KRUST_LOG_ERROR << error << " in " << function << " (msg: \"" << (msg ? msg : "") << "\") at: " << file << ":" << line << endlog;
    throw KrustErrorException(error, msg, function, file, line);
  }

} // namespace Krust
