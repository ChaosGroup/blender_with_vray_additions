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

#include "CGR_config.h"

#include <Python.h>

#include "CGR_string.h"
#include "CGR_vrscene.h"
#include "CGR_vray_for_blender.h"

#include "vrscene_exporter/exp_scene.h"
#include "vrscene_exporter/nodes/exp_nodes.h"
#include "vrscene_exporter/vrscene_api.h"

#include "DNA_material_types.h"
#include "BLI_math.h"
#include "BKE_context.h"

extern "C" {
#  include "mathutils/mathutils.h"
}


static PyObject* mExportStart(PyObject *self, PyObject *args)
{
	char *jsonDirpath = NULL;

	if(NOT(PyArg_ParseTuple(args, "s", &jsonDirpath)))
		return NULL;

	VRayExportable::initPluginDesc(jsonDirpath);

	Py_RETURN_NONE;
}


static PyObject* mExportFree(PyObject *self)
{
	VRayExportable::freePluginDesc();

	Py_RETURN_NONE;
}


static PyObject* mExportInit(PyObject *self, PyObject *args, PyObject *keywds)
{
	long      contextPtr = 0;
	long      scenePtr   = 0;
	long      useNodes   = false;

	PyObject *engine     = NULL;
	PyObject *obFile     = NULL;

	PyObject *geomFile      = NULL;
	PyObject *lightsFile    = NULL;
	PyObject *materialsFile = NULL;
	PyObject *texturesFile  = NULL;

	static char *kwlist[] = {
		_C("engine"),         // 0
		_C("context"),        // 1
		_C("objectFile"),     // 2
		_C("geometryFile"),   // 3
		_C("lightsFile"),     // 4
		_C("scene"),          // 5
		_C("useNodes"),       // 6
		_C("materialFile"),   // 7
		_C("textureFile"),    // 8
		NULL
	};

	//                                  01234 5678
	static const char  kwlistTypes[] = "OlOOO|llOO";

	if(NOT(PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
									   &engine,
									   &contextPtr,
									   &obFile,
									   &geomFile,
									   &lightsFile,
									   &scenePtr,
									   &useNodes,
									   &materialsFile,
									   &texturesFile)))
		return NULL;

	PointerRNA engineRnaPtr;
	RNA_pointer_create(NULL, &RNA_RenderEngine, (void*)PyLong_AsVoidPtr(engine), &engineRnaPtr);
	BL::RenderEngine renderEngine(engineRnaPtr);

	bContext *C = (bContext*)(intptr_t)contextPtr;

	Scene *sce  = scenePtr ? (Scene*)(intptr_t)scenePtr : CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	PointerRNA sceneRnaPtr;
	RNA_id_pointer_create((ID*)sce, &sceneRnaPtr);
	BL::Scene scene(sceneRnaPtr);

	PointerRNA dataPtr;
	RNA_id_pointer_create((ID*)main, &dataPtr);
	BL::BlendData data(dataPtr);

	VRayExportable::m_exportSettings = new ExpoterSettings(scene, data, renderEngine);
	VRayExportable::m_exportSettings->m_sce  = sce;
	VRayExportable::m_exportSettings->m_main = main;
	VRayExportable::m_exportSettings->m_useNodeTree = useNodes;
	VRayExportable::m_exportSettings->m_fileObject = obFile;
	VRayExportable::m_exportSettings->m_fileGeom   = geomFile;
	VRayExportable::m_exportSettings->m_fileLights = lightsFile;
	VRayExportable::m_exportSettings->m_fileMat    = materialsFile;
	VRayExportable::m_exportSettings->m_fileTex    = texturesFile;

	VRsceneExporter *exporter = new VRsceneExporter(VRayExportable::m_exportSettings);

	return PyLong_FromVoidPtr(exporter);
}


static PyObject* mExportExit(PyObject *self, PyObject *value)
{
	delete (VRsceneExporter*)PyLong_AsVoidPtr(value);

	delete VRayExportable::m_exportSettings;
	VRayExportable::m_exportSettings = NULL;

	Py_RETURN_NONE;
}


