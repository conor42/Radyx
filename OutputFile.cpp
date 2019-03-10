///////////////////////////////////////////////////////////////////////////////
//
// Class: OutputFile
//        Provides greater control over file opening / writing
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

#include <algorithm>
#include "OutputFile.h"

namespace Radyx {

#ifdef _WIN32

OutputFile::OutputFile()
	: handle(INVALID_HANDLE_VALUE),
	error_state(std::ios_base::goodbit)
{
}


OutputFile::~OutputFile()
{
    close();
}

OutputFile::OutputFile(const _TCHAR* filename)
{
	open(filename);
}

void OutputFile::open(const _TCHAR* filename, bool no_caching)
{
	if (handle != INVALID_HANDLE_VALUE) {
		CloseHandle(handle);
	}
	handle = CreateFile(filename,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | (no_caching ? FILE_FLAG_WRITE_THROUGH : 0),
		NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		AddError(std::ios_base::failbit);
	}
}

OutputFile& OutputFile::put(char c)
{
	return write(&c, 1);
}

OutputFile& OutputFile::write(const char* s, size_t n)
{
	while (!g_break && n != 0) {
		// Avoid possible errors from large writes over a network in WinXP
		DWORD to_write = static_cast<DWORD>(std::min(n, size_t(32) * 1024 * 1024 - 32768));
		DWORD written;
		if (WriteFile(handle, s, to_write, &written, NULL) == FALSE) {
			AddError(std::ios_base::badbit);
			break;
		}
		n -= written;
		s += written;
	}
	return *this;
}

uint_least64_t OutputFile::tellp()
{
	LARGE_INTEGER li;
	li.QuadPart = 0;
	if (SetFilePointerEx(handle, li, &li, FILE_CURRENT) == FALSE) {
		AddError(std::ios_base::badbit);
	}
	return li.QuadPart;
}

OutputFile& OutputFile::seekp(uint_least64_t pos)
{
	LARGE_INTEGER li;
	li.QuadPart = pos;
	if (SetFilePointerEx(handle, li, NULL, FILE_BEGIN) == FALSE) {
		AddError(std::ios_base::badbit);
	}
	return *this;
}

void OutputFile::close()
{
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

std::ios::iostate OutputFile::exceptions() const
{
	return exception_flags;
}

void OutputFile::exceptions(std::ios_base::iostate except)
{
	exception_flags = except;
	error_state = std::ios_base::goodbit;
}

bool OutputFile::fail() const
{
	return (error_state & (std::ios_base::badbit | std::ios_base::failbit)) != 0;
}

void OutputFile::AddError(std::ios_base::iostate error)
{
	error_state |= error;
	if ((exception_flags & error_state) != 0) {
		throw std::ios_base::failure("");
	}
}

#endif // _WIN32

}