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

#include "utils/CGR_vrscene.h"
#include "utils/CGR_string.h"
#include "utils/CGR_blender_data.h"
#include "utils/CGR_json_plugins.h"

#include "vrscene_api.h"
#include "exp_scene.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>


void VRsceneExporter::WriteGeomStaticMesh(Object *ob, const GeomStaticMesh *geomStaticMesh, int frame)
{
	if(m_animation) {
		sprintf(m_interpStart, "interpolate((%d,", frame);
		sprintf(m_interpEnd,   "))");
	}

	// Plugin name
	PYTHON_PRINTF(m_fileGeom, "\nGeomStaticMesh %s {", geomStaticMesh->getName());

	// Mesh components
	PYTHON_PRINTF(m_fileGeom, "\n\tvertices=%sListVectorHex(\"", m_interpStart);
	PYTHON_PRINT(m_fileGeom, geomStaticMesh->getVertices());
	PYTHON_PRINTF(m_fileGeom, "\")%s;", m_interpEnd);

	PYTHON_PRINTF(m_fileGeom, "\n\tfaces=%sListIntHex(\"", m_interpStart);
	PYTHON_PRINT(m_fileGeom, geomStaticMesh->getFaces());
	PYTHON_PRINTF(m_fileGeom, "\")%s;", m_interpEnd);

	PYTHON_PRINTF(m_fileGeom, "\n\tnormals=%sListVectorHex(\"", m_interpStart);
	PYTHON_PRINT(m_fileGeom, geomStaticMesh->getNormals());
	PYTHON_PRINTF(m_fileGeom, "\")%s;", m_interpEnd);

	PYTHON_PRINTF(m_fileGeom, "\n\tfaceNormals=%sListIntHex(\"", m_interpStart);
	PYTHON_PRINT(m_fileGeom, geomStaticMesh->getFaceNormals());
	PYTHON_PRINTF(m_fileGeom, "\")%s;", m_interpEnd);

	PYTHON_PRINTF(m_fileGeom, "\n\tface_mtlIDs=%sListIntHex(\"", m_interpStart);
	PYTHON_PRINT(m_fileGeom, geomStaticMesh->getFace_mtlIDs());
	PYTHON_PRINTF(m_fileGeom, "\")%s;", m_interpEnd);

	PYTHON_PRINTF(m_fileGeom, "\n\tedge_visibility=%sListIntHex(\"", m_interpStart);
	PYTHON_PRINT(m_fileGeom, geomStaticMesh->getEdge_visibility());
	PYTHON_PRINTF(m_fileGeom, "\")%s;", m_interpEnd);

	size_t mapChannelCount = geomStaticMesh->getMapChannelCount();
	if(mapChannelCount) {
		PYTHON_PRINT(m_fileGeom, "\n\tmap_channels_names=List(");
		for(size_t i = 0; i < mapChannelCount; ++i) {
			const MChan *mapChannel = geomStaticMesh->getMapChannel(i);
			if(NOT(mapChannel))
				continue;

			PYTHON_PRINTF(m_fileGeom, "\"%s\"", mapChannel->name.c_str());
			if(i < mapChannelCount-1)
				PYTHON_PRINT(m_fileGeom, ",");
		}
		PYTHON_PRINT(m_fileGeom, ");");

		PYTHON_PRINTF(m_fileGeom, "\n\tmap_channels=%sList(", m_interpStart);
		for(size_t i = 0; i < mapChannelCount; ++i) {
			const MChan *mapChannel = geomStaticMesh->getMapChannel(i);
			if(NOT(mapChannel))
				continue;

			PYTHON_PRINTF(m_fileGeom, "List(%i,ListVectorHex(\"", mapChannel->index);
			PYTHON_PRINT(m_fileGeom, mapChannel->uv_vertices);
			PYTHON_PRINT(m_fileGeom, "\"),ListIntHex(\"");
			PYTHON_PRINT(m_fileGeom, mapChannel->uv_faces);
			PYTHON_PRINT(m_fileGeom, "\"))");

			if(i < mapChannelCount-1)
				PYTHON_PRINT(m_fileGeom, ",");
		}
		PYTHON_PRINTF(m_fileGeom, ")%s;", m_interpEnd);
	}

	PYTHON_PRINT(m_fileGeom, "\n}\n");
}


