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

#include "cgr_vray_for_blender_rt.h"
#include "vfb_scene_exporter_rt.h"
#include "vfb_scene_exporter_pro.h"
#include "vfb_params_json.h"

#include "vfb_plugin_exporter_zmq.h"

#include "zmq_wrapper.h"
ZmqWrapper * heartbeatClient = nullptr;
std::mutex heartbeatLock;

static PyObject* vfb_zmq_heartbeat_start(PyObject*, PyObject *args)
{
	const char * conn_str = nullptr;

	if (PyArg_ParseTuple(args, "s", &conn_str)) {
		std::lock_guard<std::mutex> l(heartbeatLock);
		if (!heartbeatClient) {
			PRINT_INFO_EX("Starting hearbeat client for %s", conn_str);
			heartbeatClient = new ZmqWrapper(true);
			heartbeatClient->connect(conn_str);
			if (heartbeatClient->connected()) {
				Py_RETURN_TRUE;
			}
		} else {
			PRINT_ERROR("Heartbeat client already running...");
			// return true as we are running good.
			if (heartbeatClient->good() && heartbeatClient->connected()) {
				Py_RETURN_TRUE;
			}
		}
	} else {
		PRINT_ERROR("Failed to get connection string");
	}
	Py_RETURN_FALSE;
}

static PyObject* vfb_zmq_heartbeat_stop(PyObject*)
{
	std::lock_guard<std::mutex> l(heartbeatLock);
	if (heartbeatClient) {
		PRINT_INFO_EX("Stopping hearbeat client");
		delete heartbeatClient;
		heartbeatClient = nullptr;
	} else {
		PRINT_ERROR("No zmq heartbeat client running...");
	}

	Py_RETURN_NONE;
}

static PyObject* vfb_zmq_heartbeat_check(PyObject *)
{
	std::lock_guard<std::mutex> l(heartbeatLock);
	if (heartbeatClient && heartbeatClient->good() && heartbeatClient->connected()) {
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
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

	char *jsonDirpath = NULL;
	if (!PyArg_ParseTuple(args, "s", &jsonDirpath)) {
		PRINT_ERROR("PyArg_ParseTuple");
	}
	else {
		try {
			VRayForBlender::InitPluginDescriptions(jsonDirpath);
		} catch (std::exception &ex) {
			PRINT_ERROR("Exception: %s", ex.what());
		}
	}

	auto & zmqPool = ZmqWorkerPool::getInstance();

	Py_RETURN_NONE;
}


static PyObject* vfb_unload(PyObject*)
{
	PRINT_INFO_EX("vfb_unload()");

	ZmqWorkerPool::getInstance().shutdown();
	vfb_zmq_heartbeat_stop(nullptr);

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

	PyObject *mainFile = nullptr;
	PyObject *objectFile = nullptr;
	PyObject *envFile = nullptr;
	PyObject *geometryFile = nullptr;
	PyObject *lightsFile = nullptr;
	PyObject *materialFile = nullptr;
	PyObject *textureFile = nullptr;
	PyObject *cameraFile = nullptr;

	static char *kwlist[] = {
	    /* 0 */ _C("context"),
	    /* 1 */ _C("engine"),
	    /* 2 */ _C("data"),
	    /* 3 */ _C("scene"),
	    /* 4 */ _C("mainFile"),
	    /* 5 */ _C("objectFile"),
	    /* 6 */ _C("envFile"),
	    /* 7 */ _C("geometryFile"),
	    /* 8 */ _C("lightsFile"),
	    /* 9 */ _C("materialFile"),
	    /* 10 */_C("textureFile"),
	    /* 11 */_C("cameraFile"),
	    NULL
	};

	//                                 0123 456789111
	//                                            012
	static const char kwlistTypes[] = "OOOO|OOOOOOOO";

	if (PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
	                                /* 0 */ &pyContext,
	                                /* 1 */ &pyEngine,
	                                /* 2 */ &pyData,
	                                /* 3 */ &pyScene,
	                                /* 4 */ &mainFile,
	                                /* 5 */ &objectFile,
	                                /* 6 */ &envFile,
	                                /* 7 */ &geometryFile,
	                                /* 8 */ &lightsFile,
	                                /* 9 */ &materialFile,
	                                /* 10 */&textureFile,
	                                /* 11 */&cameraFile))
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
		auto pluginExporter = exporter->get_plugin_exporter();
		if (pluginExporter) {
			using VRayForBlender::ParamDesc::PluginType;
			pluginExporter->set_export_file(PluginType::PluginSettings, mainFile);
			pluginExporter->set_export_file(PluginType::PluginObject, objectFile);
			pluginExporter->set_export_file(PluginType::PluginEffect, envFile);
			pluginExporter->set_export_file(PluginType::PluginGeometry, geometryFile);
			pluginExporter->set_export_file(PluginType::PluginLight, lightsFile);
			pluginExporter->set_export_file(PluginType::PluginMaterial, materialFile);
			pluginExporter->set_export_file(PluginType::PluginTexture, textureFile);
			pluginExporter->set_export_file(PluginType::PluginCamera, cameraFile);
		}
		exporter->init_data();
	} else {
		PRINT_ERROR("Failed to initialize exporter!");
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
		exporter->init_data();

		exporter->python_thread_state_save();
		exporter->sync(false);
		exporter->render_start();
		exporter->python_thread_state_restore();
	} else {
		PRINT_ERROR("Failed to initialize RT exporter!");
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
		exporter->python_thread_state_save();
		bool result = exporter->do_export();
		exporter->python_thread_state_restore();
		if (!result) {
			Py_RETURN_FALSE;
		}
	}

	Py_RETURN_NONE;
}


static PyObject* vfb_view_update(PyObject*, PyObject *value)
{
	PRINT_INFO_EX("vfb_view_update()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		exporter->python_thread_state_save();
		exporter->sync(true);
		exporter->python_thread_state_restore();
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
		exporter->python_thread_state_save();
		exporter->render_start();
		exporter->python_thread_state_restore();
	}

	Py_RETURN_NONE;
}


static PyObject* vfb_get_exporter_types(PyObject*, PyObject*)
{
	PRINT_INFO_EX("vfb_get_exporter_types()");

	PyObject *expTypesList = PyTuple_New(ExpoterTypeLast);

	for (int i = 0; i < ExpoterTypeLast; ++i) {
		static const char *item_format = "(sss)";

		PyObject *list_item = Py_BuildValue(item_format,
											ExporterTypes[i].key, ExporterTypes[i].name, ExporterTypes[i].desc);
		PyTuple_SET_ITEM(expTypesList, i, list_item);
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

	{ "zmq_heartbeat_start",              vfb_zmq_heartbeat_start, METH_VARARGS, ""},
	{ "zmq_heartbeat_stop",  (PyCFunction)vfb_zmq_heartbeat_stop,  METH_NOARGS,  ""},
	{ "zmq_heartbeat_check", (PyCFunction)vfb_zmq_heartbeat_check, METH_NOARGS,  ""},

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
