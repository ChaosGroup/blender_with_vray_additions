#ifndef VRAY_FOR_BLENDER_PLUGIN_WRITER_FILE_H
#define VRAY_FOR_BLENDER_PLUGIN_WRITER_FILE_H

#include <cstdio>
#include <string>
#include <memory>
#include "vfb_plugin_attrs.h"

namespace VRayForBlender {


static const int static_buf_size = 4096;

class PluginWriter {
public:

	PluginWriter(std::string fname);
	~PluginWriter();

	PluginWriter(const PluginWriter &) = delete;
	PluginWriter & operator=(const PluginWriter &) = delete;

	PluginWriter & writeStr(const char * str) {
		fwrite(str, 1, strlen(str), m_File);
		return *this;
	}

	PluginWriter & write(const char * format, ...) {
		va_list args;

		va_start(args, format);
		int len = vsnprintf(m_Buff.get(), static_buf_size, format, args);
		va_end(args);

		fwrite(m_Buff.get(), 1, len, m_File);
		return *this;
	}


private:
	std::string m_FileName;
	FILE * m_File;
	std::unique_ptr<char[]>  m_Buff;
};

PluginWriter & operator<<(PluginWriter & pp, const int & val);
PluginWriter & operator<<(PluginWriter & pp, const float & val);
PluginWriter & operator<<(PluginWriter & pp, const char * val);
PluginWriter & operator<<(PluginWriter & pp, const std::string & val);

PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrColor & val);
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrAColor & val);
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrVector & val);
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrVector2 & val);
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrMatrix & val);
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrTransform & val);
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrPlugin & val);
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrMapChannels & val);
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrInstancer & val);

void         StripString(char *str);
std::string  StripString(const std::string &str);

template <typename T>
using KVPair = std::pair<std::string, T>;


template <typename T>
PluginWriter & operator<<(PluginWriter & pp, const KVPair<T> & val) {
	return pp << "  " << val.first << "=" << val.second << ";\n";
}

template <> inline
PluginWriter & operator<<(PluginWriter & pp, const KVPair<std::string> & val) {
	return pp << "  " << val.first << "=\"" << val.second << "\";\n";
}


template <typename T>
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrSimpleType<T> & val) {
	return pp << val.m_Value;
}

template <typename T>
PluginWriter & printList(PluginWriter & pp, const VRayBaseTypes::AttrList<T> & val, const char * listName, bool newLine = false) {
	if (!val.empty()) {
		pp << "List" << listName << "(\n" << (*val)[0];
		for (int c = 1; c < val.getCount(); c++) {
			pp << "," << (newLine ? "\n" : "") << (*val)[c];
		}
		pp << ")";
	}
	return pp;
}

template <typename T>
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrList<T> & val) {
	return printList(pp, val, "", true);
}

template <> inline
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrList<int> & val) {
	return printList(pp, val, "Int");
}

template <> inline
PluginWriter & operator<<(PluginWriter & pp, const VRayBaseTypes::AttrList<VRayBaseTypes::AttrVector> & val) {
	return printList(pp, val, "Vector", true);
}


} // VRayForBlender



#endif // VRAY_FOR_BLENDER_PLUGIN_WRITER_FILE_H
