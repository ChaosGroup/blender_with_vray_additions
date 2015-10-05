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

#include "cgr_config.h"

#include <Python.h>

#ifdef USE_BLENDER_VRAY_APPSDK
#include <vraysdk.hpp>
#endif

#include "cgr_vray_for_blender_rt.h"
#include "vfb_scene_exporter_rt.h"
#include "vfb_scene_exporter_pro.h"
#include "vfb_params_json.h"

#ifdef USE_BLENDER_VRAY_APPSDK
static VRay::VRayInit *VRayInit = nullptr;
#endif


static void python_thread_state_save(void **python_thread_state)
{
	*python_thread_state = (void*)PyEval_SaveThread();
}


static void python_thread_state_restore(void **python_thread_state)
{
	PyEval_RestoreThread((PyThreadState*)*python_thread_state);
	*python_thread_state = nullptr;
}


static VRayForBlender::SceneExporter *vfb_cast_exporter(PyObject *value)
{
	VRayForBlender::SceneExporter *exporter = nullptr;

	if (value != Py_None) {
		exporter = reinterpret_cast<VRayForBlender::SceneExporter*>(PyLong_AsVoidPtr(value));
	}

	return exporter;
}


static PyObject* vfb_load(PyObject*, PyObject *args)
{
	PRINT_INFO_EX("vfb_load()");

#ifdef USE_BLENDER_VRAY_APPSDK
	if (!VRayInit) {
		try {
			VRayInit = new VRay::VRayInit(false);
		}
		catch (std::exception &e) {
			PRINT_INFO_EX("Error initing V-Ray! Error: \"%s\"",
			              e.what());
			VRayInit = nullptr;
		}
	}
#endif

	char *jsonDirpath = NULL;
	if (!PyArg_ParseTuple(args, "s", &jsonDirpath)) {
		PRINT_ERROR("PyArg_ParseTuple");
	}
	else {
		VRayForBlender::InitPluginDescriptions(jsonDirpath);
	}

	Py_RETURN_NONE;
}


static PyObject* vfb_unload(PyObject*)
{
	PRINT_INFO_EX("vfb_unload()");

#ifdef USE_BLENDER_VRAY_APPSDK
	FreePtr(VRayInit);
#endif

	Py_RETURN_NONE;
}


static PyObject* vfb_init(PyObject*, PyObject *args, PyObject *keywds)
{
	PRINT_INFO_EX("vfb_init()");

	VRayForBlender::SceneExporter *exporter = nullptr;

	PyObject *pyContext = nullptr;
	PyObject *pyEngine = nullptr;
	PyObject *pyData = nullptr;
	PyObject *pyScene = nullptr;

	static char *kwlist[] = {
	    /* 0 */_C("context"),
	    /* 1 */_C("engine"),
	    /* 2 */_C("data"),
	    /* 3 */_C("scene"),
	    NULL
	};

	//                                 012345678911
	//                                           01
	static const char kwlistTypes[] = "OOOO";

	if (PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
	                                /* 0 */ &pyContext,
	                                /* 1 */ &pyEngine,
	                                /* 2 */ &pyData,
	                                /* 3 */ &pyScene))
	{
		PointerRNA contextPtr;
		PointerRNA enginePtr;
		PointerRNA dataPtr;
		PointerRNA scenePtr;

		RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyContext), &contextPtr);
		RNA_pointer_create(NULL, &RNA_RenderEngine, (void*)PyLong_AsVoidPtr(pyEngine), &enginePtr);
		RNA_main_pointer_create((Main*)PyLong_AsVoidPtr(pyData), &dataPtr);
		RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyScene), &scenePtr);

		exporter = new VRayForBlender::ProductionExporter(BL::Context(contextPtr), BL::RenderEngine(enginePtr), BL::BlendData(dataPtr), BL::Scene(scenePtr));
		exporter->init();
	}

	return PyLong_FromVoidPtr(exporter);
}


