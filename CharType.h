///////////////////////////////////////////////////////////////////////////////
//
// Definitions for character portability         
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

#ifndef RADYX_CHAR_TYPE
#define RADYX_CHAR_TYPE

#include <string>

#ifdef _UNICODE

#include <tchar.h>

namespace Radyx {
	typedef std::wstring FsString;
}

#define Tcerr wcerr

#else

namespace Radyx {
	typedef char _TCHAR;
	typedef std::string FsString;
}
#define Tcerr cerr
#define _tmain main
#define _tchdir chdir
#define _tcslen strlen
#define _tcschr strchr
#define _tcsrchr strrchr
#define _tcsicmp strcasecmp
#define _tcstoul strtoul
#define _tremove remove
#define _T(x) x

#endif // _UNICODE

#endif // RADYX_CHAR_TYPE