std::string VRsceneExporter::WriteMtlMulti(Object *ob)
{
	if(NOT(ob->totcol))
		return "MtlNoMaterial";

	StringVector mtls_list;
	StringVector ids_list;

	for(int a = 1; a <= ob->totcol; ++a) {
		Material *ma = give_current_material(ob, a);
		if(NOT(ma))
			continue;

		mtls_list.push_back(ma->id.name);
		ids_list.push_back(boost::lexical_cast<std::string>(a-1));
	}

	// No need for multi-material if only one slot
	// is used
	//
	if(mtls_list.size() == 1)
		return mtls_list[0];

	std::string plugName("MM");
	plugName.append(ob->id.name+2);

	PYTHON_PRINTF(m_fileObject, "\nMtlMulti %s {", plugName.c_str());
	PYTHON_PRINTF(m_fileObject, IND"mtls_list=List(%s);", boost::algorithm::join(mtls_list, ",").c_str());
	PYTHON_PRINTF(m_fileObject, IND"ids_list=ListInt(%s);", boost::algorithm::join(ids_list, ",").c_str());
	PYTHON_PRINT(m_fileObject, "\n}\n");

	return plugName;
}


void VRsceneExporter::WriteNode(Object *ob, const VRScene::Node *node, int frame)
{
	if(m_animation) {
		sprintf(m_interpStart, "interpolate((%d,", frame);
		sprintf(m_interpEnd,   "))");
	}

	// Move to Node.{h,cpp}
	//
	std::string materialName = WriteMtlMulti(ob);

	PYTHON_PRINTF(m_fileObject, "\nNode %s {", node->getName());
	PYTHON_PRINTF(m_fileObject, IND"geometry=%s;", node->getDataName());
	PYTHON_PRINTF(m_fileObject, IND"material=%s;", materialName.c_str());
	PYTHON_PRINTF(m_fileObject, IND"objectID=%i;", node->getObjectID());
	PYTHON_PRINTF(m_fileObject, IND"transform=%sTransformHex(\"%s\")%s;", m_interpStart, node->getTransform(), m_interpEnd);
	PYTHON_PRINT(m_fileObject, "\n}\n");
}


VRsceneExporter::VRsceneExporter(Scene *sce, Main *main, PyObject *obFile, PyObject *geomFile, PyObject *lightsFile):
	m_sce(sce),
	m_main(main),
	m_fileObject(obFile),
	m_fileGeom(geomFile),
	m_fileLights(lightsFile)
{
	PRINT_INFO("VRsceneExporter::VRsceneExporter()");

	m_eval_ctx.for_render = true;

	m_exportNodes = false;
	m_exportGeometry = true;

	m_animation = false;

	m_activeLayers = true;
	m_altDInstances = false;
	m_checkAnimated = ANIM_CHECK_BOTH;

	sprintf(m_interpStart, "%s", "");
	sprintf(m_interpEnd,   "%s", "");
}


VRsceneExporter::~VRsceneExporter()
{
	PRINT_INFO("VRsceneExporter::~VRsceneExporter()");
}


