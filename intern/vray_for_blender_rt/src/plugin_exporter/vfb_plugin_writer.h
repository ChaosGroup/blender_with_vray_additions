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

	/// Wrapper to carry data needed for async zip task
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

	/// Add task to the queue
	template <typename T>
	void addTask(const AsyncZipTask<T> &task) {
		// when adding and removing elements from deque no references are invalidated
		const auto & compressData = task.m_data;

		m_items.emplace_back();
		auto & item = m_items.back();

		// Array's data is actually shared_ptr so copy it inside to preserve the data
		m_threadManager->addTask([&item, compressData, this](int, const volatile bool &) {
			char * zipData = GetStringZip(reinterpret_cast<const u_int8_t *>(*compressData), compressData.getBytesCount());
			item.asyncDone(zipData);
		}, ThreadManager::Priority::LOW);

		processItems();
	}

	/// Block until all items are done and wirtten to file
	void blockFlushAll();
private:
	class WriteItem {
	public:
		/// Check if this write item is done (may be zipping currently)
		bool isDone() const;

		/// Get the data of this item
		const char * getData() const;

		/// Mark this item as done and store the pointer provided
		void asyncDone(const char * data);

		/// Release any data set in asycnDone
		~WriteItem();

		WriteItem(WriteItem && other);
		WriteItem(const WriteItem &) = delete;
		WriteItem & operator=(const WriteItem &) = delete;

		/// Create item that is async task, which will be completed in some point in future
		WriteItem()
			: m_data("")
			, m_asyncData(nullptr)
			, m_ready(false)
			, m_freeData(false)
			, m_isAsync(true) {}

		/// Create item that is done and has some value
		explicit WriteItem(const std::string & val)
			: m_data(val)
			, m_asyncData(nullptr)
			, m_ready(true)
			, m_freeData(false)
			, m_isAsync(false) {}

		/// Create item that is done and has some value
		explicit WriteItem(const char * val)
			: m_data(val ? val : "")
			, m_asyncData(nullptr)
			, m_ready(true)
			, m_freeData(false)
			, m_isAsync(false) {}

	private:
		std::string        m_data; ///< Data if this item is created 'done'
		const char        *m_asyncData; ///< Data if this item is asyncly zipped
		std::atomic<bool>  m_ready; ///< Flag to check if this item is ready
		bool               m_freeData; ///< True if data needs to be freed
		bool               m_isAsync; ///< True if this item is async
	};

	/// Process any pending items
	/// 1. write all completed items to file
	/// 2. write val if queue is empty or add it to the queue
	void processItems(const char * val = nullptr);

	std::mutex                      m_itemMutex; //< only used to syncronize waiting for items
	std::condition_variable         m_itemDoneVar; ///< used to block on blockFlushAll
	std::deque<WriteItem>           m_items; ///< Item queue for all items to be writen to files
	ThreadManager::Ptr              m_threadManager; ///< Thread manager for async items
	int                             m_depth; ///< Current indentation depth
	int                             m_animationFrame; ///< The current animation frame
	PyObject                       *m_file; ///< The file object coming from python api
	ExporterSettings::ExportFormat  m_format; ///< The file format (ASCII, HEX, ZIP)

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
