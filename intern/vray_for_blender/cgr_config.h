/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * * ***** END GPL LICENSE BLOCK *****
 */

#ifndef CGR_CONFIG
#define CGR_CONFIG

#define BOOST_NO_CXX11_ALLOCATOR

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#  define VC_EXTRALEAN
#endif

// For int types on POSIX systems
#include <stdlib.h>
#include <stdio.h>

// For M_PI
#ifdef WIN32
#  define _USE_MATH_DEFINES
#endif
#include <math.h>

#define CGR_PLUGIN_NAME V-Ray For Blender

#ifdef DEBUG
#  define CGR_USE_DEBUG      1
#else
#  define CGR_USE_DEBUG      1
#endif

#define CGR_USE_TRACE_DEBUG 0

#define CGR_USE_CALL_DEBUG  (1 && CGR_USE_DEBUG)
#define CGR_USE_TIME_DEBUG  (1 && CGR_USE_DEBUG)
#define CGR_USE_DRAW_DEBUG  (0 && CGR_USE_DEBUG)
#define CGR_USE_DESTR_DEBUG (1 && CGR_USE_DEBUG)

#define CGR_USE_RNA_API          0
#define CGR_NTREE_DRIVER         0
#define CGR_USE_DUPLI_INSTANCER  0
#define CGR_USE_MURMUR_HASH      0

#define CGR_EXPORT_LIGHTS_CPP    0

#define CGR_MAX_PLUGIN_NAME  1024
#define CGR_DEFAULT_MATERIAL "MANOMATERIALISSET"
#define CGR_DEFAULT_BRDF     "BRDFNOBRDFISSET"

#define CGR_MAX_LAYERED_TEXTURES  64
#define CGR_MAX_LAYERED_BRDFS     64

#ifndef WIN32
#  define COLOR_RED      "\033[0;31m"
#  define COLOR_GREEN    "\033[0;32m"
#  define COLOR_YELLOW   "\033[0;33m"
#  define COLOR_BLUE     "\033[0;34m"
#  define COLOR_MAGENTA  "\033[0;35m"
#  define COLOR_DEFAULT  "\033[0m"
#else
#  define COLOR_RED      ""
#  define COLOR_GREEN    ""
#  define COLOR_YELLOW   ""
#  define COLOR_BLUE     ""
#  define COLOR_MAGENTA  ""
#  define COLOR_DEFAULT  ""
#endif

#define NOT(x) !(x)

#define STRINGIZE_NX(A) #A
#define STRINGIZE(A) STRINGIZE_NX(A)

#ifdef CGR_PLUGIN_NAME
#  define _OUTPUT_PROMPT(P)         COLOR_MAGENTA STRINGIZE(P)          COLOR_DEFAULT ": "
#  define _OUTPUT_ERROR_PROMPT(P)   COLOR_RED     STRINGIZE(P) " Error" COLOR_DEFAULT ": "
#  define _OUTPUT_WARNING_PROMPT(P) COLOR_YELLOW  STRINGIZE(P) " Warning" COLOR_DEFAULT ": "
#  define OUTPUT_PROMPT         _OUTPUT_PROMPT(CGR_PLUGIN_NAME)
#  define OUTPUT_ERROR_PROMPT   _OUTPUT_ERROR_PROMPT(CGR_PLUGIN_NAME)
#  define OUTPUT_WARNING_PROMPT _OUTPUT_WARNING_PROMPT(CGR_PLUGIN_NAME)
#else
#  define OUTPUT_PROMPT         COLOR_MAGENTA "Info"    COLOR_DEFAULT ": "
#  define OUTPUT_ERROR_PROMPT   COLOR_RED     "Error"   COLOR_DEFAULT ": "
#  define OUTPUT_WARNING_PROMPT COLOR_YELLOW  "Warning" COLOR_DEFAULT ": "
#endif

#if CGR_USE_DEBUG == 0
#  define DEBUG_PRINT(use_debug, ...)
#else
#  define DEBUG_PRINT(use_debug, ...) \
	if(use_debug && G.debug) { \
        fprintf(stdout, OUTPUT_PROMPT); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    }
#endif

#define PRINT_ERROR(...) {\
    fprintf(stdout, OUTPUT_ERROR_PROMPT); \
    fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "\n"); \
	fflush(stdout); }

#define PRINT_INFO_EX(...) {\
	fprintf(stdout, OUTPUT_PROMPT); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	fflush(stdout); }

#define PRINT_INFO(...) \
	if(G.debug) {\
	fprintf(stdout, OUTPUT_PROMPT); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	fflush(stdout);\
	}


