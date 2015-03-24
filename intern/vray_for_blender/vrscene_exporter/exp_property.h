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

#ifndef CGR_EXP_PROPERTY_H
#define CGR_EXP_PROPERTY_H

#include <string>
#include <vector>


struct PluginDesc;


enum ExportMode {
	ExportModeFile = 0,
	ExportModeCloud,
	ExportModeZMQ
};


class SceneExporter
{

};


class PluginExporter
{
public:
	int        exportPlugin(const PluginDesc &pluginDesc);

private:
	int        pluginWrite(const PluginDesc &pluginDesc);
	int        pluginSendJSON(const PluginDesc &pluginDesc);
	int        pluginSendZMQ(const PluginDesc &pluginDesc);

private:
	ExportMode m_mode;

};


struct PluginAttr {
	std::string name;

	PluginAttr(const std::string &_name):
	    name(_name)
	{}

	virtual void write(const PluginExporter &exp)=0;
	virtual void send(const PluginExporter &exp)=0;

};

typedef std::vector<PluginAttr> PluginAttrs;


struct PluginAttrMapChannels:
        PluginAttr
{
	virtual void write(const PluginExporter &exp) {};
	virtual void send(const PluginExporter &exp)  {};
};


struct PluginAttrInstances:
        PluginAttr
{
	virtual void write(const PluginExporter &exp) {};
	virtual void send(const PluginExporter &exp)  {};
};


struct PluginDesc {
	std::string  pluginName;
	std::string  pluginID;
	PluginAttrs  pluginAttrs;

	PluginDesc() {}

	PluginDesc(const std::string &pluginName, const std::string &pluginID):
		pluginName(pluginName),
		pluginID(pluginID)
	{}

#if 0
	void addAttribute(const PluginAttr &attr) {
		PluginAttr *_attr = get(attr.paramName);
		if (_attr) {
			*_attr = attr;
		}
		else {
			pluginAttrs.push_back(attr);
		}
	}

	bool contains(const std::string &paramName) const {
		if (get(paramName)) {
			return true;
		}
		return false;
	}

	const PluginAttr *get(const std::string &paramName) const {
		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			if (paramName == p.paramName) {
				return &p;
			}
		}
		return nullptr;
	}

	PluginAttr *get(const std::string &paramName) {
		for (auto &pIt : pluginAttrs) {
			PluginAttr &p = pIt;
			if (paramName == p.paramName) {
				return &p;
			}
		}
		return nullptr;
	}
#endif
#if 0
	void showAttributes() const {
		PRINT_INFO("Plugin \"%s.%s\" parameters:",
				   pluginID.c_str(), pluginName.c_str())
		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			PRINT_INFO("  %s [%s]",
					   p.paramName.c_str(), p.typeStr());
		}
	}
#endif
};

#endif // CGR_EXP_PROPERTY_H
