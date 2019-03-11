///////////////////////////////////////////////////////////////////////////////
//
// Class: Strings
//        Message strings
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

#include "CharType.h"
#include "Strings.h"

namespace Radyx {

const _TCHAR Strings::kHelpString[] = _T(
"Usage: radyx <command> [<switches>...] <archive_name> [<file_names>...]\n"
"\n"
"<Commands>\n"
"  a : Add files to archive\n"
"\n"
"<Switches>\n"
"  -- : Stop switches parsing\n"
"  @listfile : set path to listfile that contains file names\n"
"  -ar[-] : Read more input while compressing (default: on)\n"
"  -q[-] : disable input filename display\n"
"  -i[r[-|0]]{@listfile|!wildcard} : Include filenames\n"
"  -m{Parameters} : set compression method\n"
"    -mmt[N] : set number of CPU threads\n"
"    -mx[N] : set compression level: -mx1 (fastest) ... -mx12 (ultra)\n"
"  -r[-|0] : Recurse subdirectories\n"
"  -ssw : compress shared files\n"
"  -w[{path}] : assign work directory\n"
"  -x[r[-|0]]{@listfile|!wildcard} : exclude filenames\n");
const char Strings::kBreakSignaled[] = "Break signaled.";
const _TCHAR Strings::kExtractionUnsupported[] = _T("This version of Radyx does not support extraction.\nUse 7-zip or a compatible program.");
const _TCHAR Strings::kNoCommandSpecified[] = _T("No command specified");
const _TCHAR Strings::kLcLpNoGreaterThan4[] = _T("Literal context bits (-mlc) + literal position bits (-mlp) must be no greater than 4.");
const _TCHAR Strings::kSearching[] = _T("Searching...");
const _TCHAR Strings::kCreatingArchive_[] = _T("Creating archive ");
const _TCHAR Strings::kNameCollision_[] = _T("Duplicate filenames: ");
const _TCHAR Strings::kAdding_[] = _T("Adding ");
const _TCHAR Strings::kCannotOpen_[] = _T("Cannot open ");
const _TCHAR Strings::kCannotRead_[] = _T("Cannot read ");
const _TCHAR Strings::kWarningsForFiles[] = _T("Warnings for files:");
const char Strings::kFileIoError[] = "File I/O error.";
const _TCHAR Strings::kWarningCouldntOpen_[] = _T("Warning: Could not open ");
const _TCHAR Strings::kNoFilesFound[] = _T("No files found.");
const _TCHAR Strings::kUnrecoverableErrorReading[] = _T("Unrecoverable error reading");
const _TCHAR Strings::kCannotChDir[] = _T("Cannot change to working directory");
const _TCHAR Strings::kFound_[] = _T("Found ");
const _TCHAR Strings::k_file[] = _T(" file.");
const _TCHAR Strings::k_files[] = _T(" files.");
const _TCHAR Strings::kUnableConvertUtf8to16[] = _T("Unable to convert filename from UTF-8 to UTF-16.");
const _TCHAR Strings::kCannotWriteArchive[] = _T("Cannot write archive file");
const _TCHAR Strings::kArchiveFileExists[] = _T("Archive file exists. This version of Radyx does not support updating.");
const _TCHAR Strings::kCannotCreateArchive[] = _T("Cannot create archive file");
const _TCHAR Strings::kErrorCol_[] = _T("\rError: ");
const _TCHAR Strings::kErrorNotEnoughMem[] = _T("Error: Not enough memory. Try decreasing the dictionary size.");
const _TCHAR Strings::kExtraCharsAfterSwitch_[] = _T("Extra characters after switch ");
const _TCHAR Strings::kInvalidCommand_[] = _T("Invalid command ");
const _TCHAR Strings::kIncompleteParamInSwitch_[] = _T("Incomplete parameter in switch ");
const _TCHAR Strings::kIncorrectParamInSwitch_[] = _T("Incorrect parameter in switch ");
const _TCHAR Strings::kMatchBufferTooLarge[] = _T("Warning: Buffer size set with -mb is probably too large.");
const _TCHAR Strings::k_at_[] = _T(" at ");
const _TCHAR Strings::kMissingListFileName[] = _T("Missing list file name.");
const _TCHAR Strings::kMissingArchiveName[] = _T("Missing archive name.");
const _TCHAR Strings::kCannotOpenList[] = _T("Cannot open list file");
const _TCHAR Strings::kCannotReadList[] = _T("Cannot read list file");
const _TCHAR Strings::kUnknownError[] = _T("Unknown error.");
const _TCHAR Strings::kDone[] = _T("Done.");
}