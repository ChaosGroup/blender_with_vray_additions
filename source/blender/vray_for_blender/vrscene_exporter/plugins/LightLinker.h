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

#ifndef CGR_LIGHT_LINKER_H
#define CGR_LIGHT_LINKER_H

#include "exp_types.h"
#include "CGR_vrscene.h"

#include <boost/range/algorithm/set_algorithm.hpp>


using namespace VRayScene;


struct LightList {
	enum {
		typeNone    = 0,
		typeExclude = 1,
		typeInclude = 2,
	};

	StrSet  obList;
	int     obListType;
	int     flags;
};


class LightLinker {
	typedef std::map<std::string, LightList> LightLink;
	typedef std::map<std::string, StrSet>    LightIgnore;

	enum {
		excludeIllumination = 0,
		excludeShadows      = 1,
		excludeBoth         = 2
	};

public:
	LightLinker();

	void           init(BL::BlendData data, BL::Scene sce);
	
	void           prepass();
	void           write(PyObject *output);

	void           excludePlugin(const std::string &refOb, const std::string &pluginName);

	void           setSceneSet(StrSet *ptr) { m_scene_nodes = ptr; }

private:
	void           getObject(const std::string &name, StrSet &list);
	void           getGroupObjects(const std::string &name, StrSet &list);

	BL::Scene      m_sce;
	BL::BlendData  m_data;
	StrSet        *m_scene_nodes;

	LightLink      m_include_exclude;
	LightIgnore    m_ignored_lights;
	LightIgnore    m_ignored_shadow_lights;

};

#endif // CGR_LIGHT_LINKER_H
