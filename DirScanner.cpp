///////////////////////////////////////////////////////////////////////////////
//
// Class:   DirScanner
//          Wrapper for directory searching
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

#include "DirScanner.h"

namespace Radyx {

#ifdef _WIN32

DirScanner::DirScanner(const Path& path)
{
	if (path.length() >= MAX_PATH && !path.IsExtendedLength()) {
		Path long_path;
		long_path.SetExtendedLength(path);
		handle = FindFirstFile(long_path.c_str(), &wfd);
	}
	else {
		handle = FindFirstFile(path.c_str(), &wfd);
	}
}

DirScanner::~DirScanner()
{
	if (handle != INVALID_HANDLE_VALUE) FindClose(handle);
}

#else

DirScanner::DirScanner(const Path& path)
	: dir(NULL)
{
	dir = opendir(path.c_str());
	if(dir != NULL) {
		Next();
	}
}

DirScanner::~DirScanner()
{
	if (dir != NULL) closedir(dir);
}

#endif // _WIN32

}
