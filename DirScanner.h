///////////////////////////////////////////////////////////////////////////////
//
// Class:   DirScanner
//          Wrapper for directory searching
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

#ifndef RADYX_DIR_SCANNER_H
#define RADYX_DIR_SCANNER_H

#include "common.h"
#ifdef _WIN32
#include "winlean.h"
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif
#include "CharType.h"
#include "Path.h"

namespace Radyx {

class DirScanner
{
public:
	DirScanner(const Path& path);
	~DirScanner();
	inline bool NotFound() const;
	inline const _TCHAR* GetName() const;
	inline bool IsDirectory() const;
	inline uint_least64_t GetFileSize() const;
	inline bool Next();

private:
#ifdef _WIN32
	HANDLE handle;
	WIN32_FIND_DATA wfd;
#else
	DIR* dir;
	dirent* ent;
#endif

	DirScanner(const DirScanner&) = delete;
	DirScanner& operator=(const DirScanner&) = delete;
};

#ifdef _WIN32

bool DirScanner::NotFound() const
{
	return handle == INVALID_HANDLE_VALUE;
}

const _TCHAR* DirScanner::GetName() const
{
	return wfd.cFileName;
}

bool DirScanner::IsDirectory() const
{
	return (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

uint_least64_t DirScanner::GetFileSize() const
{
	return (static_cast<uint_least64_t>(wfd.nFileSizeHigh) << 32) + wfd.nFileSizeLow;
}

bool DirScanner::Next()
{
	return FindNextFile(handle, &wfd) != 0;
}

#else

bool DirScanner::NotFound() const
{
	return dir == NULL;
}

const _TCHAR* DirScanner::GetName() const
{
	return ent->d_name;
}

bool DirScanner::IsDirectory() const
{
	return ent->d_type == DT_DIR;
}

uint_least64_t DirScanner::GetFileSize() const
{
	struct stat s;
	if (stat(ent->d_name, &s) == 0) {
		return s.st_size;
	}
	return 0;
}

bool DirScanner::Next()
{
	ent = readdir(dir);
	return ent != NULL;
}

#endif // _WIN32

}

#endif // RADYX_DIR_SCANNER_H