#if CGR_USE_TRACE_DEBUG == 1
#include <thread>
#include <chrono>
#include <cstdarg>
typedef std::chrono::milliseconds::rep vfb_ms;
#define VFB_MS_FORMAT "%llu"
static const auto appStart = std::chrono::high_resolution_clock::now();

inline vfb_ms getTickCount() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - appStart).count();
}


#define TRACE_PROMPT COLOR_RED "VFB TRACE "
#define TID_FORMAT "(%5u)"
inline unsigned thread_id_short() {
	return std::hash<std::thread::id>()(std::this_thread::get_id()) % 100000;
}

#define PRINT_TRACE(...)                                                 \
	{                                                                    \
		fprintf(stdout, TRACE_PROMPT TID_FORMAT " [" VFB_MS_FORMAT "] ", \
			thread_id_short(), getTickCount());                          \
		fprintf(stdout, __VA_ARGS__);                                    \
		fprintf(stdout, "\n");                                           \
		fflush(stdout);                                                  \
	}

struct ScopedTrace {
	explicit ScopedTrace(const char * name)
		: name(name)
		, startTime(getTickCount())
	{}

	void dump() {
		const auto elapsed = getTickCount() - startTime;
		if (name && elapsed >= 10) {
			fprintf(stdout, TRACE_PROMPT TID_FORMAT " {" VFB_MS_FORMAT "} %s\n", thread_id_short(), elapsed, name);
			name = nullptr;
		}
	}

	~ScopedTrace() {
		dump();
	}

	ScopedTrace(const ScopedTrace &) = delete;
	ScopedTrace & operator=(const ScopedTrace &) = delete;

	const char * name;
	vfb_ms startTime;
};

struct ScopedTraceFormat: public ScopedTrace {
	ScopedTraceFormat(const char * format, ...)
		: ScopedTrace(nullptr)
	{
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, 1024, format, args);
		va_end(args);
	}

	void dump() {
		const auto elapsed = getTickCount() - startTime;
		if (buffer[0] && elapsed >= 10) {
			fprintf(stdout, TRACE_PROMPT TID_FORMAT " {" VFB_MS_FORMAT "} %s\n", thread_id_short(), elapsed, buffer);
			buffer[0] = 0;
		}
	}

	~ScopedTraceFormat() {
		dump();
	}

	char buffer[1024];
};

#define SCOPED_TRACE(name) ScopedTrace _scopedTrace ## __COUNTER__ (name);
#define SCOPED_TRACE_EX(...) ScopedTraceFormat _scopedTraceFormat ## __COUNTER__ (__VA_ARGS__);
#else
struct ScopedTrace {
	void dump() {};
};
struct ScopedTraceFormat: public ScopedTrace {
	ScopedTraceFormat(...) {}
};
#define PRINT_TRACE(...)
#define SCOPED_TRACE(_)
#define SCOPED_TRACE_EX(...)
#endif

#define PRINT_INFO_LB(...) {\
	fprintf(stdout, OUTPUT_PROMPT); \
	fprintf(stdout, __VA_ARGS__); \
	fflush(stdout); }

#define PRINT_WARN(...) { \
	fprintf(stdout, OUTPUT_WARNING_PROMPT); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	fflush(stdout); }

#if CGR_USE_DEBUG == 0
#  define PRINT_TM4(label, tm) ()
#else
#  define PRINT_TM4(label, tm) \
	PRINT_INFO("%s:", label); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[0][0], tm[0][1], tm[0][2], tm[0][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[1][0], tm[1][1], tm[1][2], tm[1][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[2][0], tm[2][1], tm[2][2], tm[2][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[3][0], tm[3][1], tm[3][2], tm[3][3]);
#endif

#define COPY_VECTOR(r, index, a) \
    r[index+0] = a[0]; \
    r[index+1] = a[1]; \
    r[index+2] = a[2]; \
    index += 3;

#define GEOM_TYPE(ob) ob->type == OB_MESH || ob->type == OB_CURVE || ob->type == OB_SURF  || ob->type == OB_FONT  || ob->type == OB_MBALL
#define EMPTY_TYPE(ob) ob->type == OB_EMPTY
#define LIGHT_TYPE(ob) ob->type == OB_LAMP

#ifdef _WIN32
typedef unsigned __int8  u_int8_t;
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef unsigned __int64 u_int64_t;
#endif

#define _C(x) (char*)x

#define CGR_TRANSFORM_HEX_SIZE  129

#define USE_MT_EXPORTER 0

#endif // CGR_CONFIG
