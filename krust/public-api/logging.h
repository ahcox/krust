#ifndef KRUST_COMMON_PUBLIC_API_LOGGING_H_
#define KRUST_COMMON_PUBLIC_API_LOGGING_H_

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

#if !defined(KRUST_BUILD_CONFIG_DISABLE_LOGGING)
// Internal includes:
#include "krust/public-api/compiler.h"
// External includes:
#include <cstdint>
#include <iostream>
#endif

/**
 * @name LoggingDefines Wrappers for Krust logging functions
 *
 * Helps compile-out logging code when required.
 * These write directly to the global shared log. There may be per subsystem
 * logs that are more appropriate for internal code to use.
 *
 * Usage:
 * <code>
 *   KRUST_LOG_INFO << "Logging the number thirty: " << 30 << endlog;
 * </code>
 * The terminal `endlog` should not be omitted or replaced with a `"\n"`: it is
 * what marks the end of a single entry and causes a LogEntry object to be
 * allocated and pushed into the logging system.
 */
///@{

// Logging macros enabled in release builds by default:

#if !defined(KRUST_BUILD_CONFIG_DISABLE_LOGGING)
#define KRUST_LOG_DEBUG  Krust::LogBuilder(Krust::log, Krust::LogLevel::Debug)
#define KRUST_LOG_INFO  Krust::LogBuilder(Krust::log, Krust::LogLevel::Info)
#define KRUST_LOG_WARN  Krust::LogBuilder(Krust::log, Krust::LogLevel::Warning)
#define KRUST_LOG_ERROR Krust::LogBuilder(Krust::log, Krust::LogLevel::Error)
#else
#define KRUST_LOG_DEBUG Krust::LogBuilderNop(Krust::log, Krust::LogLevel::Debug)
#define KRUST_LOG_INFO  Krust::LogBuilderNop(Krust::log, Krust::LogLevel::Info)
#define KRUST_LOG_WARN  Krust::LogBuilderNop(Krust::log, Krust::LogLevel::Warning)
#define KRUST_LOG_ERROR Krust::LogBuilderNop(Krust::log, Krust::LogLevel::Error)
#endif

// Logging macros enabled only in debug and release-with-debug builds by default:

#if !defined(KRUST_BUILD_CONFIG_DISABLE_LOGGING) && \
    !defined(KRUST_BUILD_CONFIG_DISABLE_DEBUG_LOGGING)
#define KRUST_DEBUG_LOG_DEBUG Krust::LogBuilder(Krust::logDebug, Krust::LogLevel::Debug)
#define KRUST_DEBUG_LOG_INFO  Krust::LogBuilder(Krust::logDebug, Krust::LogLevel::Info)
#define KRUST_DEBUG_LOG_WARN  Krust::LogBuilder(Krust::logDebug, Krust::LogLevel::Warning)
#define KRUST_DEBUG_LOG_ERROR Krust::LogBuilder(Krust::logDebug, Krust::LogLevel::Error)
#else
#define KRUST_DEBUG_LOG_DEBUG Krust::LogBuilderNop(Krust::logDebug, Krust::LogLevel::Debug)
#define KRUST_DEBUG_LOG_INFO  Krust::LogBuilderNop(Krust::logDebug, Krust::LogLevel::Info)
#define KRUST_DEBUG_LOG_WARN  Krust::LogBuilderNop(Krust::logDebug, Krust::LogLevel::Warning)
#define KRUST_DEBUG_LOG_ERROR Krust::LogBuilderNop(Krust::logDebug, Krust::LogLevel::Error)
#endif
///@}

namespace Krust
{

enum class LogLevel : uint8_t
{
  Debug,
  Info,
  Warning,
  Error
};

/** A struct to carry a log entry through the logging system. */
class LogEntry;

/** A helper to allow type safe ostream-style generation of log messages and turning them into full LogEntry instances. */
class LogBuilder;

/** A place to send log entries to. */
// currently typedefed to ostream below:
class LogChannel;

/** A marker for the end of a log message. Flushes a LogBuilder's buffered string eagerly. */
class EndLog;

/**
 * A source for recycled LogEntrys and a sink for populated ones.
 * Still dummied-up using ostreams for now.
 */
class LogChannel
{
public:
  LogChannel(std::ostream &stream) : mStream(stream)
  { }

  operator std::ostream &()
  { return mStream; }

  std::ostream& GetStream()
  { return mStream; }

private:
  std::ostream &mStream;
};

/**
 * Dummied-up to give the same syntax at the call site that there will be when the logging system is implemented.
 * */
class LogBuilder
{
public:
  LogBuilder(LogChannel &channel, const LogLevel = LogLevel::Error) :
    mChannel(channel)
  { }

