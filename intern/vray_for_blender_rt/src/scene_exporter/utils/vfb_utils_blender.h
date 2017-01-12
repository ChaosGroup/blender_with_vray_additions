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

static boost::shared_mutex vfbExporterBlenderLock;
#define WRITE_LOCK_BLENDER_RAII boost::unique_lock<boost::shared_mutex> _raiiWriteLock(vfbExporterBlenderLock);
#define READ_LOCK_BLENDER_RAII boost::shared_lock<boost::shared_mutex> _raiiReadLock(vfbExporterBlenderLock);

namespace VRayForBlender {
namespace Blender {

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

std::string   GetIDName(BL::ID id, const std::string &prefix="");
std::string   GetIDNameAuto(BL::ID id);

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

} // namespace Blender
} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_UTILS_BLENDER_H
