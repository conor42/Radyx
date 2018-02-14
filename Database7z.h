#ifndef RADYX_DATABASE_7Z_H
#define RADYX_DATABASE_7Z_H

#include "common.h"
#include <list>
#include <vector>
#include <utility>
#include "CoderInfo.h"
#include "OptionalSetting.h"
#include "Path.h"

namespace Radyx {

class Database7z
{
	struct FolderCoderInfo
	{
		CoderInfo coder_info;
		uint_least64_t unpack_size;
	};

	struct SubStreamInfo
	{
		uint_least64_t unpack_size;
		uint_fast32_t crc32;
	};

	struct Folder
	{
		std::vector<FolderCoderInfo> coders_info;
		std::vector<std::pair<uint_least64_t, uint_least64_t>> bind_pairs;
		uint_fast32_t crc32;
		std::vector<SubStreamInfo> substreams_info;
	};

	struct StreamsInfo
	{
		uint_least64_t file_pos;
		std::vector<uint_least64_t> pack_sizes;
		std::vector<uint_fast32_t> crc32s;
		std::vector<Folder> folders;
	};

	struct FileInfo
	{
		FsString name;
		uint_least64_t size;
		OptionalSetting<uint_least64_t> creat_time;
		OptionalSetting<uint_least64_t> mod_time;
		OptionalSetting<uint_fast32_t> attributes;
		uint_fast32_t crc32;
		bool empty;
	};
};

}

#endif