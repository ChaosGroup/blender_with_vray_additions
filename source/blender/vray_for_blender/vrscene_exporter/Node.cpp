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

#include "BKE_depsgraph.h"
#include "BKE_scene.h"
#include "BKE_material.h"
#include "BKE_particle.h"
#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_math_matrix.h"

extern "C" {
#  include "DNA_modifier_types.h"
#  include "DNA_material_types.h"
}

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>


VRayScene::Node::Node(Scene *scene, Main *main, Object *ob, DupliObject *dOb):
	VRayExportable(scene, main, ob)
{
	m_geometry = NULL;
	m_geometryType = VRayScene::eGeometryMesh;

	m_dupliObject = dOb;
	m_object      = m_dupliObject ? m_dupliObject->ob : m_ob;
}


void VRayScene::Node::init(const std::string &mtlOverrideName)
{
	m_materialOverride = mtlOverrideName;

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
	return const_cast<char*>(m_transform);
}


int VRayScene::Node::getObjectID() const
{
	return m_objectID;
}


void VRayScene::Node::initName(const std::string &name)
{
	if(NOT(name.empty())) {
		m_name = name;
	}
	else {
		m_name.clear();

		// If base object is a duplicator also add it's name
		if(m_ob->transflag & OB_DUPLI)
			m_name = GetIDName((ID*)m_ob);

		m_name.append(GetIDName((ID*)m_object));

		// Add unique dupli index
		if(m_dupliObject)
			m_name.append(boost::lexical_cast<std::string>(m_dupliObject->persistent_id[0]));
	}
}


int VRayScene::Node::preInitGeometry()
{
	RnaAccess::RnaValue rna((ID*)m_object->data, "vray");

	if(NOT(rna.getBool("override")))
		m_geometryType = VRayScene::eGeometryMesh;
	else
		if (rna.getEnum("override_type") == 0)
			m_geometryType = VRayScene::eGeometryProxy;
		else if(rna.getEnum("override_type") == 1)
			m_geometryType = VRayScene::eGeometryPlane;

	if(m_geometryType == VRayScene::eGeometryMesh)
		return IsMeshValid(m_object);
	else
		return 1;
}


void VRayScene::Node::initGeometry()
{
	if(m_geometryType == VRayScene::eGeometryMesh)
		m_geometry = new GeomStaticMesh(m_sce, m_main, m_object, true);
	else if(m_geometryType == VRayScene::eGeometryProxy)
		m_geometry = new GeomMeshFile(m_sce, m_main, m_object);
	else if(m_geometryType == VRayScene::eGeometryPlane)
		m_geometry = new GeomPlane(m_sce, m_main, m_object);
	m_geometry->init();
}


void VRayScene::Node::initTransform()
{
	float tm[4][4];

	if(m_dupliObject)
		copy_m4_m4(tm, m_dupliObject->mat);
	else
		copy_m4_m4(tm, m_object->obmat);

	GetTransformHex(tm, m_transform);
}


void VRayScene::Node::initProperties()
{
	m_objectID = m_object->index;
}


void VRayScene::Node::initHash()
{
	// TODO: Add visibility to hash
	m_hash = HashCode(m_transform);
}


std::string VRayScene::Node::writeMtlMulti(PyObject *output)
{
	if(NOT(m_object->totcol))
		return "MANOMATERIALISSET";

	StringVector mtls_list;
	StringVector ids_list;

	for(int a = 1; a <= m_object->totcol; ++a) {
		Material *ma = give_current_material(m_object, a);
		if(NOT(ma))
			continue;

		std::string materialName = GetIDName((ID*)ma);

		RnaAccess::RnaValue rna(&ma->id, "vray");
		if(NOT(m_materialOverride.empty()) && NOT(rna.getBool("dontOverride")))
			materialName = m_materialOverride;

		mtls_list.push_back(materialName);
		ids_list.push_back(boost::lexical_cast<std::string>(a));
	}

	// No need for multi-material if only one slot
	// is used
	//
	if(mtls_list.size() == 1)
		return mtls_list[0];

	char obMtlName[MAX_ID_NAME];
	BLI_strncpy(obMtlName, m_object->id.name+2, MAX_ID_NAME);
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
	RnaAccess::RnaValue rna(&m_object->id, "vray.MtlWrapper");
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
	RnaAccess::RnaValue rna(&m_object->id, "vray.MtlOverride");
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
	RnaAccess::RnaValue rna(&m_object->id, "vray.MtlRenderStats");
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
	return IsNodeAnimated(m_object);
}


int VRayScene::Node::IsSmokeDomain(Object *ob)
{
	ModifierData *mod = (ModifierData*)ob->modifiers.first;
	while(mod) {
		if(mod->type == eModifierType_Smoke)
			return 1;
		mod = mod->next;
	}
	return 0;
}


int VRayScene::Node::HasHair(Object *ob)
{
	if(ob->particlesystem.first) {
		for(ParticleSystem *psys = (ParticleSystem*)ob->particlesystem.first; psys; psys = psys->next) {
			ParticleSettings *pset = psys->part;
			if(pset->type != PART_HAIR)
				continue;
			if(psys->part->ren_as == PART_DRAW_PATH)
				return 1;
		}
	}
	return 0;
}

int VRayScene::Node::DoRenderEmitter(Object *ob)
{
	if(ob->particlesystem.first) {
		int show_emitter = 0;
		for(ParticleSystem *psys = (ParticleSystem*)ob->particlesystem.first; psys; psys = psys->next)
			show_emitter += psys->part->draw & PART_DRAW_EMITTER;
		/* if no psys has "show emitter" selected don't render emitter */
		if (show_emitter == 0)
			return 0;
	}
	return 1;
}


int VRayScene::Node::isSmokeDomain()
{
	return Node::IsSmokeDomain(m_object);
}


int VRayScene::Node::hasHair()
{
	return Node::HasHair(m_object);
}


int VRayScene::Node::doRenderEmitter()
{
	return Node::DoRenderEmitter(m_object);
}


int VRayScene::Node::isDataAnimated()
{

}


int VRayScene::Node::isMeshLight()
{
	RnaAccess::RnaValue rna(&m_object->id, "vray.LightMesh");
	return rna.getBool("use");
}


void VRayScene::Node::writeGeometry(PyObject *output, int frame)
{
	m_geometry->write(output, frame);
}


void VRayScene::Node::writeHair(ExpoterSettings *settings)
{
	if(m_object->particlesystem.first) {
		for(ParticleSystem *psys = (ParticleSystem*)m_object->particlesystem.first; psys; psys = psys->next) {
			ParticleSettings *pset = psys->part;
			if(pset->type != PART_HAIR)
				continue;
			if(psys->part->ren_as != PART_DRAW_PATH)
				continue;

			GeomMayaHair *geomMayaHair = new GeomMayaHair(settings->m_sce, settings->m_main, m_object);
			geomMayaHair->init(psys);
			if(settings->m_exportNodes)
				geomMayaHair->writeNode(settings->m_fileObject, settings->m_sce->r.cfra);
			if(settings->m_exportGeometry)
				geomMayaHair->write(settings->m_fileGeom, settings->m_sce->r.cfra);
			if(NOT(settings->m_animation))
				delete geomMayaHair;
		}
	}
}
