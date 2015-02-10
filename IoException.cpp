///////////////////////////////////////////////////////////////////////////////
//
// Class: IoException
//        Exception for IO errors including an OS message.
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

#include "winlean.h"
#include <cstring>
#include "IoException.h"
#include "Path.h"
#include "Strings.h"

namespace Radyx {

std::array<_TCHAR, 4096> IoException::msg_buffer;

IoException::IoException(const _TCHAR* message, const _TCHAR* path)
{
#ifdef _WIN32
	int err = GetLastError();
#else
	int err = errno;
#endif
	Construct(message, err, path);
}

IoException::IoException(const _TCHAR* message, int os_error, const _TCHAR* path)
{
	Construct(message, os_error, path);
}

void IoException::Construct(const _TCHAR* message, int os_error, const _TCHAR* path)
{
#ifdef _WIN32
	_tcscpy_s(msg_buffer.data(), msg_buffer.size(), message);
	size_t dest = _tcslen(message);
	size_t path_len = _tcslen(path);
	if (path_len >= msg_buffer.size() - dest - kMaxMessageLen) {
		size_t pos = Path::GetNamePos(path);
		path += pos;
		if (path_len - pos >= msg_buffer.size() - dest - kMaxMessageLen) {
			path = _T("");
		}
	}
	dest += _stprintf_s(&msg_buffer[dest],
		msg_buffer.size() - dest,
		_T(" %s : ") + (*path == '\0'),
		path);
	FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		os_error,
		0,
		&msg_buffer[dest],
		static_cast<DWORD>(msg_buffer.size() - dest),
		NULL);
#else
	strncpy(msg_buffer.data(), message, msg_buffer.size());
	size_t dest = strlen(message);
	size_t path_len = strlen(path);
	if (path_len >= msg_buffer.size() - dest - kMaxMessageLen) {
		size_t pos = Path::GetNamePos(path);
		path += pos;
		path_len -= pos;
		if (path_len >= msg_buffer.size() - dest - kMaxMessageLen) {
			path = "";
			path_len = 0;
		}
	}
	dest += sprintf(&msg_buffer[dest],
		" %s : " + (*path == '\0'),
		path);
	strncpy(&msg_buffer[dest], strerror(os_error), msg_buffer.size() - dest);
	msg_buffer.back() = '\0';
#endif
}

const char* IoException::what() const noexcept
{
	return Strings::kFileIoError;
}

const _TCHAR* IoException::Twhat() const noexcept
{
	return msg_buffer.data();
}

const _TCHAR* IoException::GetOsMessage()
{
#ifdef _WIN32
	DWORD length = FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		0,
		msg_buffer.data(),
		static_cast<DWORD>(msg_buffer.size()),
		NULL);
	while (length > 0 && (msg_buffer[length - 1] == '\r' || msg_buffer[length - 1] == '\n')) {
		msg_buffer[--length] = '\0';
	}
	if (length == 0)
	{
		_tcscpy_s(msg_buffer.data(), msg_buffer.size(), Strings::kUnknownError);
	}
	return msg_buffer.data();
#else
	return strerror(errno);
#endif
}

}