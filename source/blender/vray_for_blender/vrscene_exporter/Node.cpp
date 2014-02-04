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

#include "Node.h"

#include "CGR_json_plugins.h"
#include "CGR_rna.h"
#include "CGR_blender_data.h"
#include "CGR_string.h"
#include "CGR_vrscene.h"

#include "GeomStaticMesh.h"
#include "GeomMayaHair.h"
#include "GeomMeshFile.h"
#include "GeomPlane.h"

extern "C" {
#  include "DNA_modifier_types.h"
#  include "DNA_material_types.h"
#  include "BLI_math.h"
}

#include "BKE_depsgraph.h"
#include "BKE_scene.h"
#include "BKE_material.h"
#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h"
#include "BLI_string.h"
#include "BLI_path_util.h"

#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>


VRayScene::Node::Node(Scene *scene, Main *main, Object *ob):
	VRayExportable(scene, main, ob)
{
	geometry = NULL;
}


void VRayScene::Node::init(DupliObject *dOb)
{
	dupliObject = dOb;
	object      = dupliObject ? dupliObject->ob : m_ob;

	initGeometry();
	if(NOT(geometry))
		return;

	initTransform();
	initProperties();

	initName();
	initHash();
}


void VRayScene::Node::freeData()
{
}


char* VRayScene::Node::getTransform() const
{
	return const_cast<char*>(transform);
}


int VRayScene::Node::getObjectID() const
{
	return objectID;
}


void VRayScene::Node::initName(const std::string &name)
{
	if(NOT(name.empty())) {
		m_name = name;
	}
	else {
		m_name = GetIDName((ID*)object);
		if(dupliObject)
			m_name.append(boost::lexical_cast<std::string>(dupliObject->persistent_id[0]));
	}
}


void VRayScene::Node::initGeometry()
{
	RnaAccess::RnaValue rna((ID*)object->data, "vray");

	if(NOT(rna.getBool("override"))) {
		GeomStaticMesh *geomStaticMesh = new GeomStaticMesh(m_sce, m_main, object);
		geomStaticMesh->init();
		if(NOT(geomStaticMesh->getHash()))
			delete geomStaticMesh;
		else
			geometry = geomStaticMesh;
	}
	else {
		if (rna.getEnum("override_type") == 0) {
			GeomMeshFile *geomMeshFile = new GeomMeshFile(m_sce, m_main, object);
			geomMeshFile->init();
			geometry = geomMeshFile;
		}
		else if(rna.getEnum("override_type") == 1) {
			GeomPlane *geomPlane = new GeomPlane();
			geomPlane->init();
			geometry = geomPlane;
		}
	}
}


void VRayScene::Node::initTransform()
{
	float tm[4][4];

	if(dupliObject)
		copy_m4_m4(tm, dupliObject->mat);
	else
		copy_m4_m4(tm, object->obmat);

	GetTransformHex(tm, transform);
}


void VRayScene::Node::initProperties()
{
	objectID = object->index;
}


void VRayScene::Node::initHash()
{
	// TODO: Add visibility to hash
	m_hash = HashCode(transform);
}


std::string VRayScene::Node::writeMtlMulti(PyObject *output)
{
	if(NOT(object->totcol))
		return "MANOMATERIALISSET";

	StringVector mtls_list;
	StringVector ids_list;

	for(int a = 1; a <= object->totcol; ++a) {
		Material *ma = give_current_material(object, a);
		if(NOT(ma))
			continue;

		// TODO: Material override

		std::string materialName = GetIDName((ID*)ma);

		mtls_list.push_back(materialName);
		ids_list.push_back(boost::lexical_cast<std::string>(a));
	}

	// No need for multi-material if only one slot
	// is used
	//
	if(mtls_list.size() == 1)
		return mtls_list[0];

	char obMtlName[MAX_ID_NAME];
	BLI_strncpy(obMtlName, object->id.name+2, MAX_ID_NAME);
	StripString(obMtlName);

	std::string plugName("MM");
	plugName.append(obMtlName);

	PYTHON_PRINTF(output, "\nMtlMulti %s {", plugName.c_str());
	PYTHON_PRINTF(output, "\n\tmtls_list=List(%s);",   boost::algorithm::join(mtls_list, ",").c_str());
	PYTHON_PRINTF(output, "\n\tids_list=ListInt(%s);", boost::algorithm::join(ids_list, ",").c_str());
	PYTHON_PRINT (output, "\n}\n");

	return plugName;
}


std::string VRayScene::Node::writeMtlWrapper(PyObject *output, const std::string &baseMtl)
{
	RnaAccess::RnaValue rna(&object->id, "vray.MtlWrapper");
	if(NOT(rna.getBool("use")))
		return baseMtl;

	std::string pluginName = "MtlWrapper" + baseMtl;

	std::stringstream ss;
	ss << "\n" << "MtlWrapper" << " " << pluginName << " {";
	ss << "\n\t" << "base_material=" << baseMtl << ";";
	rna.writePlugin(m_pluginDesc.getTree("MtlWrapper"), ss);
	ss << "\n}\n";

	PYTHON_PRINT(output, ss.str().c_str());

	return pluginName;
}


std::string VRayScene::Node::writeMtlOverride(PyObject *output, const std::string &baseMtl)
{
	RnaAccess::RnaValue rna(&object->id, "vray.MtlOverride");
	if(NOT(rna.getBool("use")))
		return baseMtl;

	std::string pluginName = "MtlOverride" + baseMtl;

	std::stringstream ss;
	ss << "\n" << "MtlOverride" << " " << pluginName << " {";
	ss << "\n\t" << "base_mtl=" << baseMtl << ";";
	rna.writePlugin(m_pluginDesc.getTree("MtlOverride"), ss);
	ss << "\n}\n";

	PYTHON_PRINT(output, ss.str().c_str());

	return pluginName;
}


std::string VRayScene::Node::writeMtlRenderStats(PyObject *output, const std::string &baseMtl)
{
	RnaAccess::RnaValue rna(&object->id, "vray.MtlRenderStats");
	if(NOT(rna.getBool("use")))
		return baseMtl;

	std::string pluginName = "MtlRenderStats" + baseMtl;

	std::stringstream ss;
	ss << "\n" << "MtlRenderStats" << " " << pluginName << " {";
	ss << "\n\t" << "base_mtl=" << baseMtl << ";";
	rna.writePlugin(m_pluginDesc.getTree("MtlRenderStats"), ss);
	ss << "\n}\n";

	PYTHON_PRINT(output, ss.str().c_str());

	return pluginName;
}


void VRayScene::Node::writeData(PyObject *output)
{
	std::string material = writeMtlMulti(output);
	material = writeMtlOverride(output, material);
	material = writeMtlWrapper(output, material);
	material = writeMtlRenderStats(output, material);

	PYTHON_PRINTF(output, "\nNode %s {", this->getName());
	PYTHON_PRINTF(output, "\n\tobjectID=%i;", this->getObjectID());
	PYTHON_PRINTF(output, "\n\tgeometry=%s;", this->getDataName());
	PYTHON_PRINTF(output, "\n\tmaterial=%s;", material.c_str());
	PYTHON_PRINTF(output, "\n\ttransform=%sTransformHex(\"%s\")%s;", m_interpStart, this->getTransform(), m_interpEnd);
	PYTHON_PRINT (output, "\n}\n");
}


int VRayScene::Node::isAnimated() {
	return IsNodeAnimated(m_ob);
}


void VRayScene::Node::writeGeometry(PyObject *output, int frame)
{
	geometry->write(output, frame);
}
