#ifndef VRAY_FOR_BLENDER_PLUGIN_WRITER_FILE_H
#define VRAY_FOR_BLENDER_PLUGIN_WRITER_FILE_H

#include <cstdio>
#include <string>
#include <memory>
#include <set>

#include "vfb_plugin_attrs.h"
#include "vfb_export_settings.h"

#include "utils/cgr_vrscene.h"
#include "utils/cgr_string.h"

namespace VRayForBlender {

class PluginWriter {
public:
	PluginWriter(PyObject *pyFile, ExporterSettings::ExportFormat = ExporterSettings::ExportFormatHEX);

	PluginWriter &writeStr(const char *str);

	ExporterSettings::ExportFormat format() const { return m_format; }

	bool good() const;

	PyObject *getFile() { return m_file; }
	void setAnimationFrame(int frame) { animationFrame = frame; }
	int getAnimationFrame() const { return animationFrame; }

	bool operator==(const PluginWriter &other) const
	{
		return m_file == other.m_file;
	}

	bool operator!=(const PluginWriter &other) const
	{
		return !(*this == other);
	}


private:
	int animationFrame;
	PyObject *m_file;
	ExporterSettings::ExportFormat m_format;

private:
	PluginWriter(const PluginWriter&) = delete;
	PluginWriter &operator=(const PluginWriter&) = delete;
};


PluginWriter &operator<<(PluginWriter &pp, const int &val);
PluginWriter &operator<<(PluginWriter &pp, const float &val);
PluginWriter &operator<<(PluginWriter &pp, const char *val);
PluginWriter &operator<<(PluginWriter &pp, const std::string &val);

PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrColor &val);
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrAColor &val);
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrVector &val);
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrVector2 &val);
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrMatrix &val);
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrTransform &val);
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrPlugin &val);
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrMapChannels &val);
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrInstancer &val);

template <typename T>
using KVPair = std::pair<std::string, T>;


template <typename T>
PluginWriter &operator<<(PluginWriter &pp, const KVPair<T> &val)
{
	if (pp.getAnimationFrame() == -1) {
		return pp << "  " << val.first << "=" << val.second << ";\n";
	} else {
		return pp << "  " << val.first << "=interpolate((" << pp.getAnimationFrame() << "," << val.second << "));\n";
	}
}

template <> inline
PluginWriter &operator<<(PluginWriter &pp, const KVPair<std::string> &val)
{
	return pp << "  " << val.first << "=\"" << val.second << "\";\n";
}

template <typename T>
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrSimpleType<T> &val)
{
	return pp << val.m_Value;
}

template <typename T>
PluginWriter &printList(PluginWriter &pp, const VRayBaseTypes::AttrList<T> &val, const char *listName, bool newLine = false)
{
	if (!val.empty()) {
		pp << "List" << listName;
		if (listName[0] == '\0' || pp.format() == ExporterSettings::ExportFormatASCII) {
			pp << "(\n    " << (*val)[0];
			for (int c = 1; c < val.getCount(); c++) {
				pp << "," << (newLine ? "\n    " : "    ") << (*val)[c];
			}
			pp << "  \n)";
		} else if (pp.format() == ExporterSettings::ExportFormatZIP) {
			char * hexData = GetStringZip(reinterpret_cast<const u_int8_t *>(*val), val.getBytesCount());
			pp << "Hex(\"" << hexData << "\")";
			delete[] hexData;
		} else {
			char * zipData = GetHex(reinterpret_cast<const u_int8_t *>(*val), val.getBytesCount());
			pp << "Hex(\"" << zipData << "\")";
			delete[] zipData;
		}
	}
	return pp;
}

template <> inline
PluginWriter &printList(PluginWriter &pp, const VRayBaseTypes::AttrList<std::string> &val, const char *listName, bool newLine)
{
	if (!val.empty()) {
		pp << "List" << listName;
		pp << "(\n    \"" << StripString((*val)[0]) << "\"";
		for (int c = 1; c < val.getCount(); c++) {
			pp << "," << (newLine ? "\n    " : "    ") << "\"" << StripString((*val)[c]) << "\"";
		}
		pp << "  \n)";
	}
	return pp;
}

template <typename T>
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrList<T> &val)
{
	return printList(pp, val, "", true);
}

template <> inline
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrList<float> &val)
{
	return printList(pp, val, "Float");
}

template <> inline
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrList<int> &val)
{
	return printList(pp, val, "Int");
}

template <> inline
PluginWriter &operator<<(PluginWriter &pp, const VRayBaseTypes::AttrList<VRayBaseTypes::AttrVector> &val)
{
	return printList(pp, val, "Vector", true);
}

} // VRayForBlender

#endif // VRAY_FOR_BLENDER_PLUGIN_WRITER_FILE_H
