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

#ifndef VRAY_FOR_BLENDER_UTILS_BLENDER_H
#define VRAY_FOR_BLENDER_UTILS_BLENDER_H

#include "vfb_rna.h"

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

#include <Python.h>

#include <memory>
#include <functional>
#include <type_traits>

#ifdef WITH_OSL
#include <OSL/oslquery.h>
#endif

// This is global because multiple mt exporters could run at the same time
static boost::shared_mutex vfbExporterBlenderLock;
#define WRITE_LOCK_BLENDER_RAII boost::unique_lock<boost::shared_mutex> _raiiWriteLock(vfbExporterBlenderLock);
#define READ_LOCK_BLENDER_RAII boost::shared_lock<boost::shared_mutex> _raiiReadLock(vfbExporterBlenderLock);

namespace VRayForBlender {
namespace Blender {
#ifdef WITH_OSL
struct OSLManager {
	std::string stdOSLPath;

	bool compile(const std::string & inputFile, const std::string & outputFile);
	std::string compileToBuffer(const std::string & code);

	// Query compiled .oso file for parameters
	bool queryFromFile(const std::string & file, OSL::OSLQuery & query);
	bool queryFromBytecode(const std::string & code, OSL::OSLQuery & query);

	bool queryFromNode(BL::Node node, OSL::OSLQuery & query, const std::string & basepath, bool writeToFile = false, std::string * output = nullptr);

	static OSLManager & getInstance() {
		static OSLManager mgr;
		return mgr;
	}
};
#endif


inline void freePyObject(PyObject * ob) {
	if (ob) {
		Py_DECREF(ob);
	}
}

typedef std::unique_ptr<PyObject, void (*)(PyObject *)> PyObjectRAII;

inline PyObjectRAII toPyPTR(PyObject * ob) {
	return PyObjectRAII(ob, freePyObject);
}

std::string   GetFilepath(const std::string &filepath, ID *holder=nullptr);

BL::Object    GetObjectByName(BL::BlendData data, const std::string &name);
BL::Material  GetMaterialByName(BL::BlendData data, const std::string &name);

int           GetMaterialCount(BL::Object ob);

template <typename T>
T GetDataFromProperty(PointerRNA *ptr, const std::string &attr) {
	T val(PointerRNA_NULL);
	PropertyRNA *ntreeProp = RNA_struct_find_property(ptr, attr.c_str());
	if (ntreeProp) {
		if  (RNA_property_type(ntreeProp) == PROP_POINTER) {
			val = T(RNA_pointer_get(ptr, attr.c_str()));
		}
	}
	return val;
}

float GetDistanceObOb(BL::Object a, BL::Object b);

float GetCameraDofDistance(BL::Object camera);

int IsHairEmitter(BL::Object ob);
int IsEmitterRenderable(BL::Object ob);
int IsDuplicatorRenderable(BL::Object ob);
int IsGeometry(BL::Object ob);
int IsLight(BL::Object ob);

/// Get the number of subframes selected for the object
inline int getObjectSubframes(BL::Object ob) {
	auto vrayOb = RNA_pointer_get(&ob.ptr, "vray");
	return vrayOb.id.data ? RNA_int_get(&vrayOb, "subframes") : 0;
}

/// Get the number of keyframes that must be exported for an object (2 + number of subframes)
inline int getObjectKeyframes(BL::Object ob) {
	return getObjectSubframes(ob) + 2;
}

/// Wrapper class over BL::BlendData collections that has begin() method which returns the iterator instead
/// of taking it as an argument, this is when using rage based for loop
/// NOTE: *never* use .length() and operator[](int) on blender collections as this is quadratic since all collections are linked lists
template <typename T, typename iterator>
class BLCollection {
	T & collection;
public:

	BLCollection(T & collection): collection(collection) {}

	iterator begin() {
		iterator iter;
		collection.begin(iter);
		return iter;
	}

	iterator end() {
		return collection.end();
	}
};

/// Utility function to enable template argument deduction
template <typename T>
auto collection(T & collection) -> BLCollection<T, decltype(collection.end())>
{
	return BLCollection<T, decltype(collection.end())>(collection);
}


/// Class representing array of some number of flags, created depending on some enum class type and enforces that
/// the enum is used to access the flags, thus improving type safety
/// This class is intended to abstract std::vector<char> with bitfield manipulation but it has the caveat that each flag
/// is stored in different chunk of memmory which speeds up loops on only 1 flag but slows loops on multiple flags (AoS -> SoA)
/// @FlagEnum - type of the enum whose values that represent the flag names
/// @N - the count of the flags, must be value of the enum type
/// Example usage
/// 	enum class InstanceFlags { UNSUPPORTED, HIDDEN, LIGHT, MESH_LIGHT, CLIPPER, FLAGS_COUNT };
/// 	Blender::FlagsArray<InstanceFlags, InstanceFlags::FLAGS_COUNT> instanceFlags;
template <typename FlagEnum, FlagEnum N>
struct FlagsArray {
	std::vector<bool> containers[static_cast<int>(N)];
public:
	/// Helper class for accessing all flags at a specific index
	class Flags{
		FlagsArray<FlagEnum, N> & data;
		int index;
	public:
		Flags(FlagsArray<FlagEnum, N> & data, int index)
			: data(data)
			, index(index)
		{}

		/// Set the @flag
		void set(FlagEnum flag) {
			data.containers[static_cast<int>(flag)][index] = true;
		}

		/// Clear the @flag
		void clear(FlagEnum flag) {
			data.containers[static_cast<int>(flag)][index] = false;
		}

		/// Get the @flag
		bool get(FlagEnum flag) {
			return data.containers[static_cast<int>(flag)][index];
		}
	};

	/// Initialize with size, all flags set to false
	FlagsArray(int size) {
		for (int c = 0; c < static_cast<int>(N); c++) {
			containers[c].resize(size, false);
		}
	}

	FlagsArray() {}

	/// Add one element to all flags
	void push_back(bool flag) {
		for (int c = 0; c < static_cast<int>(N); c++) {
			containers[c].push_back(flag);
		}
	}

	/// Get a accessor for all flags at @index
	Flags get_flags(int index) {
		return {*this, index};
	}
};


} // namespace Blender
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_BLENDER_H
