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

#include "GeomMeshFile.h"
#include "CGR_rna.h"
#include "CGR_string.h"
#include "CGR_vrscene.h"

#include "BLI_path_util.h"


void VRayScene::GeomMeshFile::preInit()
{
	initName();
	initHash();
}


void VRayScene::GeomMeshFile::initName(const std::string &name)
{
	if(NOT(name.empty())) {
		m_name = name;
	}
	else {
		RnaAccess::RnaValue rna((ID*)m_ob->data, "vray.GeomMeshFile");

		std::string filePath = rna.getPath("file");
		if(NOT(filePath.empty())) {
			char pluginName[CGR_MAX_PLUGIN_NAME];

			char fileName[FILE_MAXFILE];
			strncpy(fileName, BLI_path_basename(filePath.c_str()), FILE_MAXFILE);

			snprintf(pluginName, CGR_MAX_PLUGIN_NAME,
					 "VRayProxy%sA%iS%.0fO%.0f",
					 fileName, rna.getEnum("anim_type"), rna.getFloat("anim_speed"), rna.getFloat("anim_offset"));
			StripString(pluginName);

			m_name = pluginName;
		}
		else {
			m_name = "geomMeshFile";
		}
	}
}


void VRayScene::GeomMeshFile::initHash()
{
	RnaAccess::RnaValue rna((ID*)m_ob->data, "vray.GeomMeshFile");

	m_plugin.str("");
	m_plugin << "\n"   << "GeomMeshFile" << " " << m_name << " {";
	m_plugin << "\n\t" << "file"        << "=\"" << rna.getPath("file") << "\";";
	m_plugin << "\n\t" << "anim_type"   << "=" << rna.getEnum("anim_type") << ";";
	m_plugin << "\n\t" << "anim_speed"  << "=" << rna.getFloat("anim_speed") << ";";
	m_plugin << "\n\t" << "anim_offset" << "=" << rna.getFloat("anim_offset") << ";";
	m_plugin << "\n}\n";

	m_hash = HashCode(m_plugin.str().c_str());
}


void VRayScene::GeomMeshFile::writeData(PyObject *output, VRayExportable *prevState, bool keyFrame)
{
	PYTHON_PRINT(output, m_plugin.str().c_str());
}

