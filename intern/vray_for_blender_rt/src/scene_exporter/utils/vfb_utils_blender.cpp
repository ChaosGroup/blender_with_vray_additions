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

#include "vfb_utils_blender.h"
#include "vfb_utils_string.h"
#include "vfb_utils_math.h"

#include "DNA_ID.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BLI_string.h"
#include "BLI_path_util.h"

#include "cgr_config.h"
#include <boost/filesystem.hpp>

using namespace VRayForBlender;

#ifdef WITH_OSL
// OSL
#include <OSL/oslconfig.h>
#include <OSL/oslcomp.h>
#include <OSL/oslexec.h>

// OIIO
#include <errorhandler.h>
#include <string_view.h>

bool Blender::OSLManager::compile(const std::string & inputFile, const std::string & outputFile)
{
	OIIO_NAMESPACE_USING
	std::vector<std::string> options;
	std::string stdosl_path = stdOSLPath;

	options.push_back("-o");
	options.push_back(outputFile);

	OSL::OSLCompiler compiler(&OSL::ErrorHandler::default_handler());
	return compiler.compile(string_view(inputFile), options, string_view(stdosl_path));
}

std::string Blender::OSLManager::compileToBuffer(const std::string & code)
{
	OIIO_NAMESPACE_USING
	std::vector<std::string> options;
	std::string stdosl_path = stdOSLPath;

	OSL::OSLCompiler compiler(&OSL::ErrorHandler::default_handler());
	std::string buffer;
	compiler.compile_buffer(code, buffer, {}, stdOSLPath);
	return buffer;
}

bool Blender::OSLManager::queryFromFile(const std::string & file, OSL::OSLQuery & query)
{
	return query.open(file, "");
}

bool Blender::OSLManager::queryFromBytecode(const std::string & code, OSL::OSLQuery & query)
{
	return query.open_bytecode(code);
}

bool Blender::OSLManager::queryFromNode(BL::Node node, OSL::OSLQuery & query, const std::string & basepath, bool writeToFile, std::string * output)
{
	bool success = true;
	std::string scriptPath;
	bool compileFromFile = true;
	enum OSLNodeMode { INTERNAL = 0, EXTERNAL = 1 };
	if (RNA_enum_get(&node.ptr, "mode") == EXTERNAL) {
		scriptPath = RNA_std_string_get(&node.ptr, "filepath");
		scriptPath = String::AbsFilePath(scriptPath, basepath);

		boost::filesystem::path osoPath = scriptPath;
		osoPath.replace_extension(".oso");
		std::string strOsoPath = osoPath.string();
		if (!compile(scriptPath, strOsoPath)) {
			PRINT_ERROR("Failed to compile OSL file: \"%s\"", scriptPath.c_str());
			success = false;
		} else {
			if (!queryFromFile(strOsoPath, query)) {
				PRINT_ERROR("Failed to query compiled OSO file: \"%s\"", output->c_str());
				success = false;
			}
		}

		if (!writeToFile && success) {
			std::remove(strOsoPath.c_str());
		}
	} else {
		BL::Text text(RNA_pointer_get(&node.ptr, "script"));
		if (text) {
			if (text.is_dirty() || text.is_in_memory() || text.is_modified()) {
				// osl code is in memmory - compile and write if needed
				std::string oslCode;
				oslCode.reserve(text.lines.length() * 50); // 50 average chars per line seems reasonable
				for (auto & line : Blender::collection(text.lines)) {
					oslCode += line.body();
					oslCode += '\n';
				}
				std::string bytecode = compileToBuffer(oslCode);

				if (bytecode.empty() || !queryFromBytecode(bytecode, query)) {
					PRINT_ERROR("Failed query for osl node: \"%s\"", node.name().c_str());
					success = false;
				} else if (writeToFile && output) {
					boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("%%%%-%%%%-%%%%-%%%%.osl");
					*output = tempPath.string();
					std::ofstream tmpFile(output->c_str(), std::ios::trunc | std::ios::binary);
					if (tmpFile && tmpFile.write(oslCode.c_str(), oslCode.length())) {
						compileFromFile = false;
					} else {
						PRINT_ERROR("Failed to write OSL script to temp file \"%s\"", output->c_str());
					}
				}
			} else {
				scriptPath = text.filepath();
			}
		} else {
			PRINT_ERROR("Invalid script selected for osl node \"%s\"", node.name().c_str());
			success = false;
		}
	}

	if (writeToFile && output && !scriptPath.empty()) {
		*output = scriptPath;
	}

	return success;
}
#endif

std::string Blender::GetIDName(BL::ID id, const std::string &)
{
#if 1
	std::string id_name = id.name();
#else
	char baseName[MAX_ID_NAME];
	if (prefix.empty()) {
		BLI_strncpy(baseName, id->name, MAX_ID_NAME);
	}
	else {
		// NOTE: Skip internal prefix
		BLI_strncpy(baseName, id->name+2, MAX_ID_NAME);
	}
	StripString(baseName);

	std::string id_name = prefix + baseName;

	if(id->lib) {
		char libFilename[FILE_MAX] = "";

		BLI_split_file_part(id->lib->name+2, libFilename, FILE_MAX);
		BLI_replace_extension(libFilename, FILE_MAX, "");

		StripString(libFilename);

		id_name.append("LI");
		id_name.append(libFilename);
	}
#endif
	return id_name;
}


