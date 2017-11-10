///////////////////////////////////////////////////////////////////////////////
//
// Class: OutputFile
//        Provides greater control over file opening / writing
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

#ifndef RADYX_OUTPUT_FILE_H
#define RADYX_OUTPUT_FILE_H

#include "common.h"
#include "OutputStream.h"

#ifdef _WIN32
#include "winlean.h"
#include "CharType.h"

namespace Radyx {

class OutputFile : public OutputStream
{
public:
	OutputFile() noexcept;
	explicit OutputFile(const _TCHAR* filename);
	virtual ~OutputFile();
	void open(const _TCHAR* filename, bool no_caching = false);
	OutputFile& put(char c);
	OutputFile& write(const char* s, size_t n);
	uint_least64_t tellp();
	OutputFile& seekp(uint_least64_t pos);
	std::ios_base::iostate exceptions() const noexcept;
	void exceptions(std::ios_base::iostate except) noexcept;
	bool fail() const noexcept;

	OutputStream& Put(char c);
	OutputStream& Write(const char* s, size_t n);
	void DisableExceptions();
	void RestoreExceptions();
	bool Fail() const noexcept;

private:
	void AddError(std::ios_base::iostate error);

	HANDLE handle;
	std::ios_base::iostate error_state;
	std::ios_base::iostate exception_flags;
	std::ios_base::iostate saved_exception_flags;

	OutputFile(const OutputFile&) = delete;
	OutputFile& operator=(const OutputFile&) = delete;
	OutputFile(OutputFile&&) = delete;
	OutputFile& operator=(OutputFile&&) = delete;
};

} // Radyx

#else 

#include <ostream>

typedef std::ofstream OutputFile;

#endif // _WIN32

#endif // RADYX_OUTPUT_FILE_H