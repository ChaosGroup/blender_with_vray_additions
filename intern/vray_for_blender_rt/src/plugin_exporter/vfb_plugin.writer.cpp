#include "vfb_plugin_writer.h"
#include "BLI_fileops.h"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace VRayBaseTypes;

namespace VRayForBlender {

PluginWriter::PluginWriter(std::string fname, ExporterSettings::ExportFormat format):
	m_FileName(std::move(fname)),
	m_Buff(4096),
	m_File(nullptr),
	m_Format(format),
	m_TryOpen(false)
{
}

bool PluginWriter::doOpen()
{
	m_TryOpen = true;
	if (!m_File) {
		m_File = BLI_fopen(m_FileName.c_str(), "wb");
	}
	return m_File != nullptr;
}

bool PluginWriter::good() const
{
	return !m_TryOpen || m_File != nullptr && m_TryOpen;
}

std::string PluginWriter::getName() const
{
	return fs::path(m_FileName).filename().string();
}

PluginWriter &PluginWriter::include(std::string name)
{
	if (name != m_FileName && !name.empty()) {
		// dont include self
		m_Includes.insert(std::move(name));
	}
	return *this;
}

PluginWriter::~PluginWriter()
{
	if (!m_Includes.empty()) {
		*this << "\n";
		for (const auto &inc : m_Includes) {
			*this << "#include \"" << inc << "\"\n";
		}
	}

	if (m_File) {
		fclose(m_File);
	}
}

PluginWriter &PluginWriter::write(const char *format, ...)
{
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

	this->writeData(m_Buff.data(), len);

	return *this;
}

PluginWriter &PluginWriter::writeStr(const char *str)
{
	return this->writeData(str, strlen(str));
}

PluginWriter &PluginWriter::writeData(const void *data, int size)
{
	if (doOpen()) {
		fwrite(data, 1, size, m_File);
	}
	return *this;
}

PluginWriter &operator<<(PluginWriter &pp, const int &val)
{
	return pp.write("%d", val);
}

PluginWriter &operator<<(PluginWriter &pp, const float &val)
{
	return pp.write("%g", val);
}

PluginWriter &operator<<(PluginWriter &pp, const char *val)
{
	return pp.writeStr(val);
}

PluginWriter &operator<<(PluginWriter &pp, const std::string &val)
{
	return pp.writeData(val.c_str(), val.size());
}

PluginWriter &operator<<(PluginWriter &pp, const AttrColor &val)
{
	return pp.write("Color(%g, %g, %g)", val.r, val.g, val.b);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrAColor &val)
{
	return pp.write("AColor(%g, %g, %g, %g)", val.color.r, val.color.g, val.color.b, val.alpha);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrVector &val)
{
	return pp.write("Vector(%g, %g, %g)", val.x, val.y, val.z);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrVector2 &val)
{
	return pp.write("Vector2(%g, %g)", val.x, val.y);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrMatrix &val)
{
	return pp << "Matrix(" << val.v0 << "," << val.v1 << "," << val.v2 << ")";
}

PluginWriter &operator<<(PluginWriter &pp, const AttrTransform &val)
{
	return pp << "Transform(" << val.m << "," << val.offs << ")";
}

PluginWriter &operator<<(PluginWriter &pp, const AttrPlugin &val)
{
	return pp << StripString(val.plugin);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrMapChannels &val)
{
	pp << "List(\n";

	if (!val.data.empty()) {
		auto iter = val.data.cbegin();
		pp << "List(" << iter->first << ",\n";
		pp << iter->second.vertices << "," << iter->second.faces << ")";

		for (; iter != val.data.cend(); ++iter) {
			pp << ",\nList(" << iter->first << ",\n";
			pp << iter->second.vertices << "," << iter->second.faces << ")";
		}
	}

	return pp << ")";
}

PluginWriter &operator<<(PluginWriter &pp, const AttrInstancer &val)
{
	pp << "List(" << val.frameNumber;

	if (!val.data.empty()) {
		pp << ",\n    List(" << (*val.data)[0].index << ", " << (*val.data)[0].tm << ", " << (*val.data)[0].vel << "," << (*val.data)[0].node << ")";
		for (int c = 1; c < val.data.getCount(); c++) {
			const auto &item = (*val.data)[c];
			pp << ",\n    List(" << item.index << ", " << item.tm << ", " << item.vel << "," << item.node << ")";
		}
	}

	return pp << "\n)";
}

} // VRayForBlender
