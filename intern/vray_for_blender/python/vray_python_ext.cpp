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

#include "cgr_config.h"

#include <Python.h>

#include "cgr_string.h"
#include "cgr_vrscene.h"
#include "cgr_vray_for_blender.h"

#include "exp_scene.h"
#include "exp_nodes.h"
#include "exp_settings.h"
#include "exp_api.h"

#include "DNA_material_types.h"
#include "BLI_math.h"
#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

extern "C" {
#  include "BKE_idprop.h"
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


static PyObject* mExportInitAnim(PyObject *self, PyObject *args)
{
	int isAnimation = false;
	int frameStart  = 1;
	int frameStep   = 1;

	if(NOT(PyArg_ParseTuple(args, "iii", &isAnimation, &frameStart, &frameStep)))
		return NULL;

	ExporterSettings::gSet.m_isAnimation = isAnimation;
	ExporterSettings::gSet.m_frameStart  = frameStart;
	ExporterSettings::gSet.m_frameStep   = frameStep;

	Py_RETURN_NONE;
}


static PyObject* mExportInit(PyObject *self, PyObject *args, PyObject *keywds)
{
	PyObject *py_engine     = NULL;
	PyObject *py_context    = NULL;
	PyObject *py_scene      = NULL;
	PyObject *py_data       = NULL;

	PyObject *mainFile      = NULL;
	PyObject *envFile       = NULL;
	PyObject *obFile        = NULL;
	PyObject *geomFile      = NULL;
	PyObject *lightsFile    = NULL;
	PyObject *materialsFile = NULL;
	PyObject *texturesFile  = NULL;

	char     *drSharePath   = NULL;

	static char *kwlist[] = {
		_C("engine"),         // 0
		_C("context"),        // 1
		_C("scene"),          // 2
		_C("data"),           // 3
		_C("mainFile"),       // 4
		_C("envFile"),        // 5
		_C("objectFile"),     // 6
		_C("geometryFile"),   // 7
		_C("lightsFile"),     // 8
		_C("materialFile"),   // 9
		_C("textureFile"),    // 10
		_C("drSharePath"),    // 11
		NULL
	};

	//                                  012345678911
	//                                            01
	static const char  kwlistTypes[] = "OOOOOOOOOOOs";

	if(NOT(PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
									   &py_engine,     // 0
									   &py_context,    // 1
									   &py_scene,      // 2
									   &py_data,       // 3
									   &mainFile,      // 4
									   &envFile,       // 5
									   &obFile,        // 6
									   &geomFile,      // 7
									   &lightsFile,    // 8
									   &materialsFile, // 9
									   &texturesFile,  // 10
									   &drSharePath))) // 11
		return NULL;

	PointerRNA engineRNA;
	RNA_pointer_create(NULL, &RNA_RenderEngine, (void*)PyLong_AsVoidPtr(py_engine), &engineRNA);
	BL::RenderEngine bl_engine(engineRNA);

	PointerRNA contextRNA;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(py_context), &contextRNA);
	BL::Context bl_context(contextRNA);

	PointerRNA sceneRNA;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(py_scene), &sceneRNA);
	BL::Scene bl_scene(sceneRNA);

	PointerRNA dataRNA;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(py_data), &dataRNA);
	BL::BlendData bl_data(dataRNA);

	VRayNodeExporter::init(bl_data);

	ExporterSettings::gSet.reset();
	ExporterSettings::gSet.init(bl_context, bl_scene, bl_data, bl_engine);
	ExporterSettings::gSet.m_sce  = (Scene*)bl_scene.ptr.data;
	ExporterSettings::gSet.m_main = (Main*)bl_data.ptr.data;

	ExporterSettings::gSet.m_fileMain   = mainFile;
	ExporterSettings::gSet.m_fileObject = obFile;
	ExporterSettings::gSet.m_fileEnv    = envFile;
	ExporterSettings::gSet.m_fileGeom   = geomFile;
	ExporterSettings::gSet.m_fileLights = lightsFile;
	ExporterSettings::gSet.m_fileMat    = materialsFile;
	ExporterSettings::gSet.m_fileTex    = texturesFile;

	if(drSharePath) {
		ExporterSettings::gSet.m_drSharePath = drSharePath;
	}

	ExporterSettings::gSet.m_exporter = new VRsceneExporter();

	return PyLong_FromVoidPtr(ExporterSettings::gSet.m_exporter);
}