static PyObject* mExportInitCache(PyObject *self, PyObject *args)
{
	int  isAnimation   = false;
	int  checkAnimated = false;

	if(NOT(PyArg_ParseTuple(args, "ii", &isAnimation, &checkAnimated)))
		return NULL;

	VRayExportable::setAnimationMode(isAnimation, checkAnimated);

	Py_RETURN_NONE;
}


static PyObject* mExportClearFrames(PyObject *self)
{
	VRayExportable::clearFrames();
	VRayExportable::setAnimationMode(false, false);
	VRayNodePluginExporter::clearNodesCache();

	Py_RETURN_NONE;
}


static PyObject* mExportClearCache(PyObject *self)
{
	VRayExportable::clearCache();
	VRayNodePluginExporter::clearNamesCache();

	Py_RETURN_NONE;
}


static PyObject* mExportScene(PyObject *self, PyObject *args)
{
	long exporterPtr   = 0;

	int exportNodes    = true;
	int exportGeometry = true;

	if(NOT(PyArg_ParseTuple(args, "lii", &exporterPtr, &exportNodes, &exportGeometry)))
		return NULL;

	VRsceneExporter *exporter = (VRsceneExporter*)(intptr_t)exporterPtr;
	exporter->exportScene(exportNodes, exportGeometry);

	Py_RETURN_NONE;
}


static PyObject* mExportSmokeDomain(PyObject *self, PyObject *args)
{
	long        contextPtr;
	long        objectPtr;
	long        smdPtr;
	const char *pluginName;
	const char *lights;
	PyObject   *fileObject;

	if(NOT(PyArg_ParseTuple(args, "lllssO", &contextPtr, &objectPtr, &smdPtr, &pluginName, &lights, &fileObject))) {
		return NULL;
	}

	bContext          *C   = (bContext*)(intptr_t)contextPtr;
	Object            *ob  = (Object*)(intptr_t)objectPtr;
	SmokeModifierData *smd = (SmokeModifierData*)(intptr_t)smdPtr;

	Scene *sce = CTX_data_scene(C);

	ExportSmokeDomain(fileObject, sce, ob, smd, pluginName, lights);

	Py_RETURN_NONE;
}


static PyObject* mExportSmoke(PyObject *self, PyObject *args)
{
	long        contextPtr;
	long        objectPtr;
	long        smdPtr;
	const char *pluginName;
	PyObject   *fileObject;
	int         p_interpolation;

	if(NOT(PyArg_ParseTuple(args, "lllisO", &contextPtr, &objectPtr, &smdPtr, &p_interpolation, &pluginName, &fileObject))) {
		return NULL;
	}

	bContext          *C   = (bContext*)(intptr_t)contextPtr;
	Object            *ob  = (Object*)(intptr_t)objectPtr;
	SmokeModifierData *smd = (SmokeModifierData*)(intptr_t)smdPtr;

	Scene *sce = CTX_data_scene(C);

	ExportTexVoxelData(fileObject, sce, ob, smd, pluginName, p_interpolation);

	Py_RETURN_NONE;
}


static PyObject* mExportFluid(PyObject *self, PyObject *args)
{
	long        contextPtr;
	long        objectPtr;
	long        smdPtr;
	PyObject   *propGroup;
	const char *pluginName;
	PyObject   *fileObject;

	if(NOT(PyArg_ParseTuple(args, "lllOsO", &contextPtr, &objectPtr, &smdPtr, &propGroup, &pluginName, &fileObject))) {
		return NULL;
	}

	bContext          *C   = (bContext*)(intptr_t)contextPtr;
	Object            *ob  = (Object*)(intptr_t)objectPtr;
	SmokeModifierData *smd = (SmokeModifierData*)(intptr_t)smdPtr;

	Scene *sce = CTX_data_scene(C);

	ExportVoxelDataAsFluid(fileObject, sce, ob, smd, propGroup, pluginName);

	Py_RETURN_NONE;
}


