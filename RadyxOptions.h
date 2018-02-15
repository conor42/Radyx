///////////////////////////////////////////////////////////////////////////////
//
// Class: RadyxOptions
//        Compression and archiving options, and the list of file specs to add
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

#ifndef RADYX_OPTIONS_H
#define RADYX_OPTIONS_H

#include <string>
#include <list>
#ifndef _WIN32
#include <limits.h>
#include <cstdlib>
#endif
#include "CharType.h"
#include "Path.h"
#include "OptionalSetting.h"
#include "Lzma2Options.h"

namespace Radyx {

class ArchiveCompressor;

class RadyxOptions
{
public:
	enum Command
	{
		kAdd,
		kExtract,
		kTest,
		kList
	};

	enum Recurse
	{
		kRecurseNone,
		kRecurseWildcard,
		kRecurseAll
	};

	struct FileSpec
	{
		Path path;
		size_t root;
		size_t name;
		bool recurse;
		FileSpec() {}
		FileSpec(const _TCHAR* path_, Recurse recurse_);
		void SetFullPath(const _TCHAR* full_path);
	};

	class InvalidParameter
	{
	public:
		InvalidParameter(const _TCHAR* arg) : arg_error(arg) {}

		const _TCHAR* ArgError() const {
			return arg_error;
		}

	private:
		const _TCHAR* arg_error;
	};

	RadyxOptions(int argc, _TCHAR* argv[], Path& archive_path);
	void GetFiles(ArchiveCompressor& arch_comp);

	std::list<FileSpec> file_specs;
	std::list<FileSpec> exclusions;
	FsString working_dir;
	Command command;
	Recurse default_recurse;
	bool share_deny_none;
	bool store_full_paths;
//	bool yes_to_all;
	bool multi_thread;
	unsigned thread_count;
	bool solid_by_extension;
	uint_least64_t solid_unit_size;
	uint_fast32_t solid_file_count;
	Lzma2Options lzma2;
	bool bcj_filter;
	bool async_read;
	bool store_creation_time;
	bool quiet_mode;

private:
	static const unsigned kRandomFilterDefault = 10;
#ifdef _WIN32 
	static const unsigned kMaxPath = 32767;
#else
	static const unsigned kMaxPath = PATH_MAX;
#endif

	void ParseCommand(int argc, _TCHAR* argv[]);
	void ReadFileList(const _TCHAR* path, Recurse recurse, std::list<FileSpec>& spec_list);
	void ReadFileListW(const _TCHAR* path, Recurse recurse, std::list<FileSpec>& spec_list);
	void ParseArg(const _TCHAR* arg);
	bool SearchExclusions(const Path& path, size_t root, bool is_root) const;
	void HandleFilenames(const _TCHAR* arg, std::list<FileSpec>& spec_list);
	void HandleCompressionMethod(const _TCHAR* arg);
	void HandleSolidMode(const _TCHAR* arg);
	Recurse HandleRecurse(const _TCHAR*& arg);
	void Handle_ss(const _TCHAR* arg);
	int CheckOnOff(const _TCHAR* arg) const;
	inline unsigned long ReadDecimal(const _TCHAR* arg, _TCHAR*& end) const;
	unsigned ReadSimpleNumericParam(const _TCHAR* arg, unsigned min, unsigned max) const;
	uint_least64_t ApplyMultiplier(const _TCHAR* arg, unsigned value) const;
	void LoadFullPaths();
	void SearchDir(std::list<FileSpec>::const_iterator it_first,
		std::list<FileSpec>::const_iterator it_end,
		ArchiveCompressor& arch_comp) const;
	void SearchDir(Path& dir,
		std::list<FileSpec>::const_iterator it_first,
		std::list<FileSpec>::const_iterator it_end,
		bool all_match,
		bool recurse,
		bool is_root,
		ArchiveCompressor& arch_comp) const;
	bool IsMatch(std::list<FileSpec>::const_iterator it_first,
		std::list<FileSpec>::const_iterator it_end,
		const _TCHAR* name,
		bool is_root) const;
};

unsigned long RadyxOptions::ReadDecimal(const _TCHAR* arg, _TCHAR*& end) const
{
	return _tcstoul(arg, &end, 10);
}

}

#endif // RADYX_OPTIONS_H