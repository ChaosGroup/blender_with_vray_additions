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

#include "BKE_global.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define MY_COLOR_RED      "\033[0;31m"
#define MY_COLOR_GREEN    "\033[0;32m"
#define MY_COLOR_YELLOW   "\033[0;33m"
#define MY_COLOR_BLUE     "\033[0;34m"
#define MY_COLOR_CYAN     "\033[1;34m"
#define MY_COLOR_MAGENTA  "\033[0;35m"
#define MY_COLOR_DEFAULT  "\033[0m"

/// ESC sequence start marker.
static const char ESC_START = '\033';

/// ESC sequence end marker.
static const char ESC_END = 'm';

// Max message buffer size
const int COLOR_BUF_LEN = 2048;

// Max esc sequene length
const int MAX_ESC_LEN = 64;

/// The ID of the main thread - used to distinguish in log.
static const std::thread::id mainThreadID = std::this_thread::get_id();

static const char *months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

#ifdef _WIN32
static CONSOLE_SCREEN_BUFFER_INFO CSBI;
static int BACKGROUND_INFO;

struct win_to_ansi_struct {
	const char *color;
	const int colorLen;
	int color_id;
};

static struct win_to_ansi_struct AnsiToWinColor[] = {
	{MY_COLOR_RED, strlen(MY_COLOR_RED), FOREGROUND_RED},
	{MY_COLOR_GREEN, strlen(MY_COLOR_GREEN), FOREGROUND_GREEN},
	{MY_COLOR_YELLOW, strlen(MY_COLOR_YELLOW), FOREGROUND_RED | FOREGROUND_GREEN},
	{MY_COLOR_BLUE, strlen(MY_COLOR_BLUE), FOREGROUND_GREEN | FOREGROUND_BLUE},
	{MY_COLOR_CYAN, strlen(MY_COLOR_CYAN), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY},
	{MY_COLOR_MAGENTA, strlen(MY_COLOR_MAGENTA), FOREGROUND_RED | FOREGROUND_BLUE},
	{MY_COLOR_DEFAULT, strlen(MY_COLOR_DEFAULT), FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED},
};

static void win_cprint(HANDLE mConsoleHandle, const char *buf, const char *escSeq = NULL)
{
	if (!escSeq)
		return;

	// Using WriteFile here for correct redirection to file
	DWORD bytesWritten;
	WriteFile(mConsoleHandle, buf, DWORD(strlen(buf)), &bytesWritten, NULL);

	for (size_t i = 0; i < ArraySize(AnsiToWinColor); ++i) {
		const win_to_ansi_struct &p = AnsiToWinColor[i];

		if (strncmp(p.color, escSeq, p.colorLen) != 0)
			continue;
		if (strncmp(p.color, MY_COLOR_DEFAULT, p.colorLen) == 0)
			SetConsoleTextAttribute(mConsoleHandle, CSBI.wAttributes);
		else
			SetConsoleTextAttribute(mConsoleHandle, BACKGROUND_INFO | p.color_id);
		break;
	}
}
#endif

#ifdef _WIN32
static void cprint_stripped(const char *buf, HANDLE mConsoleHandle)
#else
static void cprint_stripped(const char *buf)
#endif
{
	if (!buf || buf[0] == '\0')
		return;

	int bufPos = 0;
	int copyBufPos = 0;

	// Flag showing that bufPos is inside
	// escape sequence
	bool inEsc = false;

	char *copyBuf = new char[strlen(buf) + 1];

	while (buf[bufPos] != '\0') {
		if (buf[bufPos] == ESC_START)
			inEsc = true;
		else if (inEsc && buf[bufPos] == ESC_END)
			inEsc = false;
		else if (!inEsc)
			copyBuf[copyBufPos++] = buf[bufPos];
		bufPos++;
	}
	copyBuf[copyBufPos] = '\0';

#ifdef _WIN32
	win_cprint(mConsoleHandle, copyBuf);
#else
	printf("%s", copyBuf);
#endif

	FreePtrArr(copyBuf);
}

