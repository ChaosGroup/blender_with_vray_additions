#include "vfb_plugin_writer.h"


using namespace VRayBaseTypes;

namespace VRayForBlender {
void StripString(char *str) {
	int nChars = strlen(str);
	int i = 0;

	for (i = 0; i < nChars; i++) {
		if (str[i]) {
			if (str[i] == '+')
				str[i] = 'p';
			else if (str[i] == '-')
				str[i] = 'm';
			else if (str[i] == '|' ||
				str[i] == '@')
				continue;
			else if (!((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z') || (str[i] >= '0' && str[i] <= '9')))
				str[i] = '_';
		}
	}
}


std::string StripString(const std::string &str) {
	static char buf[CGR_MAX_PLUGIN_NAME];
	strncpy(buf, str.c_str(), CGR_MAX_PLUGIN_NAME);
	StripString(buf);
	return buf;
}


PluginWriter::PluginWriter(std::string fname, ExportFormat format)
	: m_FileName(std::move(fname)), m_Buff(4096), m_File(nullptr), m_Format(format) {

	m_File = fopen(m_FileName.c_str(), "wb");
}

PluginWriter::~PluginWriter() {
	fclose(m_File);
}

PluginWriter & PluginWriter::write(const char * format, ...) {
	va_list args;

	va_start(args, format);
	int len = vsnprintf(m_Buff.data(), m_Buff.size(), format, args);
	va_end(args);

	if (len > m_Buff.size()) {
		m_Buff.resize(len + 1);

		va_start(args, format);
		len = vsnprintf(m_Buff.data(), m_Buff.size(), format, args);
		va_end(args);
	}

	fwrite(m_Buff.data(), 1, len, m_File);

	return *this;
}

PluginWriter & PluginWriter::writeStr(const char * str) {
	return this->writeData(str, strlen(str));
}

PluginWriter & PluginWriter::writeData(const void * data, int size) {
	fwrite(data, 1, size, m_File);
	return *this;
}

PluginWriter & operator<<(PluginWriter & pp, const int & val) {
	return pp.write("%d", val);
}

PluginWriter & operator<<(PluginWriter & pp, const float & val) {
	return pp.write("%.6g", val);
}

PluginWriter & operator<<(PluginWriter & pp, const char * val) {
	return pp.writeStr(val);
}

PluginWriter & operator<<(PluginWriter & pp, const std::string & val) {
	return pp.writeData(val.c_str(), val.size());
}

PluginWriter & operator<<(PluginWriter & pp, const AttrColor & val) {
	return pp.write("Color(%.6g, %.6g, %.6g)", val.r, val.g, val.b);
}

PluginWriter & operator<<(PluginWriter & pp, const AttrAColor & val) {
	return pp.write("AColor(%.6g, %.6g, %.6g, %.6g)", val.color.r, val.color.g, val.color.b, val.alpha);
}

PluginWriter & operator<<(PluginWriter & pp, const AttrVector & val) {
	return pp.write("Vector(%.6g, %.6g, %.6g)", val.x, val.y, val.z);
}

PluginWriter & operator<<(PluginWriter & pp, const AttrVector2 & val) {
	return pp.write("Vector2(%.6g, %.6g)", val.x, val.y);
}

PluginWriter & operator<<(PluginWriter & pp, const AttrMatrix & val) {
	return pp << "Matrix(" << val.v0 << "," << val.v1 << "," << val.v2 << ")";
}

PluginWriter & operator<<(PluginWriter & pp, const AttrTransform & val) {
	return pp << "Transform(" << val.m << "," << val.offs << ")";
}

PluginWriter & operator<<(PluginWriter & pp, const AttrPlugin & val) {
	return pp << StripString(val.plugin);
}

PluginWriter & operator<<(PluginWriter & pp, const AttrMapChannels & val) {
	pp << "List(\n";

	if (!val.data.empty()) {
		auto iter = val.data.cbegin();
		pp << "List(" << iter->first<< ",\n";
		pp << iter->second.vertices << "," << iter->second.faces << ")";

		for (; iter != val.data.cend(); ++iter) {
			pp << ",\nList(" << iter->first << ",\n";
			pp << iter->second.vertices << "," << iter->second.faces << ")";
		}
	}

	return pp << ")";
}

PluginWriter & operator<<(PluginWriter & pp, const AttrInstancer & val) {
	pp << "List(" << val.frameNumber;

	if (!val.data.empty()) {
		pp << ",\n    List(" << (*val.data)[0].index << ", " << (*val.data)[0].tm << ", " << (*val.data)[0].vel << "," << (*val.data)[0].node << ")";
		for (int c = 1; c < val.data.getCount(); c++) {
			const auto & item = (*val.data)[c];
			pp << ",\n    List(" << item.index << ", " << item.tm << ", " << item.vel << "," << item.node << ")";
		}
	}

	return pp << "\n)";
}

} // VRayForBlender