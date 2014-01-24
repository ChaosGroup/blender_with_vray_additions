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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "CGR_config.h"

#include <Python.h>

#include "CGR_string.h"
#include "CGR_vrscene.h"
#include "CGR_vray_for_blender.h"

#include "vrscene_exporter/exp_scene.h"
#include "vrscene_exporter/vrscene_api.h"

#include "DNA_material_types.h"
#include "BLI_math.h"
#include "BKE_context.h"

extern "C" {
#  include "mathutils/mathutils.h"
}


static PyObject* mExportInit(PyObject *self, PyObject *args, PyObject *keywds)
{
	int       contextPtr    = 0;
	int       isAnimation   = false;
	int       checkAnimated = ANIM_CHECK_NONE;

	PyObject *engine     = NULL;
	PyObject *obFile     = NULL;
	PyObject *geomFile   = NULL;
	PyObject *lightsFile = NULL;

	static char *kwlist[] = { "context", "engine", "isAnimation", "checkAnimated", "objectFile", "geometryFile", "lightsFile", NULL };

	if(NOT(PyArg_ParseTupleAndKeywords(args, keywds, "i|OiiOOO", kwlist,
									   &contextPtr, &engine, &isAnimation, &checkAnimated, &obFile, &geomFile, &lightsFile)))
		return NULL;

	PointerRNA engineRnaPtr;
	RNA_pointer_create(NULL, &RNA_RenderEngine, (void*)PyLong_AsVoidPtr(engine), &engineRnaPtr);
	BL::RenderEngine renderEngine(engineRnaPtr);

	bContext *C = (bContext*)(intptr_t)contextPtr;

	ExpoterSettings *settings = new ExpoterSettings(renderEngine);
	settings->m_sce  = CTX_data_scene(C);
	settings->m_main = CTX_data_main(C);

	settings->m_animation     = isAnimation;
	settings->m_checkAnimated = checkAnimated;

	settings->m_fileObject = obFile;
	settings->m_fileGeom   = geomFile;
	settings->m_fileLights = lightsFile;

	VRsceneExporter *exporter = new VRsceneExporter(settings);

	return PyLong_FromVoidPtr(exporter);
}


static PyObject* mExportExit(PyObject *self, PyObject *value)
{
	delete (VRsceneExporter*)PyLong_AsVoidPtr(value);

	Py_RETURN_NONE;
}


static PyObject* mExportScene(PyObject *self, PyObject *value)
{
	VRsceneExporter *exporter = (VRsceneExporter*)PyLong_AsVoidPtr(value);
	exporter->exportScene();

	Py_RETURN_NONE;
}


static PyObject* mExportSmokeDomain(PyObject *self, PyObject *args)
{
	int         contextPtr;
	int         objectPtr;
	int         smdPtr;
	const char *pluginName;
	const char *lights;
	PyObject   *fileObject;

	if(NOT(PyArg_ParseTuple(args, "iiissO", &contextPtr, &objectPtr, &smdPtr, &pluginName, &lights, &fileObject))) {
		return NULL;
	}

	bContext          *C   = (bContext*)(intptr_t)contextPtr;
	Object            *ob  = (Object*)(intptr_t)objectPtr;
	SmokeModifierData *smd = (SmokeModifierData*)(intptr_t)smdPtr;

	Scene *sce = CTX_data_scene(C);

	write_SmokeDomain(fileObject, sce, ob, smd, pluginName, lights);

	Py_RETURN_NONE;
}


static PyObject* mExportSmoke(PyObject *self, PyObject *args)
{
	int         contextPtr;
	int         objectPtr;
	int         smdPtr;
	const char *pluginName;
	PyObject   *fileObject;

	int         p_interpolation;

	if(NOT(PyArg_ParseTuple(args, "iiiisO", &contextPtr, &objectPtr, &smdPtr, &p_interpolation, &pluginName, &fileObject))) {
		return NULL;
	}

	bContext          *C   = (bContext*)(intptr_t)contextPtr;
	Object            *ob  = (Object*)(intptr_t)objectPtr;
	SmokeModifierData *smd = (SmokeModifierData*)(intptr_t)smdPtr;

	Scene *sce = CTX_data_scene(C);

	write_TexVoxelData(fileObject, sce, ob, smd, pluginName, p_interpolation);

	Py_RETURN_NONE;
}


