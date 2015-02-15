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

#ifndef RADYX_PATH_H
#define RADYX_PATH_H

#include <cstring>
#include <array>
#include "winlean.h"
#include "common.h"
#include "CharType.h"

namespace Radyx {

class Path : public FsString
{
public:
	static const _TCHAR separator;
#ifdef _WIN32
	static const _TCHAR posix_separator;
	static const _TCHAR drive_specifier;
	static const std::array<_TCHAR, 5> extended_length_spec;
#endif
	static const _TCHAR wildcard_all;
	static const _TCHAR wildcard_1char;
	static const _TCHAR dir_search_all[];

	Path() {}
	Path(_TCHAR* s) : FsString(s) {}
	Path(const _TCHAR* s) : FsString(s) {}
	Path(const _TCHAR* s, size_t len) : FsString(s, len) {}
	Path(const FsString& str) : FsString(str) {}
	void SetExtendedLength(const Path& full_path);
	void SetName(const _TCHAR* name);
	void AppendName(const _TCHAR* name);
	void AppendExtension(const _TCHAR* ext);
	inline void AppendPathSeparator();
	ptrdiff_t FsCompare(size_t pos, size_t end, const Path& second, size_t pos_2, size_t end_2) const;
	inline ptrdiff_t FsCompare(size_t pos, const Path& second, size_t pos_2) const;
	inline ptrdiff_t FsCompare(const Path& second) const;
	FsString GetName() const;
	void ConvertSeparators();
	inline size_t GetNamePos() const;
	bool IsPureRelativePath() const;
	inline bool IsExtendedLength() const;
	inline bool IsDevNull() const;
	static bool MatchFileSpec(const _TCHAR* name, const _TCHAR* spec);
	static inline size_t GetNamePos(const _TCHAR* name);
	static inline size_t GetExtPos(const _TCHAR* name);
	static inline bool IsRelativeAlias(const _TCHAR* name);
	static inline bool IsWildcard(const _TCHAR* name);
};

void Path::AppendPathSeparator()
{
	if (length() > 0 && back() != separator) {
		append(1, separator);
	}
}

inline ptrdiff_t Path::FsCompare(size_t pos, const Path& second, size_t pos_2) const
{
	return FsCompare(pos, npos, second, pos_2, npos);
}

inline ptrdiff_t Path::FsCompare(const Path& second) const
{
	return FsCompare(0, npos, second, 0, npos);
}

size_t Path::GetNamePos() const
{
	size_t name_pos = find_last_of(separator);
	if (name_pos != npos) return name_pos + 1;
#ifdef _WIN32
	if (length() > 1 && at(1) == drive_specifier) {
		return 2;
	}
#endif
	return 0;
}

size_t Path::GetNamePos(const _TCHAR* path)
{
	const _TCHAR* name = _tcsrchr(path, separator);
	if (name != nullptr) {
		return name + 1 - path;
	}
#ifdef _WIN32
	if (path[0] != '\0' && path[1] == drive_specifier) {
		return 2;
	}
#endif
	return 0;
}

#ifdef _UNICODE

size_t Path::GetExtPos(const _TCHAR* name)
{
	const _TCHAR* ext = wcsrchr(name, '.');
	if (ext == nullptr) {
		return wcslen(name);
	}
	return ext + 1 - name;
}

#else

size_t Path::GetExtPos(const _TCHAR* name)
{
	const _TCHAR* ext = strrchr(name, '.');
	if (ext == nullptr) {
		return strlen(name);
	}
	return ext + 1 - name;
}
#endif // _UNICODE

bool Path::IsRelativeAlias(const _TCHAR* name)
{
	for (; *name != 0; ++name) {
		if (*name != '.')
			return false;
	}
	return true;
}

bool Path::IsWildcard(const _TCHAR* name)
{
	for (; *name != '\0'; ++name) {
		if (*name == wildcard_all || *name == wildcard_1char)
			return true;
	}
	return false;
}

#ifdef _WIN32

bool Path::IsExtendedLength() const
{
	return compare(0, extended_length_spec.size() - 1, extended_length_spec.data()) == 0;
}

inline bool Path::IsDevNull() const
{
	return FsCompare(_T("nul")) == 0;
}

#else

inline bool Path::IsDevNull() const
{
	return FsCompare(_T("/dev/null")) == 0;
}

#endif

}

#endif // RADYX_PATH_H