void VRsceneExporter::exportScene()
{
	PRINT_INFO("VRsceneExporter::exportScene()");

	double timeMeasure = 0.0;
	char   timeMeasureBuf[32];

	PRINT_INFO_LB("VRsceneExporter: Exporting scene for frame %i...", m_sce->r.cfra);
	timeMeasure = PIL_check_seconds_timer();

	// Export stuff

	Base *base = (Base*)m_sce->base.first;
	while(base) {
		Object *ob = base->object;
		base = base->next;

		// PRINT_INFO("Processing '%s'...", ob->id.name);

		// Skip object here, but not in dupli!
		// Dupli could be particles and it's better to
		// have animated 'visible' param there
		//
		if(ob->restrictflag & OB_RESTRICT_RENDER)
			continue;

		if(m_activeLayers)
			if(NOT(ob->lay & m_sce->lay))
				continue;

		// Export objects
		//
		if(m_exportNodes) {
			if(GEOM_TYPE(ob) || EMPTY_TYPE(ob)) {
				// Free duplilist if there is some for some reason
				FreeDupliList(ob);

				ob->duplilist = object_duplilist(&m_eval_ctx, m_sce, ob);

				for(DupliObject *dob = (DupliObject*)ob->duplilist->first; dob; dob = dob->next) {
					VRScene::Node *node = new VRScene::Node();
					node->init(m_sce, m_main, ob, dob);

					WriteNode(ob, node, m_sce->r.cfra);
				}

				FreeDupliList(ob);

				// TODO: Check particle systems for 'Render Emitter' prop

				if(NOT(EMPTY_TYPE(ob))) {
					VRScene::Node *node = new VRScene::Node();
					node->init(m_sce, m_main, ob, NULL);

					WriteNode(ob, node, m_sce->r.cfra);
				}
			}
			else if(ob->type == OB_LAMP) {
			}
		}

		// Export geometry
		//
		if(m_exportGeometry) {
			if(NOT(m_animation)) {
				GeomStaticMesh geomStaticMesh;
				geomStaticMesh.init(m_sce, m_main, ob);
				if(geomStaticMesh.getHash())
					WriteGeomStaticMesh(ob, &geomStaticMesh);
			}
			else {
				if(m_checkAnimated == ANIM_CHECK_NONE) {
					GeomStaticMesh geomStaticMesh;
					geomStaticMesh.init(m_sce, m_main, ob);
					if(geomStaticMesh.getHash())
						WriteGeomStaticMesh(ob, &geomStaticMesh, m_sce->r.cfra);
				}
				else if(m_checkAnimated == ANIM_CHECK_HASH || m_checkAnimated == ANIM_CHECK_BOTH) {
					std::string obName(ob->id.name);

					if(m_checkAnimated == ANIM_CHECK_BOTH)
						if(NOT(IsMeshAnimated(ob)))
							continue;

					GeomStaticMesh *geomStaticMesh = new GeomStaticMesh();
					geomStaticMesh->init(m_sce, m_main, ob);

					MHash curHash  = geomStaticMesh->getHash();
					MHash prevHash = m_meshCache.getHash(obName);

					// TODO: add to cache for new pipeline
					//
					if(NOT(curHash == prevHash)) {
						// Write previous frame if hash is more then 'frame_step' back
						// If 'prevHash' is 0 than previous call was for the first frame
						// and no need to export
						if(prevHash) {
							int cacheFrame = m_meshCache.getFrame(obName);
							int prevFrame  = m_sce->r.cfra - m_sce->r.frame_step;

							if(cacheFrame < prevFrame) {
								WriteGeomStaticMesh(ob, m_meshCache.getData(obName), prevFrame);
							}
						}

						// Write current frame data
						WriteGeomStaticMesh(ob, geomStaticMesh, m_sce->r.cfra);

						// This will free previous data and store new pointer
						m_meshCache.update(obName, curHash, m_sce->r.cfra, geomStaticMesh);
					}
				}
				else if(m_checkAnimated == ANIM_CHECK_SIMPLE) {
					if(IsMeshAnimated(ob)) {
						GeomStaticMesh geomStaticMesh;
						geomStaticMesh.init(m_sce, m_main, ob);
						if(geomStaticMesh.getHash()) {
							WriteGeomStaticMesh(ob, &geomStaticMesh, m_sce->r.cfra);
						}
					}
				} // ANIM_CHECK_SIMPLE
			} // animated
		} // m_exportGeometry
	} // while(base)

	BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
	printf(" done [%s]\n", timeMeasureBuf);
}
