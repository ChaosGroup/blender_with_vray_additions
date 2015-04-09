/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vfb_node_exporter.h"
#include "vfb_utils_blender.h"


AttrValue DataExporter::exportLight(BL::Object ob, bool check_updated)
{
	AttrValue plugin;

	BL::Lamp lamp(ob.data());
	if (lamp) {
		PointerRNA  vrayLamp = RNA_pointer_get(&lamp.ptr, "vray");

		// Find plugin ID
		//
		std::string pluginID;

		if (lamp.type() == BL::Lamp::type_AREA) {
			pluginID = "LightRectangle";
		}
		else if (lamp.type() == BL::Lamp::type_HEMI) {
			pluginID = "LightDome";
		}
		else if (lamp.type() == BL::Lamp::type_SPOT) {
			const int spotType = RNA_enum_get(&vrayLamp, "spot_type");
			switch(spotType) {
				case 0: pluginID = "LightSpotMax"; break;
				case 1: pluginID = "LightIESMax";  break;
			}
		}
		else if (lamp.type() == BL::Lamp::type_POINT) {
			const int omniType = RNA_enum_get(&vrayLamp, "omni_type");
			switch(omniType) {
				case 0: pluginID = "LightOmniMax";    break;
				case 1: pluginID = "LightAmbientMax"; break;
				case 2: pluginID = "LightSphere";     break;
			}
		}
		else if (lamp.type() == BL::Lamp::type_SUN) {
			const int directType = RNA_enum_get(&vrayLamp, "direct_type");
			switch(directType) {
				case 0: pluginID = "LightDirectMax"; break;
				case 1: pluginID = "SunLight";       break;
			}
		}
		else {
			PRINT_ERROR("Lamp: %s Type: %i => Lamp type is not supported!",
			            ob.name().c_str(), lamp.type());
		}

		if (!pluginID.empty()) {
			PointerRNA lampPropGroup = RNA_pointer_get(&vrayLamp, pluginID.c_str());

			PluginDesc lampDesc(ob.name(), pluginID, "Lamp@");
			lampDesc.add("transform", AttrTransform(ob.matrix_world()));

			if (pluginID == "LightRectangle") {
				BL::AreaLamp  areaLamp(lamp);

				const float sizeX = areaLamp.size() / 2.0f;
				const float sizeY = areaLamp.shape() == BL::AreaLamp::shape_SQUARE
				                    ? sizeX
				                    : areaLamp.size_y() / 2.0f;

				lampDesc.add("u_size", sizeX);
				lampDesc.add("v_size", sizeY);
			}
			else if (pluginID == "LightDome") {
				// ...
			}
			else if (pluginID == "LightSpotMax") {
				// ...
			}
			else if (ELEM(pluginID, "LightRectangle", "LightSphere", "LightDome")) {
				lampDesc.add("objectID", ob.pass_index());
			}

			float color[3];
			RNA_float_get_array(&lampPropGroup, "color", color);

			lampDesc.add("intensity", RNA_float_get(&lampPropGroup, "intensity"));
			lampDesc.add("color", AttrColor(color[0], color[1], color[2]));

			plugin = m_exporter->export_plugin(lampDesc);
		}
	}

	return plugin;
}
