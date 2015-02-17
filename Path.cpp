///////////////////////////////////////////////////////////////////////////////
//
// Class: Path
//        Manipulation of file system paths
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
#ifdef _WIN32
#include <Shlwapi.h>
#else
#include <fnmatch.h>
#endif
#include "Path.h"

namespace Radyx {

#ifdef _WIN32
const _TCHAR Path::separator = '\\';
const _TCHAR Path::posix_separator = '/';
const _TCHAR Path::drive_specifier = ':';
const _TCHAR Path::dir_search_all[] = _T("*");
const std::array<_TCHAR, 5> Path::extended_length_spec = { _T("\\\\?\\") };
#else
const _TCHAR Path::separator = '/';
const _TCHAR Path::dir_search_all[] = "";
#endif
const _TCHAR Path::wildcard_all = '*';
const _TCHAR Path::wildcard_1char = '?';

#ifdef _WIN32

void Path::SetExtendedLength(const Path& full_path)
{
	if (!full_path.IsExtendedLength()) {
		reserve(full_path.length() + extended_length_spec.size());
		*this = extended_length_spec.data();
	}
	append(full_path);
}

ptrdiff_t Path::FsCompare(size_t pos, size_t end, const Path& second, size_t pos_2, size_t end_2) const
{
	end = std::min(length(), end);
	end_2 = std::min(second.length(), end_2);
	for (; pos < end && pos_2 < end_2; ++pos, ++pos_2) {
		if (operator[](pos) != second[pos_2]) {
			_TCHAR ch;
			if (LCMapString(LOCALE_INVARIANT, LCMAP_UPPERCASE, &operator[](pos), 1, &ch, 1) == 0) {
				ch = operator[](pos);
			}
			_TCHAR ch_2;
			if (LCMapString(LOCALE_INVARIANT, LCMAP_UPPERCASE, &second[pos_2], 1, &ch_2, 1) == 0) {
				ch_2 = second[pos_2];
			}
			if (ch != ch_2) {
				return ptrdiff_t(ch) - ptrdiff_t(ch_2);
			}
		}
	}
	if (pos < end) {
		return 1;
	}
	if (pos_2 < end_2) {
		return -1;
	}
	return 0;
}

void Path::ConvertSeparators()
{
	for (auto& it : *this) {
		if (it == posix_separator) {
			it = separator;
		}
	}
}

bool Path::MatchFileSpec(const _TCHAR* name, const _TCHAR* spec)
{
	return PathMatchSpec(name, spec) == TRUE;
}

#else

ptrdiff_t Path::FsCompare(size_t pos, size_t end, const Path& second, size_t pos_2, size_t end_2) const
{
	return compare(pos, end - pos, second, pos_2, end_2 - pos_2);
}

void Path::ConvertSeparators()
{
}

bool Path::MatchFileSpec(const _TCHAR* name, const _TCHAR* spec)
{
	return fnmatch(spec, name, FNM_PERIOD) == 0;
}

#endif

void Path::SetName(const _TCHAR* name)
{
	replace(GetNamePos(), npos, name);
}

void Path::AppendName(const _TCHAR* name)
{
	AppendPathSeparator();
	append(name);
}

void Path::AppendExtension(const _TCHAR* ext)
{
	if (length() > 0 && find_first_of('.') == npos) {
		append(ext);
	}
}

FsString Path::GetName() const
{
	return substr(GetNamePos());
}

// Is path a relative path containing only directory names (not ..\ etc)
bool Path::IsPureRelativePath() const
{
	if (length() < 1 || front() == separator) {
		return false;
	}
#ifdef _WIN32
	if (length() > 1 && operator[](1) == drive_specifier) {
		return false;
	}
#endif
	size_t i = 0;
	size_t end = find_first_of(separator);
	while (end != npos) {
		for (; i < end && operator[](i) == '.'; ++i) {
		}
		if (i == end) {
			return false;
		}
		i = end + 1;
		end = find_first_of(separator, end + 1);
	}
	return true;
}

}