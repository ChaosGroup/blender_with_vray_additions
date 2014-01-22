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

#include <boost/lexical_cast.hpp>

extern "C" {
#  include "DNA_modifier_types.h"
#  include "BKE_depsgraph.h"
#  include "BKE_scene.h"
#  include "BLI_math.h"
#  include "MEM_guardedalloc.h"
#  include "RNA_access.h"
#  include "BLI_sys_types.h"
#  include "BLI_string.h"
#  include "BLI_path_util.h"
}


VRScene::Node::Node()
{
	name.clear();
	dataName.clear();

	hash = 0;

	object      = NULL;
	dupliObject = NULL;
}


void VRScene::Node::init(Scene *sce, Main *main, Object *ob, DupliObject *dOb)
{
    float tm[4][4];

	object = ob;
	dupliObject = dOb;

	if(dupliObject) {
		object = dupliObject->ob;
		copy_m4_m4(tm, dupliObject->mat);
	}
	else
		copy_m4_m4(tm, object->obmat);

	GetTransformHex(tm, transform);

    objectID = object->index;

	initName();
	// initWrappers();
}


void VRScene::Node::freeData()
{
}


char* VRScene::Node::getTransform() const
{
    return const_cast<char*>(transform);
}


int VRScene::Node::getObjectID() const
{
	return objectID;
}


void VRScene::Node::initName()
{
	char obName[MAX_ID_NAME] = "";

	// Get object name with 'OB' prefix (+2)
	//
	BLI_strncpy(obName, object->id.name+2, MAX_ID_NAME);
	StripString(obName);

	// Construct object (Node) name
	//
	name.clear();
	name.append("OB");
	name.append(obName);

	if(dupliObject) {
		name.append(boost::lexical_cast<std::string>(dupliObject->persistent_id[0]));
	}

	// Check if object is linked
	if(object->id.lib) {
		char libFilename[FILE_MAX] = "";

		BLI_split_file_part(object->id.lib->name+2, libFilename, FILE_MAX);
		BLI_replace_extension(libFilename, FILE_MAX, "");

		StripString(libFilename);

		name.append("LI");
		name.append(libFilename);
	}

	// Construct data (geometry) name
	//
	dataName.clear();
	dataName.append("ME");
	dataName.append(obName);

	// Check if object's data is linked
	const ID *dataID = (ID*)object->data;
	if(dataID->lib) {
		char libFilename[FILE_MAX] = "";
		BLI_split_file_part(dataID->lib->name+2, libFilename, FILE_MAX);
		BLI_replace_extension(libFilename, FILE_MAX, "");

		StripString(libFilename);

		dataName.append("LI");
		dataName.append(libFilename);
	}
}


void VRScene::Node::initWrappers()
{
    boost::property_tree::ptree mtlRenderStatsTree;

    ReadPluginDesc("MtlRenderStats", mtlRenderStatsTree);

    RnaAccess::RnaValue rnaValueAccess(&object->id, "vray.MtlRenderStats");

    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, mtlRenderStatsTree.get_child("Parameters")) {
        std::string attrName = v.second.get_child("attr").data();
        std::string attrType = v.second.get_child("type").data();

        std::cout << "Name: " << attrName << std::endl;
        std::cout << "  Type: " << attrType << std::endl;

        if(attrType == "BOOL") {
            bool defaultValue = v.second.get<bool>("default");

            bool value;
            rnaValueAccess.GetValue(attrName.c_str(), value);

            std::cout << "  Default: " << defaultValue << std::endl;
            std::cout << "  Value: "   << value << std::endl;
        }
    }
}
