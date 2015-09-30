#include "vfb_plugin_writer.h"
#include "BLI_fileops.h"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace VRayBaseTypes;

namespace VRayForBlender {

struct TraceTransformHex {
	TraceTransformHex(AttrTransform tm) {
		m[0][0] = tm.m.v0.x;
		m[0][1] = tm.m.v0.y;
		m[0][2] = tm.m.v0.z;

		m[1][0] = tm.m.v1.x;
		m[1][1] = tm.m.v1.y;
		m[1][2] = tm.m.v1.z;

		m[2][0] = tm.m.v2.x;
		m[2][1] = tm.m.v2.y;
		m[2][2] = tm.m.v2.z;

		v[0] = tm.offs.x;
		v[1] = tm.offs.y;
		v[2] = tm.offs.z;
	}

	float  m[3][3];
	double v[3];
};

PluginWriter::PluginWriter(std::string fname, ExporterSettings::ExportFormat format)
    : m_fileName(std::move(fname))
    , m_file(nullptr)
    , m_buff(4096)
    , m_format(format)
    , m_tryOpen(false)
{
}

bool PluginWriter::doOpen()
{
	m_tryOpen = true;
	if (!m_file) {
		m_file = BLI_fopen(m_fileName.c_str(), "wb");
	}
	return m_file != nullptr;
}

bool PluginWriter::good() const
{
	return !m_tryOpen || (m_tryOpen && (m_file != nullptr) && (ferror(m_file) == 0));
}

std::string PluginWriter::getName() const
{
	return fs::path(m_fileName).filename().string();
}

PluginWriter &PluginWriter::include(std::string name)
{
	if (name != getName() && !name.empty()) {
		// dont include self
		m_includeList.insert(std::move(name));
	}
	return *this;
}

void PluginWriter::flush()
{
	if (!m_includeList.empty()) {
		*this << "\n";
		for (const auto &inc : m_includeList) {
			*this << "#include \"" << inc << "\"\n";
		}
		m_includeList.clear();
	}
	// check m_File ptr not this->good() since we need to flush the buffer
	if (m_file) {
		fflush(m_file);
	}
}

PluginWriter::~PluginWriter()
{
	flush();
	if (m_file) {
		fclose(m_file);
	}
}

PluginWriter &PluginWriter::write(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	int len = vsnprintf(m_buff.data(), m_buff.size(), format, args);
	va_end(args);

	if (len > m_buff.size()) {
		m_buff.resize(len + 1);

		va_start(args, format);
		len = vsnprintf(m_buff.data(), m_buff.size(), format, args);
		va_end(args);
	}

	this->writeData(m_buff.data(), len);

	return *this;
}

PluginWriter &PluginWriter::writeStr(const char *str)
{
	return this->writeData(str, strlen(str));
}

PluginWriter &PluginWriter::writeData(const void *data, int size)
{
	if (doOpen()) {
		fwrite(data, 1, size, m_file);
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
	if (pp.format() == ExporterSettings::ExportFormatASCII) {
		pp << "Transform(" << val.m << "," << val.offs << ")";
	}
	else {
		static char tmBuf[CGR_TRANSFORM_HEX_SIZE];
		TraceTransformHex tm(val);

		const u_int8_t *tm8 = (const u_int8_t*)&tm;
		getStringHex(tm8, sizeof(TraceTransformHex), tmBuf);

		pp << "TransformHex(\"" << tmBuf << "\")";
	}
	return pp;
}

PluginWriter &operator<<(PluginWriter &pp, const AttrPlugin &val)
{
	pp << StripString(val.plugin);
	if (!val.output.empty()) {
		pp << "::" << StripString(val.output);
	}
	return pp;
}

PluginWriter &operator<<(PluginWriter &pp, const AttrMapChannels &val)
{
	pp << "List(\n";

	if (!val.data.empty()) {
		auto iter = val.data.cbegin();
		int index = 0;
		pp << "List(" << index++ << ",\n";
		pp << iter->second.vertices << "," << iter->second.faces << ")";

		for (; iter != val.data.cend(); ++iter) {
			pp << ",\nList(" << index++ << ",\n";
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