static PyObject* vfb_init_rt(PyObject*, PyObject *args, PyObject *keywds)
{
	PRINT_INFO_EX("vfb_init_rt()");

	VRayForBlender::SceneExporter *exporter = nullptr;

	PyObject *pyContext = nullptr;
	PyObject *pyEngine = nullptr;
	PyObject *pyData = nullptr;
	PyObject *pyScene = nullptr;

	static char *kwlist[] = {
	    /* 0 */_C("context"),
	    /* 1 */_C("engine"),
	    /* 2 */_C("data"),
	    /* 3 */_C("scene"),
	    NULL
	};

	//                                 012345678911
	//                                           01
	static const char kwlistTypes[] = "OOOO";

	if (PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
	                                /* 0 */ &pyContext,
	                                /* 1 */ &pyEngine,
	                                /* 2 */ &pyData,
	                                /* 3 */ &pyScene))
	{
		PointerRNA contextPtr;
		PointerRNA enginePtr;
		PointerRNA dataPtr;
		PointerRNA scenePtr;

		RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyContext), &contextPtr);
		RNA_pointer_create(NULL, &RNA_RenderEngine, (void*)PyLong_AsVoidPtr(pyEngine), &enginePtr);
		RNA_main_pointer_create((Main*)PyLong_AsVoidPtr(pyData), &dataPtr);
		RNA_id_pointer_create((ID*)PyLong_AsVoidPtr(pyScene), &scenePtr);

		exporter = new VRayForBlender::InteractiveExporter(BL::Context(contextPtr), BL::RenderEngine(enginePtr), BL::BlendData(dataPtr), BL::Scene(scenePtr));
		exporter->init();
		exporter->do_export();
	}

	return PyLong_FromVoidPtr(exporter);
}



static PyObject* vfb_free(PyObject*, PyObject *value)
{
	PRINT_INFO_EX("vfb_free()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		FreePtr(exporter);
	}

	Py_RETURN_NONE;
}


static PyObject* vfb_update(PyObject*, PyObject *value)
{
	PRINT_INFO_EX("vfb_update()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		python_thread_state_save(&exporter->m_pythonThreadState);
		exporter->do_export();
		python_thread_state_restore(&exporter->m_pythonThreadState);
	}

	Py_RETURN_NONE;
}


static PyObject* vfb_view_update(PyObject*, PyObject *value)
{
	PRINT_INFO_EX("vfb_view_update()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		python_thread_state_save(&exporter->m_pythonThreadState);
		exporter->sync(true);
		python_thread_state_restore(&exporter->m_pythonThreadState);
	}

	Py_RETURN_NONE;
}


static PyObject* vfb_view_draw(PyObject*, PyObject *value)
{
	// PRINT_INFO_EX("vfb_view_draw()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		exporter->draw();
	}

	Py_RETURN_NONE;
}


static PyObject* vfb_render(PyObject*, PyObject *value)
{
	PRINT_INFO_EX("vfb_render()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		python_thread_state_save(&exporter->m_pythonThreadState);
		exporter->render_start();
		python_thread_state_restore(&exporter->m_pythonThreadState);
	}

	Py_RETURN_NONE;
}


static PyObject* vfb_get_exporter_types(PyObject*, PyObject*)
{
	PRINT_INFO_EX("vfb_get_exporter_types()");

	static PyObject *expTypesList = NULL;

	if (!expTypesList) {
		expTypesList = PyTuple_New(ExpoterTypeLast);

		for (int i = 0; i < ExpoterTypeLast; ++i) {
			static const char *item_format = "(sss)";

			PyObject *list_item = Py_BuildValue(item_format,
												ExporterTypes[i].key, ExporterTypes[i].name, ExporterTypes[i].desc);
			PyTuple_SET_ITEM(expTypesList, i, list_item);
		}
	}

	return expTypesList;
}


static PyMethodDef methods[] = {
    { "load",                vfb_load,   METH_VARARGS, ""},
    { "unload", (PyCFunction)vfb_unload, METH_NOARGS,  ""},

    { "init",    (PyCFunction)vfb_init,    METH_VARARGS|METH_KEYWORDS, ""},
    { "init_rt", (PyCFunction)vfb_init_rt, METH_VARARGS|METH_KEYWORDS, ""},
    { "free",                 vfb_free,    METH_O, ""},

    { "update",      vfb_update,      METH_O, "" },
    { "render",      vfb_render,      METH_O, "" },
    { "view_update", vfb_view_update, METH_O, "" },
    { "view_draw",   vfb_view_draw,   METH_O, "" },

    { "getExporterTypes", vfb_get_exporter_types, METH_NOARGS, "" },

    {NULL, NULL, 0, NULL},
};


static struct PyModuleDef module = {
	PyModuleDef_HEAD_INIT,
	"_vray_for_blender",
	"V-Ray For Blender Translator Module",
	-1,
	methods,
	NULL, NULL, NULL, NULL
};


void* VRayForBlenderRT_initPython()
{
	return (void*)PyModule_Create(&module);
}