static PyObject* mExportExit(PyObject *self, PyObject *value)
{
	VRayExportable::clearFrames();
	VRayExportable::clearCache();
	VRayNodePluginExporter::clearNodesCache();
	VRayNodePluginExporter::clearNamesCache();

	void *exporterPtr = PyLong_AsVoidPtr(value);
	if(exporterPtr) {
		delete (VRsceneExporter*)exporterPtr;
	}

	Py_RETURN_NONE;
}


static PyObject* mExportSetFrame(PyObject *self, PyObject *args)
{
	int frameCurrent = 0;

	if(NOT(PyArg_ParseTuple(args, "i", &frameCurrent)))
		return NULL;

	ExporterSettings::gSet.m_frameCurrent = frameCurrent;

	Py_RETURN_NONE;
}


static PyObject* mExportClearFrames(PyObject *self)
{
	VRayExportable::clearFrames();
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
	PyObject *py_exporter = NULL;

	int exportNodes    = true;
	int exportGeometry = true;

	if(NOT(PyArg_ParseTuple(args, "Oii", &py_exporter, &exportNodes, &exportGeometry)))
		return NULL;

	if(py_exporter) {
		VRsceneExporter *exporter = (VRsceneExporter*)PyLong_AsVoidPtr(py_exporter);
		if(exporter) {
			int err = exporter->exportScene(exportNodes, exportGeometry);
			if(err) {
				if(err == 1) {
					PyErr_SetString(PyExc_RuntimeError, "Export is interrupted by the user!");
				}
				else {
					PyErr_SetString(PyExc_RuntimeError, "Unknown export error!");
				}
				return NULL;
			}
		}
	}

	Py_RETURN_NONE;
}


static PyObject* mExportSmokeDomain(PyObject *self, PyObject *args)
{
	PyObject   *py_context = NULL;
	PyObject   *py_object  = NULL;
	PyObject   *py_smd     = NULL;
	const char *pluginName = NULL;
	const char *lights     = NULL;
	PyObject   *fileObject = NULL;

	if(NOT(PyArg_ParseTuple(args, "OOOssO", &py_context, &py_object, &py_smd, &pluginName, &lights, &fileObject))) {
		return NULL;
	}

	bContext          *C   = (bContext*)PyLong_AsVoidPtr(py_context);
	Object            *ob  = (Object*)PyLong_AsVoidPtr(py_object);
	SmokeModifierData *smd = (SmokeModifierData*)PyLong_AsVoidPtr(py_smd);

	Scene *sce = CTX_data_scene(C);

	ExportSmokeDomain(fileObject, sce, ob, smd, pluginName, lights);

	Py_RETURN_NONE;
}


static PyObject* mExportSmoke(PyObject *self, PyObject *args)
{
	PyObject   *py_context    = NULL;
	PyObject   *py_object     = NULL;
	PyObject   *py_smd        = NULL;
	int         interpolation = 0;
	const char *pluginName    = NULL;
	PyObject   *fileObject    = NULL;

	if(NOT(PyArg_ParseTuple(args, "OOOisO", &py_context, &py_object, &py_smd, &interpolation, &pluginName, &fileObject))) {
		return NULL;
	}

	bContext          *C   = (bContext*)PyLong_AsVoidPtr(py_context);
	Object            *ob  = (Object*)PyLong_AsVoidPtr(py_object);
	SmokeModifierData *smd = (SmokeModifierData*)PyLong_AsVoidPtr(py_smd);

	Scene *sce = CTX_data_scene(C);

	ExportTexVoxelData(fileObject, sce, ob, smd, pluginName, interpolation);

	Py_RETURN_NONE;
}


