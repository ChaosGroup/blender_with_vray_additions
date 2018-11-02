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

#include "vfb_log.h"
#include "vfb_util_defines.h"

#include "BKE_global.h"

#ifdef _WIN32
#include <windows.h>
#endif

/// The ID of the main thread - used to distinguish in log.
static const std::thread::id mainThreadID = std::this_thread::get_id();

static const char *months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *logLevelAsString(LogLevel level)
{
	switch (level) {
		case LogLevel::info: return "Info";
		case LogLevel::progress: return "Progress";
		case LogLevel::warning: return "Warning";
		case LogLevel::error: return "Error";
		case LogLevel::debug: return "Debug";
		default: return "Unknown";
	}
}

static const char *logLevelAsColor(LogLevel level)
{
	switch (level) {
		case LogLevel::info: return COLOR_DEFAULT;
		case LogLevel::progress: return COLOR_GREEN;
		case LogLevel::warning: return COLOR_YELLOW;
		case LogLevel::error: return COLOR_RED;
		case LogLevel::debug: return COLOR_CYAN;
		default: return COLOR_DEFAULT;
	}
}

static int timeToStr(char *str, int strLen, time_t time)
{
	tm *t = localtime(&time);
	if (t) {
		return snprintf(str, strLen, "%02i:%02i:%02i", t->tm_hour, t->tm_min, t->tm_sec);
	}
	str[0] = '\0';
	return 0;
}

int dateToStr(char *str, int strLen, time_t time)
{
	tm *t = localtime(&time);
	if (t) {
		int month = t->tm_mon;
		if (month < 0)
			month = 0;
		if (month >= 11)
			month = 11;
		const int year = t->tm_year + 1900;
		return snprintf(str, strLen, "%i/%s/%i", year, months[month], t->tm_mday);
	}
	str[0] = '\0';
	return 0;
}

Logger::Logger()
	: logLevel(LogLevel::error)
	, re(PointerRNA_NULL)
{}

void Logger::printMessage(const VfhLogMessage &msg)
{
	char strTime[100];
	char strDate[100];
	char buf[2249];

	timeToStr(strTime, ArraySize(strTime), msg.time);
	dateToStr(strDate, ArraySize(strDate), msg.time);

	snprintf(buf, ArraySize(buf),
		COLOR_BLUE "[%s|%s]" COLOR_DEFAULT " VFB |%s%8s" COLOR_DEFAULT "| %s %s%s%s" COLOR_DEFAULT,
		strDate,
		strTime,
		logLevelAsColor(msg.level),
		logLevelAsString(msg.level),
		msg.fromMain ? "*" : " ",
		logLevelAsColor(msg.level),
		msg.message.c_str(),
		msg.level == LogLevel::progress ? "\r" : "\n"
	);

	FILE *output = msg.level == LogLevel::error ? stderr : stdout;
	fprintf(output, "%s", buf);
	fflush(output);

#ifdef _WIN32
	OutputDebugStringA(buf);
#endif
}

void Logger::run() const
{
	while (isRunning) {
		if (queue.empty()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		else {
			VfhLogQueue logBatch; {
				std::lock_guard<std::mutex> lock(mutex);
				logBatch = std::move(queue);
			}
			for (const VfhLogMessage &msg : logBatch) {
				printMessage(msg);
			}
		}
	}
}

void Logger::startLogging()
{
	isRunning = true;
	logThread = std::thread(&Logger::run, this);
}

void Logger::stopLogging()
{
	isRunning = false;

	{
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
	}

	if (logThread.joinable()) {
		logThread.join();
		logThread = std::thread();
	}
}

void Logger::add(const VfhLogMessage &msg) const
{
	std::lock_guard<std::mutex> lock(mutex);
	queue.emplace_back(msg);
}

void Logger::add(LogLevel level, const char *format, va_list args) const
{
	// Show all messages in debug.
	if (!G.debug) {
		const bool showMessage = level <= logLevel;
		if (!showMessage) {
			return;
		}
	}

	char msgBuf[2048];
	vsnprintf(msgBuf, ArraySize(msgBuf), format, args);

	VfhLogMessage msg;
	time(&msg.time);
	msg.level = level;
	msg.message = msgBuf;
	msg.fromMain = std::this_thread::get_id() == mainThreadID;

	if (isRunning) {
		add(msg);
	}
	else {
		printMessage(msg);
	}
}

void Logger::log(LogLevel level, const char *format, ...) const
{
	va_list args;
	va_start(args, format);

	add(level, format, args);

	va_end(args);
}

void Logger::setLogLevel(LogLevel value)
{
	logLevel = value;
}

void Logger::info(const char *format, ...) const
{
	va_list args;
	va_start(args, format);

	add(LogLevel::info, format, args);

	va_end(args);
}

void Logger::warning(const char *format, ...) const
{
	va_list args;
	va_start(args, format);

	add(LogLevel::warning, format, args);

	va_end(args);
}

void Logger::error(const char *format, ...) const
{
	va_list args;
	va_start(args, format);

	add(LogLevel::error, format, args);

	va_end(args);
}

void Logger::debug(const char *format, ...) const
{
	if (!G.debug)
		return;

	va_list args;
	va_start(args, format);

	add(LogLevel::debug, format, args);

	va_end(args);
}

void Logger::progress(const char *format, ...) const
{
	va_list args;
	va_start(args, format);

	add(LogLevel::progress, format, args);

	va_end(args);
}

static Logger logger;

Logger &getLog()
{
	return logger;
}
