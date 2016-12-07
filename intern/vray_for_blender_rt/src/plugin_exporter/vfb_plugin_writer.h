#ifndef VRAY_FOR_BLENDER_PLUGIN_WRITER_FILE_H
#define VRAY_FOR_BLENDER_PLUGIN_WRITER_FILE_H

#include <cstdio>
#include <string>
#include <memory>
#include <atomic>
#include <set>
#include <deque>

#include "vfb_plugin_attrs.h"
#include "vfb_export_settings.h"
#include "vfb_thread_manager.h"

#include "utils/cgr_vrscene.h"
#include "utils/cgr_string.h"

namespace VRayForBlender {

#define VRSCENE_INDENT "    "
class PluginWriter {
public:
	PluginWriter(ThreadManager::Ptr tm, PyObject *pyFile, ExporterSettings::ExportFormat = ExporterSettings::ExportFormatHEX);

	PluginWriter &writeStr(const char *str);

	void setFormat(ExporterSettings::ExportFormat fm) { m_format = fm; }
	ExporterSettings::ExportFormat format() const { return m_format; }

	bool good() const;

	PyObject *getFile() { return m_file; }
	void setAnimationFrame(int frame) { m_animationFrame = frame; }
	int getAnimationFrame() const { return m_animationFrame; }

	bool operator==(const PluginWriter &other) const
	{
		return m_file == other.m_file;
	}

	bool operator!=(const PluginWriter &other) const
	{
		return !(*this == other);
	}

	const char * indent() { ++m_depth; return indentation(); }
	const char * unindent() { --m_depth; return ""; }

	const char * indentation();

	template <typename T>
	struct AsyncZipTask {
		const VRayBaseTypes::AttrList<T> & m_data;

		AsyncZipTask() = delete;
		AsyncZipTask(const AsyncZipTask &) = delete;
		AsyncZipTask & operator=(const AsyncZipTask &) = delete;

		explicit AsyncZipTask(const VRayBaseTypes::AttrList<T> & val): m_data(val) {}
	};

	void addTask(const char * data) {
		processItems(data);
	}

	void addTask(const std::string & data) {
		processItems(data.c_str());
	}

	template <typename T>
	void addTask(const AsyncZipTask<T> &task) {
		// when adding and removing elements from deque no references are invalidated
		const auto & compressData = task.m_data;

		m_items.emplace_back();
		auto & item = m_items.back();

		// get ref to item and copy the data
		// we could get away with reference the task's VRayBaseTypes::AttrList because it is kept in cache
		// and the scene exporter will blockFlushAll to wait for all tasks
		m_threadManager->addTask([&item, compressData, this](std::thread::id id, const volatile bool & stop) {
			char * zipData = GetHex(reinterpret_cast<const u_int8_t *>(*compressData), compressData.getBytesCount());
			item.asyncDone(zipData);
		}, ThreadManager::Priority::LOW);

		processItems();
	}


	void blockFlushAll();
private:
	class WriteItem {
	public:
		bool isDone() const {
			return m_ready.load(std::memory_order_acquire);
		}

		const char * getData() const {
			if (m_isAsync) {
				return m_asyncData;
			} else {
				return m_data.c_str();
			}
		}

		void asyncDone(const char * data) {
			BLI_assert(m_isAsync && "Called asyncDone on sync WriteItem");
			// first copy data into task
			if (data) {
				m_asyncData = data;
				m_freeData = true;
			} else {
				m_asyncData = "";
			}
			// than mark as ready
			// release-aquire order so readers will see m_asyncData changed if m_ready is true
			m_ready.store(true, std::memory_order_release);
		}

		~WriteItem() {
			if (m_freeData) {
				delete[] m_asyncData;
			}
		}

		WriteItem(WriteItem && other): WriteItem() {
			std::swap(m_data, other.m_data);
			std::swap(m_asyncData, other.m_asyncData);
			m_ready = other.m_ready.load();
			other.m_ready = false;
			std::swap(m_freeData, other.m_freeData);
			std::swap(m_isAsync, other.m_isAsync);
		}

		WriteItem(const WriteItem &) = delete;
		WriteItem & operator=(const WriteItem &) = delete;

		WriteItem(): m_data(""), m_ready(false), m_freeData(false), m_isAsync(true) {}
		explicit WriteItem(const std::string & val): m_data(val), m_ready(true), m_freeData(false), m_isAsync(false) {}
		explicit WriteItem(const char * val): m_data(val ? val : ""), m_ready(true), m_freeData(false), m_isAsync(false) {}

	private:
		std::string        m_data;
		const char        *m_asyncData;
		std::atomic<bool>  m_ready;
		bool               m_freeData;
		bool               m_isAsync;
	};

	void processItems(const char * val = nullptr);

	// only used to syncronize waiting for items
	std::mutex                      m_itemMutex;
	// used to block on blockFlushAll
	std::condition_variable         m_itemDoneVar;
	std::deque<WriteItem>           m_items;
	ThreadManager::Ptr              m_threadManager;
	int                             m_depth;
	int                             m_animationFrame;
	PyObject                       *m_file;
	ExporterSettings::ExportFormat  m_format;

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
PluginWriter &operator<<(PluginWriter &pp, const PluginWriter::AsyncZipTask<T> &task)
{
	pp.addTask(task);
	return pp;
}

template <typename T>
using KVPair = std::pair<std::string, T>;


template <typename T>
PluginWriter &operator<<(PluginWriter &pp, const KVPair<T> &val)
{
	if (pp.getAnimationFrame() == -1) {
		return pp << pp.indent() << val.first << "=" << val.second << ";\n" << pp.unindent();
	} else {
		return pp << pp.indent() << val.first << "=interpolate((" << pp.getAnimationFrame() << "," << val.second << "));\n" << pp.unindent();
	}
}

template <> inline
PluginWriter &operator<<(PluginWriter &pp, const KVPair<std::string> &val)
{
	return pp << pp.indent() << val.first << "=\"" << val.second << "\";\n" << pp.unindent();
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
			pp << "(\n" << pp.indent() << (*val)[0];
			for (int c = 1; c < val.getCount(); c++) {
				pp << ",";
				if (newLine) {
					pp << "\n" << pp.indentation();
				} else {
					pp << " ";
				}
				pp << (*val)[c];
			}
			pp.unindent();
			pp << "\n" << pp.indentation() << ")";
		} else if (pp.format() == ExporterSettings::ExportFormatZIP) {
			pp << "Hex(\"" << PluginWriter::AsyncZipTask<T>(val) << "\")";
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
		pp << "(\n" << pp.indent() << "\"" << StripString((*val)[0]) << "\"";
		for (int c = 1; c < val.getCount(); c++) {
			pp << ",";
			if (newLine) {
				pp << "\n" << pp.indentation();
			} else {
				pp << " ";
			}
			pp <<"\"" << StripString((*val)[c]) << "\"";
		}
		pp.unindent();
		pp << "\n" << pp.indentation() << ")";
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
