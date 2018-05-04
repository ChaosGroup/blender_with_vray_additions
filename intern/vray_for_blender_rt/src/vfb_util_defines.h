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

#include "BLI_assert.h"
#include <cassert>

#ifndef CGR_UTIL_DEFINES_H
#define CGR_UTIL_DEFINES_H

template<typename T>
inline void FreePtr(T *&p) {
	delete p;
	p = nullptr;
}

template<typename T>
inline void FreePtrArr(T *&p) {
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
	/// Assert will **ALWAYS** test its condition but will only call assert in debug mode
	#define VFB_Assert(test) (void)((!!(test)) ? (BLI_system_backtrace(stderr), _VFB_Assert_PRINT_POS(test), 0) : 0);
#endif

#endif // CGR_UTIL_DEFINES_H
