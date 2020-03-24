/*
 * Copyright (c) 2015, Chaos Software Ltd
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

#include "BLI_system.h"
#include <cassert>

#ifndef CGR_UTIL_DEFINES_H
#define CGR_UTIL_DEFINES_H

#ifndef WIN32
#define COLOR_RED      "\033[0;31m"
#define COLOR_GREEN    "\033[0;32m"
#define COLOR_YELLOW   "\033[0;33m"
#define COLOR_BLUE     "\033[0;34m"
#define COLOR_CYAN     "\033[1;34m"
#define COLOR_MAGENTA  "\033[0;35m"
#define COLOR_DEFAULT  "\033[0m"
#else
#define COLOR_RED ""
#define COLOR_GREEN ""
#define COLOR_YELLOW ""
#define COLOR_BLUE ""
#define COLOR_CYAN ""
#define COLOR_MAGENTA ""
#define COLOR_DEFAULT ""
#endif

template <typename T>
inline void FreePtr(T *&p)
{
	delete p;
	p = nullptr;
}

template <typename T>
inline void FreePtrArr(T *&p)
{
	delete [] p;
	p = nullptr;
}

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define ArraySize(array) (sizeof(ArraySizeHelper(array)))

#define MemberEq(member) (member == other.member)
#define MemberNotEq(member) (!(member == other.member))

#ifdef DEBUG
/// Assert will **ALWAYS** test its condition but will only call assert in debug mode
#define VFB_Assert(test) assert(test);
#else
#if defined(__GNUC__)
		#define _VFB_ASSERT_PRINT_POS(a) fprintf(stderr, "BLI_assert failed: %s:%d, %s(), at \'%s\'\n", __FILE__, __LINE__, __func__, #a)
#elif defined(_MSC_VER)
		#define _VFB_ASSERT_PRINT_POS(a) fprintf(stderr, "BLI_assert failed: %s:%d, %s(), at \'%s\'\n", __FILE__, __LINE__, __FUNCTION__, #a)
#else
		#define _VFB_ASSERT_PRINT_POS(a) fprintf(stderr, "BLI_assert failed: %s:%d, at \'%s\'\n", __FILE__, __LINE__, #a)
#endif

	/// Assert will **ALWAYS** test its condition but will only call assert in debug mode
	#define VFB_Assert(test) (void)((!(test)) ? (_VFB_ASSERT_PRINT_POS(test), 0) : 0);
#endif

/// Disables copy-constructor and assignment operator
#define VFB_DISABLE_COPY(cls) \
private: \
	cls(const cls&) = delete; \
	cls& operator=(const cls&) = delete;

#endif // CGR_UTIL_DEFINES_H