static void cprint(bool useColor, const char *buf)
{
	if (!buf || buf[0] == '\0')
		return;

#ifndef _WIN32
	// UNIX natively supports ANSI escape colors.
	// isatty(1) == 1 means we are printing to the terminal,
	// otherwise we are printing to the file and want
	// colorizing to be off.
	//
	if(useColor && isatty(1))
		printf("%s", buf);
	else
		cprint_stripped(buf);
	return;
#else
	HANDLE mConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

	GetConsoleScreenBufferInfo(mConsoleHandle, &CSBI);
	BACKGROUND_INFO = CSBI.wAttributes & 0xF0;

	if (mConsoleHandle == INVALID_HANDLE_VALUE) {
		printf("cprint: Invalid handle!\n");
		return;
	}

	if (!useColor) {
		cprint_stripped(buf, mConsoleHandle);
		return;
	}

	char *copyBuf = new char[strlen(buf) + 1];
	char escSeq[MAX_ESC_LEN];

	int bufPos = 0;
	int copyBufPos = 0;
	int escPos = 0;

	bool inEsc = false;

	while (buf[bufPos] != '\0') {
		if (buf[bufPos] == ESC_START)
			inEsc = true;
		if (!inEsc)
			copyBuf[copyBufPos++] = buf[bufPos];
		else {
			if (buf[bufPos] == ESC_END) {
				escSeq[escPos++] = ESC_END;
				escSeq[escPos++] = '\0';

				copyBuf[copyBufPos] = '\0';
				copyBufPos = 0;

				win_cprint(mConsoleHandle, copyBuf, escSeq);

				inEsc = false;
				escPos = 0;
				bufPos++;
				continue;
			}
			else {
				escSeq[escPos++] = buf[bufPos];
				if (escPos == MAX_ESC_LEN) {
					printf("cprint: Incorrect ESC length!\n");
					break;
				}
			}
		}

		bufPos++;
	}
	copyBuf[copyBufPos] = '\0';

	// Print if we have any data left
	if (copyBufPos)
		win_cprint(mConsoleHandle, copyBuf);

	FreePtrArr(copyBuf);

	// Reset color back to default
	SetConsoleTextAttribute(mConsoleHandle, CSBI.wAttributes);
#endif // _WIN32
}

static void cprintf(bool useColor, const char *format, ...)
{
	if (!format || format[0] == '\0')
		return;

	va_list args;
	char buf[COLOR_BUF_LEN];
	char *buffer = buf;

	va_start(args, format);
	// first get the required number of chars to avoid buffer overflows
#ifdef _WIN32
	int len = _vscprintf(format, args);
#else
	int len = vsnprintf(NULL, 0, format, args);
#endif
	va_end(args);

	++len;

	if (len > COLOR_BUF_LEN)
		buffer = new char[len];

	va_start(args, format);

	vsnprintf(buffer, len, format, args);
	cprint(useColor, buffer);

	va_end(args);

	if (buffer != buf)
		delete [] buffer;
}

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
		case LogLevel::info: return MY_COLOR_DEFAULT;
		case LogLevel::progress: return MY_COLOR_GREEN;
		case LogLevel::warning: return MY_COLOR_YELLOW;
		case LogLevel::error: return MY_COLOR_RED;
		case LogLevel::debug: return MY_COLOR_CYAN;
		default: return MY_COLOR_DEFAULT;
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
	, isRunning(false)
{}

Logger::~Logger()
{
	VFB_Assert(!isRunning && "Logger must be stopped before destroying it");
}

void Logger::setLogLevel(LogLevel value)
{
	logLevel = value;
}

void Logger::printMessage(const VfhLogMessage &msg) const
{
	char strTime[100];
	char strDate[100];
	char buf[2249];

	timeToStr(strTime, ArraySize(strTime), msg.time);
	dateToStr(strDate, ArraySize(strDate), msg.time);

	snprintf(buf, ArraySize(buf),
		MY_COLOR_BLUE "[%s|%s]" MY_COLOR_DEFAULT " VFB |%s%8s" MY_COLOR_DEFAULT "| %s %s%s%s" MY_COLOR_DEFAULT,
		strDate,
		strTime,
		logLevelAsColor(msg.level),
		logLevelAsString(msg.level),
		msg.fromMain ? "*" : " ",
		logLevelAsColor(msg.level),
		msg.message.c_str(),
		msg.level == LogLevel::progress ? "\r" : "\n"
	);

	cprintf(true, "%s", buf);

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

void Logger::stopLogging(bool flush)
{
	if (!isRunning) {
		return;
	}
	isRunning = false;

	{
		std::lock_guard<std::mutex> lock(mutex);
		if (flush) {
			for (const VfhLogMessage &msg : queue) {
				printMessage(msg);
			}
		}
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
	if (logLevel == LogLevel::none)
		return;

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
	if (logLevel == LogLevel::none)
		return;

	va_list args;
	va_start(args, format);

	add(level, format, args);

	va_end(args);
}

void Logger::info(const char *format, ...) const
{
	if (logLevel == LogLevel::none)
		return;

	va_list args;
	va_start(args, format);

	add(LogLevel::info, format, args);

	va_end(args);
}

void Logger::warning(const char *format, ...) const
{
	if (logLevel == LogLevel::none)
		return;

	va_list args;
	va_start(args, format);

	add(LogLevel::warning, format, args);

	va_end(args);
}

void Logger::error(const char *format, ...) const
{
	if (logLevel == LogLevel::none)
		return;

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
	if (logLevel == LogLevel::none)
		return;

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
