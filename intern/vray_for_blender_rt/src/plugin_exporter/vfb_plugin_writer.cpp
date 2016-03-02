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

PluginWriter::PluginWriter(PyObject *pyFile, ExporterSettings::ExportFormat format)
    : m_file(pyFile)
    , m_format(format)
    , animationFrame(-1)
{
}

bool PluginWriter::good() const
{
	return m_file != nullptr;
}

#define PyPrintf(pp, ...)                                         \
	if (!pp.good()) {                                             \
		return pp;                                                \
	}                                                             \
	char buf[2048];                                               \
	sprintf(buf, __VA_ARGS__);                                    \
	PyObject_CallMethod(pp.getFile(), _C("write"), _C("s"), buf); \
	return pp;                                                    \


PluginWriter &PluginWriter::writeStr(const char *str)
{
	if (good()) {
		PyObject_CallMethod(m_file, _C("write"), _C("s"), str);
	}
	return *this;
}

PluginWriter &operator<<(PluginWriter &pp, const int &val)
{
	PyPrintf(pp, "%d", val);
}

PluginWriter &operator<<(PluginWriter &pp, const float &val)
{
	PyPrintf(pp, "%g", val);
}

PluginWriter &operator<<(PluginWriter &pp, const char *val)
{
	return pp.writeStr(val);
}

PluginWriter &operator<<(PluginWriter &pp, const std::string &val)
{
	return pp.writeStr(val.c_str());
}

PluginWriter &operator<<(PluginWriter &pp, const AttrColor &val)
{
	PyPrintf(pp, "Color(%g, %g, %g)", val.r, val.g, val.b);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrAColor &val)
{
	PyPrintf(pp, "AColor(%g, %g, %g, %g)", val.color.r, val.color.g, val.color.b, val.alpha);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrVector &val)
{
	PyPrintf(pp, "Vector(%g, %g, %g)", val.x, val.y, val.z);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrVector2 &val)
{
	PyPrintf(pp, "Vector2(%g, %g)", val.x, val.y);
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
