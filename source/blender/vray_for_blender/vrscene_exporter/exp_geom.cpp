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

#include "exp_defines.h"
#include "GeomStaticMesh.h"
#include "vrscene_api.h"


using namespace VRayScene;


static void WritePythonAttribute(PyObject *outputFile, PyObject *propGroup, const char *attrName)
{
	static char buf[CGR_MAX_PLUGIN_NAME];

	PyObject *attr      = NULL;
	PyObject *attrValue = NULL;

	if(propGroup == Py_None)
		return;

	attr      = PyObject_GetAttrString(propGroup, attrName);
	attrValue = PyNumber_Long(attr);

	if(attrValue) {
        PYTHON_PRINTF(outputFile, "\n\t%s=%li;", attrName, PyLong_AsLong(attrValue));
	}
}


void write_Mesh(PyObject *outputFile, Scene *sce, Object *ob, Main *main, const char *pluginName, PyObject *propGroup)
{
    PYTHON_PRINT_BUF;

    char interpStart[64] = "";
    char interpEnd[3] = "";

    GeomStaticMesh geomStaticMesh;
    geomStaticMesh.init(sce, main, ob);

    if(NOT(geomStaticMesh.getHash()))
        return;

    int useAnimation = false;
    if(useAnimation) {
        sprintf(interpStart, "interpolate((%d,", sce->r.cfra);
        sprintf(interpEnd, "))");
    }

    // Plugin name
    PYTHON_PRINTF(outputFile, "\nGeomStaticMesh %s {", pluginName);

    // Mesh components
    PYTHON_PRINTF(outputFile, "\n\tvertices=%sListVectorHex(\"", interpStart);
    PYTHON_PRINT(outputFile, geomStaticMesh.getVertices());
    PYTHON_PRINTF(outputFile, "\")%s;", interpEnd);

    PYTHON_PRINTF(outputFile, "\n\tfaces=%sListIntHex(\"", interpStart);
    PYTHON_PRINT(outputFile, geomStaticMesh.getFaces());
    PYTHON_PRINTF(outputFile, "\")%s;", interpEnd);

    PYTHON_PRINTF(outputFile, "\n\tnormals=%sListVectorHex(\"", interpStart);
    PYTHON_PRINT(outputFile, geomStaticMesh.getNormals());
    PYTHON_PRINTF(outputFile, "\")%s;", interpEnd);

    PYTHON_PRINTF(outputFile, "\n\tfaceNormals=%sListIntHex(\"", interpStart);
    PYTHON_PRINT(outputFile, geomStaticMesh.getFaceNormals());
    PYTHON_PRINTF(outputFile, "\")%s;", interpEnd);

    PYTHON_PRINTF(outputFile, "\n\tface_mtlIDs=%sListIntHex(\"", interpStart);
    PYTHON_PRINT(outputFile, geomStaticMesh.getFace_mtlIDs());
    PYTHON_PRINTF(outputFile, "\")%s;", interpEnd);

    PYTHON_PRINTF(outputFile, "\n\tedge_visibility=%sListIntHex(\"", interpStart);
    PYTHON_PRINT(outputFile, geomStaticMesh.getEdge_visibility());
    PYTHON_PRINTF(outputFile, "\")%s;", interpEnd);

    size_t mapChannelCount = geomStaticMesh.getMapChannelCount();
    if(mapChannelCount) {
        PYTHON_PRINTF(outputFile, "\n\tmap_channels_names=List(");
        for(size_t i = 0; i < mapChannelCount; ++i) {
            const MChan *mapChannel = geomStaticMesh.getMapChannel(i);
            if(NOT(mapChannel))
                continue;

            PYTHON_PRINTF(outputFile, "\"%s\"", mapChannel->name.c_str());
            if(i < mapChannelCount-1)
                PYTHON_PRINT(outputFile, ",");
        }
        PYTHON_PRINTF(outputFile, ");");

        PYTHON_PRINTF(outputFile, "\n\tmap_channels=%sList(", interpStart);
        for(size_t i = 0; i < mapChannelCount; ++i) {
            const MChan *mapChannel = geomStaticMesh.getMapChannel(i);
            if(NOT(mapChannel))
                continue;

            PYTHON_PRINTF(outputFile, "List(%i,ListVectorHex(\"", mapChannel->index);
            PYTHON_PRINT(outputFile, mapChannel->uv_vertices);
            PYTHON_PRINT(outputFile, "\"),ListIntHex(\"");
            PYTHON_PRINT(outputFile, mapChannel->uv_faces);
            PYTHON_PRINT(outputFile, "\"))");

            if(i < mapChannelCount-1)
                PYTHON_PRINT(outputFile, ",");
        }
        PYTHON_PRINTF(outputFile, ");");
    }

    // Custom attibutes
    if(propGroup) {
        WritePythonAttribute(outputFile, propGroup, "dynamic_geometry");
        WritePythonAttribute(outputFile, propGroup, "osd_subdiv_level");
    }

    PYTHON_PRINT(outputFile, "\n}\n");
}
