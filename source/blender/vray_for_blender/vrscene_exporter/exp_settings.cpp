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

#include "exp_settings.h"

ExpoterSettings ExpoterSettings::gSet;


void ExpoterSettings::init(BL::Scene scene, BL::BlendData data, BL::RenderEngine engine)
{
	b_scene  = scene;
	b_data   = data;
	b_engine = engine;
}


void ExpoterSettings::reset()
{
	m_sce  = NULL;
	m_main = NULL;

	m_fileObject = NULL;
	m_fileGeom   = NULL;
	m_fileLights = NULL;
	m_fileMat    = NULL;
	m_fileTex    = NULL;

	m_activeLayers  = 0;
	m_useInstancerForGroup = false;

	m_isAnimation  = false;
	m_frameCurrent = 0;
	m_frameStart   = 1;
	m_frameStep    = 1;

	m_mtlOverride = "";
	m_drSharePath = "";
}


bool ExpoterSettings::DoUpdateCheck()
{
	return m_isAnimation && (m_frameCurrent > m_frameStart);
}


bool ExpoterSettings::IsFirstFrame()
{
	if(NOT(m_isAnimation))
		return true;
	if(m_frameCurrent > m_frameStart)
		return true;
	return false;
}