static PyObject* mExportHair(PyObject *self, PyObject *args)
{
	long        contextPtr;
	long        objectPtr;
	long        psysPtr;
	const char *pluginName;
	PyObject   *fileObject;

	if(NOT(PyArg_ParseTuple(args, "lllsO", &contextPtr, &objectPtr, &psysPtr, &pluginName, &fileObject))) {
		return NULL;
	}

	bContext       *C    = (bContext*)(intptr_t)contextPtr;
	Object         *ob   = (Object*)(intptr_t)objectPtr;
	ParticleSystem *psys = (ParticleSystem*)(intptr_t)psysPtr;

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	if(ExportGeomMayaHair(fileObject, sce, main, ob, psys, pluginName)) {
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyObject* mExportMesh(PyObject *self, PyObject *args)
{
	long        contextPtr;
	long        objectPtr;
	const char *pluginName;
	PyObject   *propGroup;
	PyObject   *fileObject;

	if(NOT(PyArg_ParseTuple(args, "llsOO", &contextPtr, &objectPtr, &pluginName, &propGroup, &fileObject))) {
		return NULL;
	}

	bContext *C = (bContext*)(intptr_t)contextPtr;
	Object   *ob = (Object*)(intptr_t)objectPtr;

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	if(ExportGeomStaticMesh(fileObject, sce, ob, main, pluginName, propGroup)) {
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyObject* mExportNode(PyObject *self, PyObject *args)
{
	long      contextPtr;
	long      objectPtr;
	PyObject *nodeFile;
	PyObject *geomFile;

	char      pluginName[CGR_MAX_PLUGIN_NAME];

	if(NOT(PyArg_ParseTuple(args, "llOO", &contextPtr, &objectPtr, &nodeFile, &geomFile))) {
		return NULL;
	}

	bContext *C = (bContext*)(intptr_t)contextPtr;
	Object *ob  = (Object*)(intptr_t)objectPtr;

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	sprintf(pluginName, "%s", ob->id.name);
	StripString(pluginName);

	// TODO

	Py_RETURN_NONE;
}


static PyObject* mExportDupli(PyObject *self, PyObject *args)
{
	long      contextPtr;
	long      objectPtr;
	PyObject *nodeFile;
	PyObject *geomFile;

	if(NOT(PyArg_ParseTuple(args, "llOO", &contextPtr, &objectPtr, &nodeFile, &geomFile))) {
		return NULL;
	}

	bContext *C = (bContext*)(intptr_t)contextPtr;
	Object *ob  = (Object*)(intptr_t)objectPtr;

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	// TODO

	Py_RETURN_NONE;
}


static PyObject* mGetTransformHex(PyObject *self, PyObject *value)
{
	if (MatrixObject_Check(value)) {
		MatrixObject *transform = (MatrixObject*)value;

		float tm[4][4];
		char  tmBuf[CGR_TRANSFORM_HEX_SIZE]  = "";
		char  buf[CGR_TRANSFORM_HEX_SIZE+20] = "";

		copy_v3_v3(tm[0], MATRIX_COL_PTR(transform, 0));
		copy_v3_v3(tm[1], MATRIX_COL_PTR(transform, 1));
		copy_v3_v3(tm[2], MATRIX_COL_PTR(transform, 2));
		copy_v3_v3(tm[3], MATRIX_COL_PTR(transform, 3));

		GetTransformHex(tm, tmBuf);
		sprintf(buf, "TransformHex(\"%s\")", tmBuf);

		return _PyUnicode_FromASCII(buf, strlen(buf));
	}

	Py_RETURN_NONE;
}


static PyObject* mSetSkipObjects(PyObject *self, PyObject *args)
{
	Py_ssize_t  exporterPtr;
	PyObject   *skipList;

	if(NOT(PyArg_ParseTuple(args, "nO", &exporterPtr, &skipList)))
		return NULL;

	VRsceneExporter *exporter = (VRsceneExporter*)(intptr_t)exporterPtr;

	if(PySequence_Check(skipList)) {
		int listSize = PySequence_Size(skipList);
		if(listSize > 0) {
			for(int i = 0; i < listSize; ++i) {
				PyObject *item = PySequence_GetItem(skipList, i);

				PyObject *value = PyNumber_Long(item);
				if(PyNumber_Long(value))
					exporter->addSkipObject((void*)PyLong_AsLong(value));

				Py_DecRef(item);
			}
		}
	}

	Py_RETURN_NONE;
}


static PyObject* mSetHideFromView(PyObject *self, PyObject *args)
{
	Py_ssize_t  exporterPtr;
	PyObject   *hideFromViewDict;

	if(NOT(PyArg_ParseTuple(args, "nO", &exporterPtr, &hideFromViewDict)))
		return NULL;

	VRsceneExporter *exporter = (VRsceneExporter*)(intptr_t)exporterPtr;

	const char *hideFromViewKeys[] = {
		"all",
		"camera",
		"gi",
		"reflect",
		"refract",
		"shadows"
	};

	int nHideFromViewKeys = sizeof(hideFromViewKeys) / sizeof(hideFromViewKeys[0]);

	if(PyDict_Check(hideFromViewDict)) {
		for(int k = 0; k < nHideFromViewKeys; ++k) {
			const char *key = hideFromViewKeys[k];

			PyObject *hideSet = PyDict_GetItemString(hideFromViewDict, key);
			if(PySet_Check(hideSet)) {
				int setSize = PySet_Size(hideSet);
				if(setSize > 0) {
					Py_ssize_t  pos = 0;
					PyObject   *item;
					Py_hash_t   hash;
					while(_PySet_NextEntry(hideSet, &pos, &item, &hash)) {
						PyObject *value = PyNumber_Long(item);
						if(PyNumber_Long(value))
							exporter->addToHideFromViewList(key, (void*)PyLong_AsLong(value));
						Py_DecRef(item);
					}
				}
			}
		}
	}

	Py_RETURN_NONE;
}


static PyMethodDef methods[] = {
	{"start",             mExportStart ,      METH_VARARGS, "Startup init"},
	{"free", (PyCFunction)mExportFree,        METH_NOARGS,  "Free resources"},

	{"init", (PyCFunction)mExportInit ,       METH_VARARGS|METH_KEYWORDS, "Init exporter"},
	{"exit",              mExportExit ,       METH_O,                     "Shutdown exporter"},

	{"exportScene",       mExportScene,       METH_VARARGS, "Export scene to the *.vrscene file"},

	{"exportDupli",       mExportDupli,       METH_VARARGS, "Export dupli / particles"},
	{"exportMesh",        mExportMesh,        METH_VARARGS, "Export mesh"},
	{"exportSmoke",       mExportSmoke,       METH_VARARGS, "Export voxel data"},
	{"exportSmokeDomain", mExportSmokeDomain, METH_VARARGS, "Export domain data"},
	{"exportHair",        mExportHair,        METH_VARARGS, "Export hair"},
	{"exportNode",        mExportNode,        METH_VARARGS, "Export Node description"},
	{"exportFluid",       mExportFluid,       METH_VARARGS, "Export voxel data as TexMayaFluid"},

	{"initCache",                mExportInitCache,   METH_VARARGS, "Init animation cache"},
	{"clearFrames", (PyCFunction)mExportClearFrames, METH_NOARGS,  "Clear frame cache"},
	{"clearCache",  (PyCFunction)mExportClearCache,  METH_NOARGS,  "Clear name cache"},

	{"getTransformHex",   mGetTransformHex,   METH_O,       "Get transform hex string"},
	{"setSkipObjects",    mSetSkipObjects,    METH_VARARGS, "Set a list of objects to skip from exporting"},
	{"setHideFromView",   mSetHideFromView,   METH_VARARGS, "Setup overrides for objects for the current view"},

	{NULL, NULL, 0, NULL},
};


static struct PyModuleDef module = {
	PyModuleDef_HEAD_INIT,
	"_vray_for_blender",
	"V-Ray For Blender export helper module",
	-1,
	methods,
	NULL, NULL, NULL, NULL
};


void* VRayForBlender_initPython()
{
	return (void*)PyModule_Create(&module);
}
