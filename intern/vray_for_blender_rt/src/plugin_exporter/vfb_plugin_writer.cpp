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

PluginWriter::PluginWriter(ThreadManager::Ptr tm, PyObject *pyFile, ExporterSettings::ExportFormat format)
	: m_threadManager(tm)
    , m_file(pyFile)
    , m_format(format)
    , m_animationFrame(-1)
    , m_depth(1)
{
}

bool PluginWriter::good() const
{
	return m_file != nullptr;
}

#define FormatAndAdd(pp, ...)                                     \
	char buf[2048];                                               \
	sprintf(buf, __VA_ARGS__);                                    \
	pp.writeStr(buf);                                             \
	return pp;                                                    \


PluginWriter &PluginWriter::writeStr(const char *str)
{
	if (good()) {
		addTask(str);
	}
	return *this;
}

void PluginWriter::processItems(const char * val)
{
	// this function will not be called concurrently
	// so it is safe to traverse m_items and expect not to change during execution

	const int maxRep = m_items.size();
	for (int c = 0; c < maxRep && m_items.front().isDone(); ++c) {
		PyObject_CallMethod(m_file, _C("write"), _C("s"), m_items.front().getData());
		m_items.pop_front();
	}

	if (val && *val) {
		if (m_items.empty()) {
			// no items left in que, just write current value
			PyObject_CallMethod(m_file, _C("write"), _C("s"), val);
		} else {
			m_items.push_back(WriteItem(val));
		}
	}
}


void PluginWriter::blockFlushAll()
{
	for (int c = 0; c < m_items.size(); ++c) {
		auto & item = m_items[c];
		// lazy wait for item
		while (!item.isDone()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		PyObject_CallMethod(m_file, _C("write"), _C("s"), item.getData());
	}

	m_items.clear();
}

const char * PluginWriter::indentation()
{
	switch (m_depth) {
	case 0: return "";
	case 1: return VRSCENE_INDENT;
	case 2: return VRSCENE_INDENT VRSCENE_INDENT;
	case 3: return VRSCENE_INDENT VRSCENE_INDENT VRSCENE_INDENT;
	case 4: return VRSCENE_INDENT VRSCENE_INDENT VRSCENE_INDENT VRSCENE_INDENT;
	default: return "";
	}
}

PluginWriter &operator<<(PluginWriter &pp, const int &val)
{
	FormatAndAdd(pp, "%d", val);
}

PluginWriter &operator<<(PluginWriter &pp, const float &val)
{
	FormatAndAdd(pp, "%g", val);
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
	FormatAndAdd(pp, "Color(%g,%g,%g)", val.r, val.g, val.b);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrAColor &val)
{
	FormatAndAdd(pp, "AColor(%g,%g,%g,%g)", val.color.r, val.color.g, val.color.b, val.alpha);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrVector &val)
{
	FormatAndAdd(pp, "Vector(%g,%g,%g)", val.x, val.y, val.z);
}

PluginWriter &operator<<(PluginWriter &pp, const AttrVector2 &val)
{
	FormatAndAdd(pp, "Vector(%g,%g,0)", val.x, val.y);
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

		for (++iter; iter != val.data.cend(); ++iter) {
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
		pp << ",\n" << pp.indent() << "List(" << (*val.data)[0].index << ", " << (*val.data)[0].tm << ", " << (*val.data)[0].vel << "," << (*val.data)[0].node << ")";
		for (int c = 1; c < val.data.getCount(); c++) {
			const auto &item = (*val.data)[c];
			pp << ",\n" << pp.indentation() << "List(" << item.index << ", " << item.tm << ", " << item.vel << "," << item.node << ")";
		}
		pp.unindent();
		pp << pp.indentation();
	}

	return pp << "\n" << pp.indentation() << ")";
}

} // VRayForBlender