static PyObject* mExportHair(PyObject *self, PyObject *args)
{
	int         contextPtr;
	int         objectPtr;
	int         psysPtr;
	const char *pluginName;
	PyObject   *fileObject;

	if(NOT(PyArg_ParseTuple(args, "iiisO", &contextPtr, &objectPtr, &psysPtr, &pluginName, &fileObject))) {
		return NULL;
	}

	bContext       *C    = (bContext*)(intptr_t)contextPtr;
	Object         *ob   = (Object*)(intptr_t)objectPtr;
	ParticleSystem *psys = (ParticleSystem*)(intptr_t)psysPtr;

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	if(write_GeomMayaHair(fileObject, sce, main, ob, psys, pluginName)) {
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyObject* mExportMesh(PyObject *self, PyObject *args)
{
	int         contextPtr;
	int         objectPtr;
	const char *pluginName;
	PyObject   *propGroup;
	PyObject   *fileObject;

	if(NOT(PyArg_ParseTuple(args, "iisOO", &contextPtr, &objectPtr, &pluginName, &propGroup, &fileObject))) {
		return NULL;
	}

	bContext *C = (bContext*)(intptr_t)contextPtr;
	Object   *ob = (Object*)(intptr_t)objectPtr;

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	write_Mesh(fileObject, sce, ob, main, pluginName, propGroup);

	Py_RETURN_NONE;
}


static PyObject* mExportNode(PyObject *self, PyObject *args)
{
	int       contextPtr;
	int       objectPtr;
	PyObject *nodeFile;
	PyObject *geomFile;

	char      pluginName[CGR_MAX_PLUGIN_NAME];

	if(NOT(PyArg_ParseTuple(args, "iiOO", &contextPtr, &objectPtr, &nodeFile, &geomFile))) {
		return NULL;
	}

	bContext *C = (bContext*)(intptr_t)contextPtr;
	Object *ob  = (Object*)(intptr_t)objectPtr;

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	sprintf(pluginName, "%s", ob->id.name);
	StripString(pluginName);

	write_ObjectNode(nodeFile, geomFile, sce, main, ob, ob->obmat, pluginName);

	Py_RETURN_NONE;
}


static PyObject* mExportDupli(PyObject *self, PyObject *args)
{
	int       contextPtr;
	int       objectPtr;
	PyObject *nodeFile;
	PyObject *geomFile;

	if(NOT(PyArg_ParseTuple(args, "iiOO", &contextPtr, &objectPtr, &nodeFile, &geomFile))) {
		return NULL;
	}

	bContext *C = (bContext*)(intptr_t)contextPtr;
	Object *ob  = (Object*)(intptr_t)objectPtr;

	Scene *sce  = CTX_data_scene(C);
	Main  *main = CTX_data_main(C);

	write_Dupli(nodeFile, geomFile, sce, main, ob);

	Py_RETURN_NONE;
}


static PyObject* mGetTransformHex(PyObject *self, PyObject *value)
{
	if (MatrixObject_Check(value)) {
		MatrixObject *transform = (MatrixObject*)value;

		float tm[4][4];
		char  tmBuf[TRANSFORM_HEX_SIZE]  = "";
		char  buf[TRANSFORM_HEX_SIZE+20] = "";

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


static PyMethodDef methods[] = {
	{"exportInit", (PyCFunction)mExportInit , METH_VARARGS|METH_KEYWORDS, "Init vrscene exporter"},

	{"exportScene",       mExportScene,       METH_O,                     "Export scene to the *.vrscene file"},
	{"exportExit",        mExportExit ,       METH_O,                     "Shutdown vrscene exporter"},

	{"exportDupli",       mExportDupli,       METH_VARARGS, "Export dupli / particles"},
	{"exportMesh",        mExportMesh,        METH_VARARGS, "Export mesh"},
	{"exportSmoke",       mExportSmoke,       METH_VARARGS, "Export voxel data"},
	{"exportSmokeDomain", mExportSmokeDomain, METH_VARARGS, "Export domain data"},
	{"exportHair",        mExportHair,        METH_VARARGS, "Export hair"},
	{"exportNode",        mExportNode,        METH_VARARGS, "Export Node description"},

	{"getTransformHex",   mGetTransformHex,   METH_O,       "Get transform hex string"},

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
