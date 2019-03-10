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

#ifndef RADYX_OUTPUT_FILE_H
#define RADYX_OUTPUT_FILE_H

#include <ios>

#include "common.h"

#ifdef _WIN32

#include "winlean.h"
#include "CharType.h"

namespace Radyx {
	
class OutputFile
{
public:
	OutputFile();
	explicit OutputFile(const _TCHAR* filename);
	virtual ~OutputFile();
	void open(const _TCHAR* filename, bool no_caching = false);
	OutputFile& put(char c);
	OutputFile& write(const char* s, size_t n);
	uint_least64_t tellp();
	OutputFile& seekp(uint_least64_t pos);
    void close();
	std::ios_base::iostate exceptions() const;
	void exceptions(std::ios_base::iostate except);
	bool fail() const;

private:
	void AddError(std::ios_base::iostate error);

	HANDLE handle;
	std::ios_base::iostate error_state;
	std::ios_base::iostate exception_flags;
};

typedef OutputFile OutputStream;

}

#else 

#include <ostream>

namespace Radyx {

typedef std::ostream OutputStream;
typedef std::ofstream OutputFile;

}

#endif // _WIN32

#endif // RADYX_OUTPUT_FILE_H