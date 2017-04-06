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
#include <queue>

#include "cgr_vray_for_blender_rt.h"
#include "vfb_scene_exporter_rt.h"
#include "vfb_scene_exporter_pro.h"
#include "vfb_params_json.h"

#include "vfb_plugin_exporter_zmq.h"

#include "zmq_wrapper.hpp"
#include "vfb_plugin_exporter_zmq.h"

#ifdef WITH_OSL
	// OSL
	#include <OSL/oslquery.h>
	#include <OSL/oslconfig.h>
	#include <OSL/oslcomp.h>
	#include <OSL/oslexec.h>

	// OIIO
	#include <errorhandler.h>
	#include <string_view.h>
#endif

VRayForBlender::ClientPtr heartbeatClient;
std::mutex heartbeatLock;

// TODO: possible data race when multiple exporters start at the same time
std::queue<VRayForBlender::SceneExporter*> stashedExporters;
namespace {
// stash instead of delete exporters here that are marked for undo
void stashExporter(VRayForBlender::SceneExporter* exporter)
{
	stashedExporters.push(exporter);
}

// try to take exporter from stash
VRayForBlender::SceneExporter * tryTakeStashedExporter()
{
	if (stashedExporters.empty()) {
		return nullptr;
	}

	auto exp = stashedExporters.front();
	stashedExporters.pop();
	return exp;
}
}

/// This is just like std::lock_guard<T> but it calls unlock first and then lock on destructor
/// It is needed to allow c++ code to unlock GIL before export and lock it back before returning to 
/// python code.
template <typename LockType>
struct ReverseRAIILock {
	ReverseRAIILock(LockType & lock)
		: lock(lock)
	{
		lock.unlock();
	}

	ReverseRAIILock(const ReverseRAIILock &) = delete;
	ReverseRAIILock & operator=(const ReverseRAIILock &) = delete;

	~ReverseRAIILock() {
		lock.lock();
	}

	LockType & lock;
};

static PyObject* vfb_zmq_heartbeat_start(PyObject*, PyObject *args)
{
	const char * conn_str = nullptr;

	if (PyArg_ParseTuple(args, "s", &conn_str)) {
		if (ZmqServer::start(conn_str)) {
			Py_RETURN_TRUE;
		}
	} else {
		PRINT_ERROR("Failed to get connection string");
	}

	Py_RETURN_FALSE;
}

static PyObject* vfb_zmq_heartbeat_stop(PyObject*)
{
	ZmqServer::stop();
	Py_RETURN_NONE;
}

static PyObject* vfb_zmq_heartbeat_check(PyObject *)
{
	if (ZmqServer::isRunning()) {
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

	Py_RETURN_NONE;
}


static PyObject* vfb_unload(PyObject*)
{
	PRINT_INFO_EX("vfb_unload()");

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

		auto stashed = tryTakeStashedExporter();

		if (stashed) {
			exporter = stashed;
			exporter->resume_from_undo(BL::Context(contextPtr), BL::RenderEngine(enginePtr), BL::BlendData(dataPtr), BL::Scene(scenePtr));
		} else {
			exporter = new VRayForBlender::InteractiveExporter(BL::Context(contextPtr), BL::RenderEngine(enginePtr), BL::BlendData(dataPtr), BL::Scene(scenePtr));
			exporter->init();
			exporter->init_data();
		}

		// python locks GIL before calling C++ so we unlock to enable UI event handling
		// and will lock when we need to change some python related data
		ReverseRAIILock<PythonGIL> lck(exporter->m_pyGIL);
		exporter->export_scene(stashed != nullptr);
		if (!stashed) {
			exporter->render_start();
		}
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
		if (exporter->is_engine_undo_taged() && exporter->is_viewport()) {
			stashExporter(exporter);
			exporter->pause_for_undo();
		} else {
			FreePtr(exporter);
		}
	}

	Py_RETURN_NONE;
}

/// Export data from blender for production rendering
static PyObject* vfb_update(PyObject*, PyObject *value)
{
	PRINT_INFO_EX("vfb_update()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		//ReverseRAIILock<PythonGIL> lck(exporter->m_pyGIL);
		//bool result = exporter->export_scene();
		//if (!result) {
		//	Py_RETURN_FALSE;
		//}
	}

	Py_RETURN_NONE;
}

/// Start production rendering
static PyObject* vfb_render(PyObject*, PyObject *value)
{
	PRINT_INFO_EX("vfb_render()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		ReverseRAIILock<PythonGIL> lck(exporter->m_pyGIL);
		exporter->export_scene();
	}

	Py_RETURN_NONE;
}

/// Export data for viewport rendering
static PyObject* vfb_view_update(PyObject*, PyObject *value)
{
	PRINT_INFO_EX("vfb_view_update()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		ReverseRAIILock<PythonGIL> lck(exporter->m_pyGIL);
		exporter->export_scene(true);
	}
	Py_RETURN_NONE;
}

