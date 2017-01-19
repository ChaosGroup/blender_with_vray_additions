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

#ifndef TEX_VOXEL_DATA_H
#define TEX_VOXEL_DATA_H

#include "vfb_plugin_exporter.h"
#include "vfb_export_settings.h"
#include "vfb_typedefs.h"
#include "vfb_params_desc.h"

#include "DNA_object_types.h"
#include "DNA_ID.h"

#include <map>
#include <memory>

extern "C" {
#  include "DNA_modifier_types.h"
#  include "DNA_smoke_types.h"
}

#define CGR_USE_HEAT           0
#define CGR_DEBUG_GIZMO_SHAPE  0


namespace VRayForBlender {

class TexVoxelData {
public:
	TexVoxelData(Object *ob)
	    : m_smd(nullptr)
	    , p_interpolation(0)
	    , m_ob(ob)
	{}

	void               initName(const std::string &name);
	AttrValue          export_plugins(PluginExporter *exporter);

	void               init(SmokeModifierData *smd);
	void               setInterpolation(int value);

private:
	void               initUvTransform();
	void               initSmoke();

	SmokeModifierData *m_smd;

	int                m_res_high[3];

	AttrListFloat      m_dens;
	AttrListFloat      m_flame;
	AttrListFloat      m_fuel;
	std::string        m_name;

#if CGR_USE_HEAT
	int                m_res_low[3];
	AttrListFloat      m_heat;
#endif

	float              m_uvw_transform[4][4];
	int                p_interpolation;
	Object            *m_ob;
};

} // namespace VRayForBlender

#endif // TEX_VOXEL_DATA_H