static PyObject* mExportFluid(PyObject *self, PyObject *args)
{
	PyObject   *py_context = NULL;
	PyObject   *py_object  = NULL;
	PyObject   *py_smd     = NULL;
	PyObject   *propGroup  = NULL;
	const char *pluginName = NULL;
	PyObject   *fileObject = NULL;

	if(NOT(PyArg_ParseTuple(args, "OOOOsO", &py_context, &py_object, &py_smd, &propGroup, &pluginName, &fileObject))) {
		return NULL;
	}

	bContext          *C   = (bContext*)PyLong_AsVoidPtr(py_context);
	Object            *ob  = (Object*)PyLong_AsVoidPtr(py_object);
	SmokeModifierData *smd = (SmokeModifierData*)PyLong_AsVoidPtr(py_smd);

	Scene *sce = CTX_data_scene(C);

	ExportVoxelDataAsFluid(fileObject, sce, ob, smd, pluginName, 0);

	Py_RETURN_NONE;
}


static PyObject* mExportHair(PyObject *self, PyObject *args)
{
	PyObject   *py_context = NULL;
	PyObject   *py_object  = NULL;
	PyObject   *py_psys    = NULL;
	const char *pluginName = NULL;
	PyObject   *fileObject = NULL;

	if(NOT(PyArg_ParseTuple(args, "OOOsO", &py_context, &py_object, &py_psys, &pluginName, &fileObject))) {
		return NULL;
	}

	bContext       *C    = (bContext*)PyLong_AsVoidPtr(py_context);
	Object         *ob   = (Object*)PyLong_AsVoidPtr(py_object);
	ParticleSystem *psys = (ParticleSystem*)PyLong_AsVoidPtr(py_psys);

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	if(ExportGeomMayaHair(fileObject, sce, main, ob, psys, pluginName)) {
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyObject* mExportMesh(PyObject *self, PyObject *args)
{
	PyObject   *py_context = NULL;
	PyObject   *py_object  = NULL;
	const char *pluginName = NULL;
	PyObject   *propGroup  = NULL;
	PyObject   *fileObject = NULL;

	if(NOT(PyArg_ParseTuple(args, "OOsOO", &py_context, &py_object, &pluginName, &propGroup, &fileObject))) {
		return NULL;
	}

	bContext *C = (bContext*)PyLong_AsVoidPtr(py_context);
	Object   *ob = (Object*)PyLong_AsVoidPtr(py_object);

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	if(ExportGeomStaticMesh(fileObject, sce, ob, main, pluginName, propGroup)) {
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyObject* mExportNode(PyObject *self, PyObject *args)
{
	PyObject *ntreePtr    = NULL;
	PyObject *nodePtr     = NULL;
	PyObject *socketPtr   = NULL;

	if(NOT(PyArg_ParseTuple(args, "OOO", &ntreePtr, &nodePtr, &socketPtr))) {
		return NULL;
	}

	PointerRNA ntreeRNA;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(ntreePtr), &ntreeRNA);
	BL::NodeTree ntree(ntreeRNA);

	PointerRNA nodeRNA;
	RNA_pointer_create((ID*)ntree.ptr.data, &RNA_Node, PyLong_AsVoidPtr(nodePtr), &nodeRNA);
	BL::Node node(nodeRNA);

	BL::NodeSocket fromSocket(PointerRNA_NULL);
	if (socketPtr != Py_None) {
		PointerRNA socketRNA;
		RNA_pointer_create((ID*)ntree.ptr.data, &RNA_NodeSocket, PyLong_AsVoidPtr(socketPtr), &socketRNA);
		fromSocket = BL::NodeSocket(socketRNA);
	}

	VRayNodeContext nodeCtx;
	std::string pluginName = VRayNodeExporter::exportVRayNode(ntree, node, fromSocket, nodeCtx);

	return PyUnicode_FromString(pluginName.c_str());
}


static PyObject* mExportShaders(PyObject *self, PyObject *args)
{
	PRINT_ERROR("exportShaders is not yet implemented...");
	Py_RETURN_NONE;
}


static void ExportObjectMaterials(BL::BlendData b_data, BL::Object b_ob)
{
	BL::Object::material_slots_iterator slIt;
	for(b_ob.material_slots.begin(slIt); slIt != b_ob.material_slots.end(); ++slIt) {
		BL::MaterialSlot b_sl = *slIt;
		if (b_sl) {
			BL::Material b_ma = b_sl.material();
			if (b_ma) {
				VRayNodeExporter::exportMaterial(b_data, b_ma);
			}
		}
	}
}


static PyObject* mExportObject(PyObject *self, PyObject *args)
{
	PyObject *py_exporter = NULL;
	PyObject *py_data     = NULL;
	PyObject *py_object   = NULL;

	if(NOT(PyArg_ParseTuple(args, "OOO", &py_object, &py_data, &py_exporter))) {
		return NULL;
	}

	PointerRNA objectRNA;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(py_object), &objectRNA);
	BL::Object b_ob(objectRNA);

	PointerRNA dataRNA;
	RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(py_data), &dataRNA);
	BL::BlendData b_data(dataRNA);

	VRsceneExporter *exporter = (VRsceneExporter*)PyLong_AsVoidPtr(py_exporter);
	if(exporter) {
		ExportObjectMaterials(b_data, b_ob);

		if(b_ob.is_duplicator()) {
			b_ob.dupli_list_create(G_MAIN, ExporterSettings::gSet.b_scene, 2);

			BL::Object::dupli_list_iterator b_dup;
			for(b_ob.dupli_list.begin(b_dup); b_dup != b_ob.dupli_list.end(); ++b_dup) {
				BL::DupliObject bl_dupliOb      = *b_dup;
				BL::Object      bl_duplicatedOb =  bl_dupliOb.object();

				if(bl_dupliOb.hide() || bl_duplicatedOb.hide_render())
					continue;

				DupliObject *dupliOb = (DupliObject*)bl_dupliOb.ptr.data;
				if(NOT(GEOM_TYPE(dupliOb->ob)))
					continue;

				ExportObjectMaterials(b_data, bl_duplicatedOb);
			}

			b_ob.dupli_list_clear();
		}

		exporter->exportObjectBase(b_ob);
	}

	Py_RETURN_NONE;
}


static PyObject* mExportObjectsPre(PyObject *self, PyObject *args)
{
	PyObject *py_exporter = NULL;
	if(NOT(PyArg_ParseTuple(args, "O", &py_exporter))) {
		return NULL;
	}

	VRsceneExporter *exporter = (VRsceneExporter*)PyLong_AsVoidPtr(py_exporter);
	if(exporter) {
		exporter->exportObjectsPre();
	}

	Py_RETURN_NONE;
}


static PyObject* mExportObjectsPost(PyObject *self, PyObject *args)
{
	PyObject *py_exporter = NULL;
	if(NOT(PyArg_ParseTuple(args, "O", &py_exporter))) {
		return NULL;
	}

	VRsceneExporter *exporter = (VRsceneExporter*)PyLong_AsVoidPtr(py_exporter);
	if(exporter) {
		exporter->exportObjectsPost();
		exporter->exportClearCaches();
		VRayExportable::clearCache();
		VRayNodePluginExporter::clearNamesCache();
	}

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
	PyObject *exporterPtr = NULL;
	PyObject *skipList    = NULL;

	if(NOT(PyArg_ParseTuple(args, "OO", &exporterPtr, &skipList)))
		return NULL;

	VRsceneExporter *exporter = (VRsceneExporter*)PyLong_AsVoidPtr(exporterPtr);

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
	PyObject *exporterPtr      = NULL;
	PyObject *hideFromViewDict = NULL;

	if(NOT(PyArg_ParseTuple(args, "OO", &exporterPtr, &hideFromViewDict)))
		return NULL;

	VRsceneExporter *exporter = (VRsceneExporter*)PyLong_AsVoidPtr(exporterPtr);

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
							exporter->addToHideFromViewList(key, PyLong_AsVoidPtr(value));
						Py_DecRef(item);
					}
				}
			}
		}
	}

	Py_RETURN_NONE;
}