/// Draw rendered image for viewport rendering
static PyObject* vfb_view_draw(PyObject*, PyObject *value)
{
	// PRINT_INFO_EX("vfb_view_draw()");

	VRayForBlender::SceneExporter *exporter = vfb_cast_exporter(value);
	if (exporter) {
		// dont allow python UI to run as this should be only image draw code
		exporter->draw();
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
											ExporterTypesList[i].key, ExporterTypesList[i].name, ExporterTypesList[i].desc);
		PyTuple_SET_ITEM(expTypesList, i, list_item);
	}

	return expTypesList;
}


static PyObject* vfb_osl_update_node_func(PyObject * /*self*/, PyObject *args)
{
	using namespace std;
	PyObject *pynodegroup, *pynode;
	const char *filepath = NULL;

	if (!PyArg_ParseTuple(args, "OOs", &pynodegroup, &pynode, &filepath)) {
		return NULL;
	}

	enum ErrorType {
		None = 0,
		Warning = 0,
		Error = 0,
	};

	auto makeErr = [](ErrorType type, const char * err) {
		return Py_BuildValue("(is)", static_cast<int>(type), err);
	};
	ErrorType returnErrType = Error;
	const char * returnErrStr = "OSL not supported";

#ifdef WITH_OSL
	OIIO_NAMESPACE_USING
	/* RNA */
	PointerRNA nodeptr;
	RNA_pointer_create((ID*)PyLong_AsVoidPtr(pynodegroup), &RNA_ShaderNodeScript, (void*)PyLong_AsVoidPtr(pynode), &nodeptr);
	BL::ShaderNodeScript b_node(nodeptr);

	OSL::OSLQuery query;
	if (!query.open(filepath, "")) {
		string errStr = "OSL query failed to open ";
		errStr += filepath;
		return makeErr(Error, errStr.c_str());
	}

	const bool isMtl = RNA_std_string_get(&nodeptr, "vray_type") == "MATERIAL";

	HashSet<void*> used_sockets;
	if (isMtl) {
		static const std::string ciName = "Ci";
		used_sockets.insert(b_node.outputs[ciName].ptr.data);
	} else {
		static const std::string texSockets[] = {"Color", "Transparency", "Alpha", "Intensity"};
		for (int c = 0; c < sizeof(texSockets) / sizeof(texSockets[0]); c++) {
			auto socket = b_node.outputs[texSockets[c]];
			BLI_assert(!!socket && "Missing texture socket on TexOSL");
			PRINT_ERROR("Missing socket \"%s\" on TexOSL", texSockets[c].c_str());
			used_sockets.insert(socket.ptr.data);
		}
	}

	static const std::string uvwGenSockName = "Uvwgen";
	used_sockets.insert(b_node.inputs[string(uvwGenSockName)].ptr.data);


	for(int i = 0; i < query.nparams(); i++) {
		const OSL::OSLQuery::Parameter *param = query.getparam(i);

		/* skip unsupported types */
		if (param->varlenarray || param->isstruct || param->type.arraylen > 1) {
			continue;
		}

		/* determine socket type */
		string socket_type;
		BL::NodeSocket::type_enum data_type = BL::NodeSocket::type_VALUE;
		float default_float4[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		float default_float = 0.0f;
		int default_int = 0;
		string default_string = "";

		if (param->isclosure && param->isoutput) {
			if (!isMtl) {
				returnErrType = Warning;
				returnErrStr = "Closure output is not supported for TexOSL";
				continue;
			} else {
				socket_type = "VRaySocketMtl";
				data_type = BL::NodeSocket::type_SHADER;
			}
		} else if (param->type.vecsemantics == TypeDesc::COLOR) {
			socket_type = "VRaySocketColor";
			data_type = BL::NodeSocket::type_RGBA;

			if (param->validdefault) {
				default_float4[0] = param->fdefault[0];
				default_float4[1] = param->fdefault[1];
				default_float4[2] = param->fdefault[2];
			}
		} else if (param->type.vecsemantics == TypeDesc::POINT ||
		        param->type.vecsemantics == TypeDesc::VECTOR ||
		        param->type.vecsemantics == TypeDesc::NORMAL) {

			socket_type = "VRaySocketVector";
			data_type = BL::NodeSocket::type_VECTOR;

			if (param->validdefault) {
				default_float4[0] = param->fdefault[0];
				default_float4[1] = param->fdefault[1];
				default_float4[2] = param->fdefault[2];
			}
		} else if (param->type.aggregate == TypeDesc::SCALAR) {
			if (param->type.basetype == TypeDesc::INT) {
				socket_type = "VRaySocketInt";
				data_type = BL::NodeSocket::type_INT;
				if (param->validdefault)
					default_int = param->idefault[0];
			} else if (param->type.basetype == TypeDesc::FLOAT) {
				socket_type = "VRaySocketFloat";
				data_type = BL::NodeSocket::type_VALUE;
				if (param->validdefault)
					default_float = param->fdefault[0];
			} else if (param->type.basetype == TypeDesc::STRING) {
				// TODO: strings are only for plugin inputs
				socket_type = "VRaySocketPlugin";
				data_type = BL::NodeSocket::type_STRING;
				if (param->validdefault) {
					default_string = param->sdefault[0];
				}
			} else {
				continue;
			}
		} else {
			continue;
		}

		/* find socket socket */
		BL::NodeSocket b_sock(PointerRNA_NULL);
		if (param->isoutput) {
			b_sock = b_node.outputs[param->name.string()];
			/* remove if type no longer matches */
			if (b_sock && b_sock.bl_idname() != socket_type) {
				b_node.outputs.remove(b_sock);
				b_sock = BL::NodeSocket(PointerRNA_NULL);
			}
		} else {
			b_sock = b_node.inputs[param->name.string()];
			/* remove if type no longer matches */
			if (b_sock && b_sock.bl_idname() != socket_type) {
				b_node.inputs.remove(b_sock);
				b_sock = BL::NodeSocket(PointerRNA_NULL);
			}
		}

		if (!b_sock) {
			/* create new socket */
			if (param->isoutput) {
				b_sock = b_node.outputs.create(socket_type.c_str(), param->name.c_str(), param->name.c_str());
			} else {
				b_sock = b_node.inputs.create(socket_type.c_str(), param->name.c_str(), param->name.c_str());
			}

			if(data_type == BL::NodeSocket::type_VALUE) {
				RNA_float_set(&b_sock.ptr, "value", default_float);
			} else if(data_type == BL::NodeSocket::type_INT) {
				RNA_int_set(&b_sock.ptr, "value", default_int);
			} else if(data_type == BL::NodeSocket::type_RGBA) {
				RNA_float_set_array(&b_sock.ptr, "value", default_float4);
			} else if(data_type == BL::NodeSocket::type_VECTOR) {
				RNA_float_set_array(&b_sock.ptr, "value", default_float4);
			} else if(data_type == BL::NodeSocket::type_STRING) {
				RNA_string_set(&b_sock.ptr, "value", default_string.c_str());
			}
		}

		used_sockets.insert(b_sock.ptr.data);
	}

	/* remove unused parameters */
	bool removed;

	do {
		BL::Node::inputs_iterator b_input;
		BL::Node::outputs_iterator b_output;

		removed = false;

		for (b_node.inputs.begin(b_input); b_input != b_node.inputs.end(); ++b_input) {
			if (used_sockets.find(b_input->ptr.data) == used_sockets.end()) {
				b_node.inputs.remove(*b_input);
				removed = true;
				break;
			}
		}

		for (b_node.outputs.begin(b_output); b_output != b_node.outputs.end(); ++b_output) {
			if (used_sockets.find(b_output->ptr.data) == used_sockets.end()) {
				b_node.outputs.remove(*b_output);
				removed = true;
				break;
			}
		}
	} while(removed);

#endif
	return makeErr(returnErrType, returnErrStr);
}

static PyObject* vfb_osl_compile_func(PyObject * /*self*/, PyObject *args)
{
#ifdef WITH_OSL
	OIIO_NAMESPACE_USING
	using namespace std;
	const char *inputfile = nullptr, *outputfile = nullptr, *stdoslfile = nullptr;

	if (!PyArg_ParseTuple(args, "sss", &inputfile, &outputfile, &stdoslfile)) {
		return nullptr;
	}

	vector<string> options;
	string stdosl_path = stdoslfile;

	options.push_back("-o");
	options.push_back(outputfile);

	/* compile */
	bool ok = false;
	{
		OSL::OSLCompiler compiler(&OSL::ErrorHandler::default_handler());
		ok = compiler.compile(string_view(inputfile), options, string_view(stdosl_path));
	}

	if (ok) {
		Py_RETURN_TRUE;
	}
	PRINT_WARN("OSL compilation failed using \"\" path for stdosl.h", stdoslfile);

#endif
	Py_RETURN_FALSE;
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

    {"osl_update_node", vfb_osl_update_node_func, METH_VARARGS, ""},
    {"osl_compile", vfb_osl_compile_func, METH_VARARGS, ""},

    {NULL, NULL, 0, NULL},
};


static struct PyModuleDef module = {
	PyModuleDef_HEAD_INIT,
	"_vray_for_blender_rt",
	"V-Ray For Blender Translator Module",
	-1,
	methods,
	NULL, NULL, NULL, NULL
};


void* VRayForBlenderRT_initPython()
{
	return (void*)PyModule_Create(&module);
}