std::string VRayForBlender::Blender::GetIDNameAuto(BL::ID id)
{
	return VRayForBlender::Blender::GetIDName(id);
}


BL::Object Blender::GetObjectByName(BL::BlendData data, const std::string &name)
{
	BL::Object object(PointerRNA_NULL);

	if (!name.empty()) {
		BL::BlendData::objects_iterator obIt;
		for (data.objects.begin(obIt); obIt != data.objects.end(); ++obIt) {
			BL::Object ob = *obIt;
			if (ob.name() == name) {
				 object = ob;
				 break;
			}
		}
	}

	return object;
}


BL::Material Blender::GetMaterialByName(BL::BlendData data, const std::string &name)
{
	BL::Material material(PointerRNA_NULL);

	if (!name.empty()) {
		BL::BlendData::materials_iterator maIt;
		for (data.materials.begin(maIt); maIt != data.materials.end(); ++maIt) {
			BL::Material ma = *maIt;
			if (ma.name() == name) {
				 material = ma;
				 break;
			}
		}
	}

	return material;
}


int Blender::GetMaterialCount(BL::Object ob)
{
	int material_count = 0;

	BL::Object::material_slots_iterator slotIt;
	for (ob.material_slots.begin(slotIt); slotIt != ob.material_slots.end(); ++slotIt) {
		BL::Material ma((*slotIt).material());
		if (ma) {
			material_count++;
		}
	}

	return material_count;
}


std::string Blender::GetFilepath(const std::string &filepath, ID *holder)
{
#define ID_BLEND_PATH_EX(b_id) (b_id ? (ID_BLEND_PATH(G.main, b_id)) : G.main->name)

	char absFilepath[FILE_MAX];
	BLI_strncpy(absFilepath, filepath.c_str(), FILE_MAX);

	BLI_path_abs(absFilepath, ID_BLEND_PATH_EX(holder));

	// Convert UNC filepath "\\" to "/" on *nix
	// User then have to mount share "\\MyShare" to "/MyShare"
#ifndef _WIN32
	if (absFilepath[0] == '\\' && absFilepath[1] == '\\') {
		absFilepath[1] = '/';
		return absFilepath+1;
	}
#endif

	return absFilepath;
}


float Blender::GetDistanceObOb(BL::Object a, BL::Object b)
{
	return Math::GetDistanceTmTm(a.matrix_world(), b.matrix_world());
}


float Blender::GetCameraDofDistance(BL::Object camera)
{
	BL::Camera camera_data(camera.data());
	BL::Object dofObject(camera_data.dof_object());

	float dofDistance = dofObject
	                    ? GetDistanceObOb(camera, dofObject)
	                    : camera_data.dof_distance();

	return dofDistance;
}


int Blender::IsHairEmitter(BL::Object ob)
{
	int has_hair = false;

	BL::Object::modifiers_iterator modIt;
	for (ob.modifiers.begin(modIt); modIt != ob.modifiers.end(); ++modIt) {
		BL::Modifier mod(*modIt);
		if (mod.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
			BL::ParticleSystemModifier pmod(mod);
			BL::ParticleSystem psys(pmod.particle_system());
			if (psys) {
				BL::ParticleSettings pset(psys.settings());
				if (pset &&
				    pset.type()        == BL::ParticleSettings::type_HAIR &&
				    pset.render_type() == BL::ParticleSettings::render_type_PATH) {
					has_hair = true;
					break;
				}
			}
		}
	}

	return has_hair;
}


int Blender::IsEmitterRenderable(BL::Object ob)
{
	int render_emitter = true;

	for (auto & ps : Blender::collection(ob.particle_systems)) {
		BL::ParticleSettings pset(ps.settings());
		if (!pset.use_render_emitter()) {
			render_emitter = false;
			break;
		}
	}

	return render_emitter;
}


int Blender::IsDuplicatorRenderable(BL::Object ob)
{
	bool is_renderable = false;

	if (!ob.is_duplicator()) {
		is_renderable = true;
	}
	else {
		if (ob.particle_systems.length()) {
			is_renderable = IsEmitterRenderable(ob);
		}
		else if (ob.dupli_type() == BL::Object::dupli_type_NONE ||
		         ob.dupli_type() == BL::Object::dupli_type_FRAMES) {
			is_renderable = true;
		}
	}

	return is_renderable;
}


int Blender::IsGeometry(BL::Object ob)
{
	int is_geometry = false;
	if (ob.type() == BL::Object::type_MESH    ||
	    ob.type() == BL::Object::type_CURVE   ||
	    ob.type() == BL::Object::type_SURFACE ||
	    ob.type() == BL::Object::type_FONT    ||
	    ob.type() == BL::Object::type_META) {
		is_geometry = true;
	}
	return is_geometry;
}


int Blender::IsLight(BL::Object ob)
{
	int is_light = false;
	if (ob.type() == BL::Object::type_LAMP) {
		is_light = true;
	}
	return is_light;
}
