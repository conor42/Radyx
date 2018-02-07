///////////////////////////////////////////////////////////////////////////////
//
// Class: IoException
//        Exception for IO errors including an OS message.
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

#ifndef RADYX_IO_EXCEPTION_H
#define RADYX_IO_EXCEPTION_H

#include <array>
#include "common.h"
#include "CharType.h"

namespace Radyx {

class IoException : public std::exception
{
public:
	IoException() {}
	IoException(const _TCHAR* message, const _TCHAR* path);
	IoException(const _TCHAR* message, int os_error, const _TCHAR* path);
	const char* what() const noexcept;
	const _TCHAR* Twhat() const noexcept;
	static const _TCHAR* GetOsMessage();

private:
	static const size_t kMaxMessageLen = 160;

	void Construct(const _TCHAR* message, int os_error, const _TCHAR* path);

	static std::array<_TCHAR, 4096> msg_buffer;
};

}

#endif // RADYX_IO_EXCEPTION_H