static PyObject* mUpdatePreview(PyObject *self, PyObject *args)
{
	PyObject *py_context  = NULL;
	int       update_flag = 0;

	if(NOT(PyArg_ParseTuple(args, "Oi", &py_context, &update_flag))) {
		return NULL;
	}

	bContext *C = (bContext*)PyLong_AsVoidPtr(py_context);

	int update_type = 0;
	switch(update_flag) {
		case 0:	update_type |= NC_WORLD; break;
		case 1:	update_type |= NC_MATERIAL; break;
		case 2:	update_type |= NC_LAMP; break;
		default: break;
	}

	WM_event_add_notifier(C, update_type | ND_SHADING_PREVIEW, NULL);

	Py_RETURN_NONE;
}


static PyObject* mIsIDUsedInIDProp(PyObject *self, PyObject *args)
{
	PyObject *py_object   = NULL;

	if(NOT(PyArg_ParseTuple(args, "O", &py_object))) {
		return NULL;
	}

	ID *id = (ID*)PyLong_AsVoidPtr(py_object);
	if (id && IDP_is_ID_used(id)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_NONE;
}


static PyMethodDef methods[] = {
	{"start",             mExportStart ,      METH_VARARGS, "Startup init"},
	{"free", (PyCFunction)mExportFree,        METH_NOARGS,  "Free resources"},

	{"init", (PyCFunction)mExportInit ,       METH_VARARGS|METH_KEYWORDS, "Init exporter"},
	{"exit",              mExportExit ,       METH_O,                     "Shutdown exporter"},

	{"exportScene",       mExportScene,       METH_VARARGS, "Export scene to the *.vrscene file"},

	{"exportMesh",        mExportMesh,        METH_VARARGS, "Export mesh"},
	{"exportSmoke",       mExportSmoke,       METH_VARARGS, "Export voxel data"},
	{"exportSmokeDomain", mExportSmokeDomain, METH_VARARGS, "Export domain data"},
	{"exportHair",        mExportHair,        METH_VARARGS, "Export hair"},
	{"exportFluid",       mExportFluid,       METH_VARARGS, "Export voxel data as TexMayaFluid"},

	{"exportNode",        mExportNode,        METH_VARARGS, "Export node tree node"},
	{"exportShaders",     mExportShaders,     METH_VARARGS, "Export shaders from selection"},

	{"exportObject",      mExportObject,      METH_VARARGS, "Export object"},
	{"exportObjectsPre",  mExportObjectsPre,  METH_VARARGS, "Init data defore objects export"},
	{"exportObjectsPost", mExportObjectsPost, METH_VARARGS, "Write / cleanup data after objects export"},

	{"initAnimation",            mExportInitAnim,    METH_VARARGS, "Init animation settings"},
	{"setFrame",                 mExportSetFrame,    METH_VARARGS, "Set current frame"},
	{"clearFrames", (PyCFunction)mExportClearFrames, METH_NOARGS,  "Clear frame cache"},
	{"clearCache",  (PyCFunction)mExportClearCache,  METH_NOARGS,  "Clear name cache"},

	{"getTransformHex",   mGetTransformHex,   METH_O,       "Get transform hex string"},
	{"setSkipObjects",    mSetSkipObjects,    METH_VARARGS, "Set a list of objects to skip from exporting"},
	{"setHideFromView",   mSetHideFromView,   METH_VARARGS, "Setup overrides for objects for the current view"},

	{"updatePreview",     mUpdatePreview,     METH_VARARGS, "Generate preview update event"},

	{"isIdUsed",          mIsIDUsedInIDProp,  METH_VARARGS, "Check if ID is used in ID properties"},

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
