///////////////////////////////////////////////////////////////////////////////
//
// Class:   ArchiveCompressor
//          Reads input files into the unit compressor and collects
//          information for the archive database
//
// Copyright 2015 Conor McCarthy
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

#include <list>
#include "common.h"
#include "Path.h"
#include "ThreadPool.h"
#include "OptionalSetting.h"
#include "UnitCompressor.h"
#include "CompressorInterface.h"
#include "Crc32.h"

namespace Radyx {

class RadyxOptions;

class ArchiveCompressor
{
public:
	struct FileInfo
	{
		Path path;
		size_t name;
		size_t root;
		size_t ext;
		uint_least64_t size;
		OptionalSetting<uint_least64_t> creat_time;
		OptionalSetting<uint_least64_t> mod_time;
		OptionalSetting<uint_fast32_t> attributes;
		unsigned ext_group;
		Crc32 crc32;
		FileInfo(const _TCHAR* path_, size_t root_, uint_least64_t size_)
			: path(path_),
			name(path.GetNamePos()),
			root(root_),
			ext(name + Path::GetExtPos(path.c_str() + name)),
			size(size_),
			creat_time(0),
			mod_time(0),
			attributes(0),
			ext_group(GetExtensionIndex(path_ + ext)) {}
		bool IsEmpty() const { return size == 0; }
		FileInfo& operator=(const FileInfo&) = delete;
	};

	struct DataUnit
	{
		uint_least64_t out_file_pos;
		uint_least64_t unpack_size;
		uint_least64_t pack_size;
		uint_least64_t file_count;
		CoderInfo coder_info;
		CoderInfo bcj_info;
		std::list<FileInfo>::const_iterator in_file_first;
		std::list<FileInfo>::const_iterator in_file_last;
		bool used_bcj;
		DataUnit()
			: out_file_pos(0),
			unpack_size(0),
			pack_size(0),
			file_count(0),
			used_bcj(false) {}
	};

	class FileReader
	{
	public:
		FileReader(Path& path, bool share_deny_none);
		~FileReader();
		inline bool IsValid() const;
		inline bool Read(void* buffer, uint_fast32_t byte_count, unsigned long& bytes_read);
		void GetAttributes(FileInfo& fi, bool get_creation_time);
	private:
#ifdef _WIN32
		HANDLE handle;
#else
		int fd;
#endif
		FileReader(const FileReader&) = delete;
		FileReader& operator=(const FileReader&) = delete;
	};

	ArchiveCompressor();
	inline void Add(const _TCHAR* path, size_t root, uint_least64_t size);
	uint_least64_t Compress(UnitCompressor& unit_comp,
		CompressorInterface& compressor,
		const RadyxOptions& options,
		ThreadPool& threads,
		std::ostream& out_stream);
	const std::list<FileInfo>& GetFileList() const { return file_list; }
	const std::list<DataUnit>& GetUnitList() const { return unit_list; }
	size_t GetEmptyFileCount() const;
	size_t GetNameLengthTotal() const;

private:
	static const _TCHAR extensions[];

	void EliminateDuplicates();
	bool AddFile(FileInfo& fi,
		UnitCompressor& unit_comp,
		CompressorInterface& compressor,
		const RadyxOptions& options,
		ThreadPool& threads,
		Progress& progress,
		std::ostream& out_stream);
	static unsigned GetExtensionIndex(const _TCHAR* ext);

	std::list<FileInfo> file_list;
	std::list<DataUnit> unit_list;
	uint_least64_t initial_total_bytes;

	ArchiveCompressor(const ArchiveCompressor&) = delete;
	ArchiveCompressor& operator=(const ArchiveCompressor&) = delete;
};

#ifdef _WIN32

bool ArchiveCompressor::FileReader::IsValid() const
{
	return handle != INVALID_HANDLE_VALUE;
}

bool ArchiveCompressor::FileReader::Read(void* buffer, uint_fast32_t byte_count, unsigned long& bytes_read)
{
	return ReadFile(handle, buffer, byte_count, &bytes_read, NULL) != 0;
}

#else

#include <unistd.h>

bool ArchiveCompressor::FileReader::IsValid() const
{
	return fd >= 0;
}

bool ArchiveCompressor::FileReader::Read(void* buffer, uint_fast32_t byte_count, unsigned long& bytes_read)
{
	ssize_t nread = read(fd, buffer, byte_count);
	if (nread < 0) {
		return false;
	}
	else {
		bytes_read = static_cast<unsigned long>(nread);
		return true;
	}
}

#endif

void ArchiveCompressor::Add(const _TCHAR* path, size_t root, uint_least64_t size)
{
	file_list.push_back(FileInfo(path, root, size));
	initial_total_bytes += size;
}

}
#endif // RADYX_ARCHIVE_COMPRESSOR_H