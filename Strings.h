///////////////////////////////////////////////////////////////////////////////
//
// Class: Strings
//        Message strings
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

#ifndef RADYX_STRINGS_H
#define RADYX_STRINGS_H

namespace Radyx {

class Strings
{
public:
	static const _TCHAR kExtractionUnsupported[];
	static const _TCHAR kNoCommandSpecified[];
	static const _TCHAR kLcLpNoGreaterThan4[];
	static const _TCHAR kSearching[];
	static const _TCHAR kAdding_[];
	static const _TCHAR kCannotOpen_[];
	static const _TCHAR kCannotRead_[];
	static const _TCHAR kWarningsForFiles[];
	static const char kFileIoError[];
	static const _TCHAR kWarningCouldntOpen_[];
	static const _TCHAR kNoFilesFound[];
	static const _TCHAR kUnrecoverableErrorReading[];
	static const _TCHAR kCannotChDir[];
	static const _TCHAR kFound_[];
	static const _TCHAR k_file[];
	static const _TCHAR k_files[];
	static const _TCHAR kUnableConvertUtf8to16[];
	static const _TCHAR kCannotWriteArchive[];
	static const _TCHAR kArchiveFileExists[];
	static const _TCHAR kCannotCreateArchive[];
	static const _TCHAR kErrorCol_[];
	static const _TCHAR kErrorNotEnoughMem[];
	static const _TCHAR kExtraCharsAfterSwitch_[];
	static const _TCHAR kInvalidCommand_[];
	static const _TCHAR kIncompleteParamInSwitch_[];
	static const _TCHAR kIncorrectParamInSwitch_[];
	static const _TCHAR kMatchBufferTooLarge[];
	static const _TCHAR k_at_[];
	static const _TCHAR kMissingListFileName[];
	static const _TCHAR kMissingArchiveName[];
	static const _TCHAR kCannotOpenList[];
	static const _TCHAR kCannotReadList[];
	static const _TCHAR kUnknownError[];
};

}

#endif // RADYX_STRINGS_H