  operator std::ostream &()
  { return mChannel; }

  LogChannel &Channel()
  { return mChannel; }

  template<typename T>
  LogBuilder &operator<<(const T &t)
  {
    mChannel.GetStream() << t;
    return *this;
  }

  /*template<typename T>
  LogBuilder &operator<<(T t)
  {
    mChannel.GetStream() << t;
    return *this;
  }*/

  template<typename T>
  LogBuilder &operator<<(T *t)
  {
    mChannel.GetStream() << t;
    return *this;
  }

private:
  LogChannel &mChannel;
};

/** When compiling-out logging, this is targeted */
class LogBuilderNop
{
public:
  LogBuilderNop(std::ostream &channel, const LogLevel = LogLevel::Error)
  {
    suppress_unused(&channel);
  }

  template<typename T>
  LogBuilderNop &operator<<(T &t)
  {
    suppress_unused(&t);
    return *this;
  }

  template<typename T>
  LogBuilderNop &operator<<(T t)
  {
    suppress_unused(&t);
    return *this;
  }

  template<typename T>
  LogBuilderNop &operator<<(T *t)
  {
    suppress_unused(&t);
    return *this;
  }

private:
};

/**
 * The bundle of data carrying a log entry through the system from a logging
 * event on an arbitrary thread to being dumped to console, disk, etc, probably
 * on a dedicated thread or as an IO task on an IO thread pool.
 * Just a sketch right now. Not yet used.
 */
class LogEntry
{
  uint64_t mInstant; ///< When the event was logged
  const char *mTag; ///< A label usable for filtering log entries out of a systemwide log. Defaults to "KRUST".
  const char *mSource; ///< Typically the function the log entry was generated from.
  const char *mFile; ///< The source file from which this entry originated.
  uint32_t mSequenceNumber; ///< Monotonic global number got from shared atomic. Wrapping not imporant.
  std::string mMessage; ///< Reusable buffer targeted by LogBuilders
  uint32_t mLine; ///< Where in the code the logging event was raised.
  uint32_t mThreadId; ///< The thread that emitted the entry.
  LogLevel mLevel;
};

/**
 * @name TemporaryLogging Just a stopgap writing to std::clog.
 *
 * This allows the final logging syntax to be used so a bunch of code doesn't
 * need rewriting once the logging is done properly.
 */
///@{

/** std streams can sit lexically where chanels will in the code text.*/
//stypedef std::ostream LogChannel;

/** Global root log channel for all threads, all subsystems. */
extern LogChannel log;      ///< Will be a LogChannel when logging finished.

/**
 * Global root log channel for debug-only messages used by all threads, all
 * subsystems.
 * If debug logs are compiled-out, this can be defined as a NOP channel type,
 * which overrides builder operations to nops, hopefully allowing debug logging
 * to compile right out without resorting to macros. */
extern LogChannel logDebug; ///< Will be a LogChannel when logging finished.

// Don't use these (logging level is a parameter to the logging):
extern LogChannel logErr;
extern LogChannel logWarn;
extern LogChannel logInfo;

extern const char *const endl;
extern const char *const endlog;

///@}

/**
 * Exercise some of the code above, instantiating templates.
 */
inline void test_caller()
{
  // Write to the global error log:
  LogBuilder(log) << "A test log line" << endlog;
  LogBuilder lb(log);
  lb << "hi";
  lb << 99;
  lb << "Hi there " << 99 << "this is a " << 99.9 << "test" << char(1) << endlog;
  LogBuilder(log) << "A test log line" << 99 << "this is a " << 99.9 << "test" << char(1) << endlog;
  Krust::LogBuilder(log) << 99;
  Krust::LogBuilder(logDebug) << "A test log line" << 99 << "this is a " << 99.9 << "test" << char(1) << endlog;

  // Macro versions:
  KRUST_LOG_ERROR << "Hi there Krust logging system. I think you are gr" << 8 << endlog;
  KRUST_DEBUG_LOG_ERROR << "Hi there Krust debug logging system. I think you are s" << 0 << 0 << 0 << 0 <<
  " awesome!" << endlog;

  // Nop versions:
  LogBuilderNop(log) << "hi" << endlog;
  LogBuilderNop(log) << "hi" << 99 << "there" << 99.9 << "you" << 1 << &lb << endlog;
}


} /* namespace Krust */

#endif /* KRUST_COMMON_PUBLIC_API_LOGGING_H_ */
