///////////////////////////////////////////////////////////////////////////////
//
// Class:   ArchiveCompressor
//          Reads input files sequentially into a buffer
//
// Copyright 2015-present Conor McCarthy
//
// This file is part of Radyx.
//
// Radyx is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Radyx is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Radyx. If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef RADYX_ARCHIVE_COMPRESSOR_H
#define RADYX_ARCHIVE_COMPRESSOR_H

#include "common.h"
#include <list>
#include <unordered_set>
#include "ArchiveStreamIn.h"
#include "OptionalSetting.h"
#include "CoderInfo.h"
#include "InPlaceFilter.h"
#include "OutputFile.h"
#include "Path.h"
#include "RadyxOptions.h"
#include "Crc32.h"
#include "Progress.h"
#include "Strings.h"

namespace Radyx {

class ArchiveCompressor : public ArchiveStreamIn
{
public:
	struct FileInfo
	{
		const Path& dir;
		Path name;
		size_t root;
		size_t ext;
		uint_least64_t size;
		OptionalSetting<uint_least64_t> creat_time;
		OptionalSetting<uint_least64_t> mod_time;
		OptionalSetting<uint_fast32_t> attributes;
		unsigned ext_index;
		Crc32 crc32;
		FileInfo(const Path& path_, const _TCHAR* name_, size_t root_, uint_least64_t size_)
			: dir(path_),
			name(name_),
			root(root_),
			ext(Path::GetExtPos(name_)),
			size(size_),
			creat_time(0),
			mod_time(0),
			attributes(0),
			ext_index(GetExtensionIndex(name_ + ext)) {}
		bool IsEmpty() const {
			return size == 0;
		}
	};

	struct DataUnit
	{
		uint_least64_t out_file_pos;
		uint_least64_t unpack_size;
		uint_least64_t pack_size;
		uint_least64_t file_count;
		std::list<CoderInfo> coder_info;
		std::list<FileInfo>::const_iterator in_file_first;
		std::list<FileInfo>::const_iterator in_file_last;
		DataUnit()
			: out_file_pos(0),
			unpack_size(0),
			pack_size(0),
			file_count(0) {}
		void Reset() {
			file_count = 0;
			unpack_size = 0;
			coder_info.clear();
		}
	};

	class FileReader
	{
	public:
		FileReader()
#ifdef _WIN32
			: handle(INVALID_HANDLE_VALUE)
#else
			: fd(-1)
#endif
		{}
		FileReader(const FileInfo& fi, bool share_deny_none) {
			Open(fi, share_deny_none);
		}
		~FileReader();
		bool Open(const FileInfo& fi, bool share_deny_none);
		bool IsValid() const;
		bool Read(void* buffer, uint_fast32_t byte_count, unsigned long& bytes_read);
		void GetAttributes(FileInfo& fi, bool get_creation_time);
		void Close();
	private:
#ifdef _WIN32
		HANDLE handle;
#else
		int fd;
#endif
		FileReader(const FileReader&) = delete;
		FileReader& operator=(const FileReader&) = delete;
		FileReader(FileReader&&) = delete;
		FileReader& operator=(FileReader&&) = delete;
	};

	ArchiveCompressor(const RadyxOptions& options, Progress& progress);

	void Add(const _TCHAR* path, size_t root, uint_least64_t size);
	void PrepareList();
	const std::list<FileInfo>& GetFileList() const {
		return file_list;
	}
	const std::list<DataUnit>& GetUnitList() const {
		return unit_list;
	}
	size_t GetFileCount() const {
		return file_list.size();
	}
	size_t GetEmptyFileCount() const;
	size_t GetNameLengthTotal() const;
	uint_least64_t GetTotalByteCount() const noexcept {
		return initial_total_bytes;
	}
	void InitUnit(OutputFile& out_stream);
	bool IsExeUnit() const noexcept {
		return ext_index >= exe_group;
	}
	uint_least64_t FinalizeUnit(std::list<CoderInfo>& coder_info, OutputFile& out_stream);
	size_t Read(uint8_t* const buffer, size_t const length);
	bool Complete() const noexcept {
		return cur_file == file_list.end();
	}

private:
	static const _TCHAR extensions[];

	void EliminateDuplicates();
	void DetectCollisions();
	bool AddFile(uint8_t* buffer, size_t length, unsigned long& read_count);

	static unsigned GetExtensionIndex(const _TCHAR* ext);

	std::list<FileInfo> file_list;
	std::list<DataUnit> unit_list;
	std::unordered_set<Path, std::hash<FsString>> path_set;
	std::list<FsString> file_warnings;
	std::list<FileInfo>::iterator cur_file;
	const RadyxOptions& options;
	Progress& progress;
	DataUnit unit;
	uint_least64_t initial_size;
	uint_least64_t initial_total_bytes;
	FileReader reader;
	unsigned exe_group;
	unsigned ext_index;

	ArchiveCompressor(const ArchiveCompressor&) = delete;
	ArchiveCompressor& operator=(const ArchiveCompressor&) = delete;
	ArchiveCompressor(ArchiveCompressor&&) = delete;
	ArchiveCompressor& operator=(ArchiveCompressor&&) = delete;
};

} // Radyx

#endif