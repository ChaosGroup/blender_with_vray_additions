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

#ifndef CGR_EXPORT_SCENE_H
#define CGR_EXPORT_SCENE_H

#include "CGR_config.h"

#include "vrscene_exporter/exp_defines.h"
#include "vrscene_exporter/exp_types.h"

#include "vrscene_exporter/Node.h"

#include "BKE_depsgraph.h"
#include "MEM_guardedalloc.h"
#include "RNA_blender_cpp.h"

#include <Python.h>

#include <string>
#include <vector>


using namespace VRayScene;


// Blender gives us particles for the current frame,
// so we need to track when they appear and dissappear
//
class VRsceneParticles {
	typedef std::vector<Node*> Particles;

public:
	const int size() const {
		return m_particles.size();
	}

	void append(Node *node) {
		m_visibleParticles.push_back(node);
	}

	void write() {
		// First call; simply copy data
		if(NOT(m_particles.size())) {
			m_particles.insert(m_particles.end(), m_visibleParticles.begin(), m_visibleParticles.end());
		}

		Particles::const_iterator nodeIt;
		for(nodeIt = m_particles.begin(); nodeIt != m_particles.end(); ++nodeIt) {
			Node *node = *nodeIt;
			node->setVisiblity(false);

			Particles::const_iterator visibleIt;
			for(visibleIt = m_visibleParticles.begin(); visibleIt != m_visibleParticles.end(); ++visibleIt) {
				Node *newNode = *visibleIt;
				if(node->getMName() == newNode->getMName()) {
					node->setVisiblity(true);
					continue;
				}
			}
		}
	}

	void clear() {
		m_particles.clear();
		m_visibleParticles.clear();
	}

private:
	Particles  m_visibleParticles;
	Particles  m_particles;
	StrSet     m_particlesNames;

};


class VRsceneExporter {
	friend class VRsceneParticles;

public:
	VRsceneExporter(ExpoterSettings *settings);
	~VRsceneExporter();

	void               exportScene();

private:
	void               init();

	void               exportNode(Node *node);

	void               exportObjectBase(Object *ob);
	void               exportObject(Object *ob, const int &visible=true, DupliObject *dOb=NULL, const int &from_particles=false);
	void               exportLight(Object *ob, DupliObject *dOb=NULL);

#if CGR_USE_CPP_API
	void               exportObject(BL::Object dupOb, BLTm tm, bool visible=true);
#endif

	int                checkUpdates();

	ExpoterSettings   *m_settings;

	std::string        m_mtlOverride;

	VRsceneParticles   m_particles;

	PYTHON_PRINT_BUF;

};

#endif // CGR_EXPORT_SCENE_H
