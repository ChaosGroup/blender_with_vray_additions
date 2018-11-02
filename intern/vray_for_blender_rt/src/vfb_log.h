/*
 * Copyright (c) 2018, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VRAY_FOR_BLENDER_LOG_H
#define VRAY_FOR_BLENDER_LOG_H

#include "vfb_rna.h"

#include <cstdarg>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>

#if defined(__RESHARPER__)
#define PRINTF_ATTR(StringIndex, FirstToCheck) [[rscpp::format(printf, StringIndex, FirstToCheck)]]
#else
#define PRINTF_ATTR(StringIndex, FirstToCheck)
#endif

enum class LogLevel {
	info,
	progress,
	warning,
	error,
	debug,
};

/// Simple logger class wrapping over printf.
struct Logger {
	Logger();
	~Logger();

	/// Log string with info level.
	PRINTF_ATTR(2, 3) void info(const char *format, ...) const;

	/// Log string with progress level.
	PRINTF_ATTR(2, 3) void progress(const char *format, ...) const;

	/// Log string with warning level.
	PRINTF_ATTR(2, 3) void warning(const char *format, ...) const;

	/// Log string with error level
	PRINTF_ATTR(2, 3) void error(const char *format, ...) const;

	/// Log string with debug level.
	PRINTF_ATTR(2, 3) void debug(const char *format, ...) const;

	/// Log string with custom level.
	PRINTF_ATTR(3, 4) void log(LogLevel level, const char *format, ...) const;

	/// Set max log level to be printed, unless Logger::msg is used where current filter is ignored.
	void setLogLevel(LogLevel value);

	/// Set render engine.
	void setRenderEngine(BL::RenderEngine value);

	/// Initialize the logger, needs to be called only once.
	/// Don't call this from static variable's ctor, which is inside a module (causes deadlock).
	void startLogging();

	/// Call this before exiting the application, and not from static variable's dtor
	/// It will stop and join the logger thread.
	void stopLogging();

private:
	struct VfhLogMessage {
		/// The message's log level.
		LogLevel level = LogLevel::debug;

		/// The message.
		std::string message;

		/// The time the log was made.
		time_t time = 0;

		/// The thread ID of the caller thread.
		int fromMain = -1;
	};

	typedef std::vector<VfhLogMessage> VfhLogQueue;

	/// Printing
	/// @param msg Log message.
	void printMessage(const VfhLogMessage &msg) const;

	/// Add message to the queue.
	/// @param level Message level.
	/// @param format Format string.
	/// @param args Format arguments.
	void add(LogLevel level, const char *format, va_list args) const;

	/// Add message to the queue.
	void add(const VfhLogMessage &msg) const;

	/// Process log queue.
	void run() const;

	/// Current max log level to be shown.
	LogLevel logLevel;

	/// Log message queue.
	mutable VfhLogQueue queue;

	/// Queue lock.
	mutable std::mutex mutex;

	/// Run flag.
	mutable std::atomic<int> isRunning = false;

	/// Logger thread.
	std::thread logThread;

	/// Render engine for GUI messages.
	mutable BL::RenderEngine re;
};

/// Get singleton instance of Logger.
Logger &getLog();

#endif // VRAY_FOR_BLENDER_LOG_H
