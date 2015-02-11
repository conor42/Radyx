///////////////////////////////////////////////////////////////////////////////
//
// Class: RadyxOptions
//        Compression and archiving options, and the list of file specs to add
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

#ifndef _WIN32
#include <unistd.h>
#endif
#include "winlean.h"
#include "common.h"
#include <thread>
#include <iostream>
#include "RadyxOptions.h"
#include "Lzma2Encoder.h"
#include "Path.h"
#include "DirScanner.h"
#include "IoException.h"
#include "Strings.h"

namespace Radyx {

static bool CompareFileSpec(const RadyxOptions::FileSpec& first, const RadyxOptions::FileSpec& second)
{
	if (first.root != second.root) {
		return first.root < second.root;
	}
	return first.path.FsCompare(0, first.root, second.path, 0, first.root) < 0;
}

RadyxOptions::FileSpec::FileSpec(const _TCHAR* path_, Recurse recurse_)
	: path(path_),
	root(path.GetNamePos()),
	recurse(recurse_ == kRecurseAll || (recurse_ == kRecurseWildcard && Path::IsWildcard(path.c_str() + root)))
{
	path.ConvertSeparators();
}

void RadyxOptions::FileSpec::SetFullPath(const _TCHAR* full_path, unsigned length)
{
	if(path.compare(full_path) != 0) {
		path = full_path;
		root = path.GetNamePos();
	}
}

RadyxOptions::RadyxOptions(int argc, _TCHAR* argv[], Path& archive_path)
	: default_recurse(kRecurseNone),
	share_deny_none(false),
//	yes_to_all(false),
	multi_thread(true),
	thread_count(0),
	solid_by_extension(false),
	solid_unit_size(UINT64_C(1) << 31),
	solid_file_count(UINT32_MAX),
	bcj_filter(true),
	async_read(true),
	store_creation_time(false),
	quiet_mode(false)
{
	ParseCommand(argc, argv);
	int i = 2;
	for (; i < argc && (argv[i][0] != '-' || argv[i][1] != '-'); ++i) {
		if (argv[i][0] == '-') {
			const _TCHAR* arg = argv[i] + 1;
			if (*arg == 'r') {
				default_recurse = HandleRecurse(arg);
				if (*arg != '\0') {
					std::Tcerr << Strings::kErrorCol_ << Strings::kExtraCharsAfterSwitch_ << _T("'-r'") << std::endl;
					throw std::invalid_argument("");
				}
			}
		}
	}
	int switches_end = i;
	for (i = 2; i < switches_end; ++i) {
		if (argv[i][0] == '-') {
			try {
				ParseArg(argv[i] + 1);
			}
			catch (InvalidParameter& ex) {
				if (*ex.ArgError() == '\0') {
					std::Tcerr << Strings::kIncompleteParamInSwitch_ << argv[i] << std::endl;
				}
				else {
					std::Tcerr << Strings::kIncorrectParamInSwitch_ << argv[i] << Strings::k_at_ << ex.ArgError() << std::endl;
				}
				throw std::invalid_argument("");
			}
		}
	}
	if (lzma2.match_buffer_size.IsSet()
		&& lzma2.match_buffer_size > MatchTableBuilder::GetMaxBufferSize(lzma2.dictionary_size))
	{
		std::Tcerr << Strings::kMatchBufferTooLarge << std::endl;
	}
	for (i = 2; i < argc; ++i) {
		if (argv[i][0] == '@') {
			if (argv[i][1] == '\0') {
				std::Tcerr << Strings::kErrorCol_ << Strings::kMissingListFileName << std::endl;
				throw std::invalid_argument("");
			}
			ReadFileList(argv[i] + 1, default_recurse, file_specs);
		}
		else if (argv[i][0] != '-' || i > switches_end) {
			if (archive_path.length() == 0) {
				archive_path.reserve(_tcslen(argv[i]) + 4);
				archive_path = argv[i];
				if (!archive_path.IsDevNull()) {
					archive_path.AppendExtension(_T(".7z"));
				}
			}
			else {
				file_specs.push_back(FileSpec(argv[i], default_recurse));
			}
		}
		else if (i == switches_end) {
			if (argv[i][2] != '\0') {
				std::Tcerr << Strings::kErrorCol_ << Strings::kExtraCharsAfterSwitch_ << _T("'--'") << std::endl;
				throw std::invalid_argument("");
			}
		}
	}
	if (archive_path.length() == 0) {
		std::Tcerr << Strings::kErrorCol_ << Strings::kMissingArchiveName << std::endl;
		throw std::invalid_argument("");
	}
	if (lzma2.lc + lzma2.lp > 4) {
		std::Tcerr << Strings::kLcLpNoGreaterThan4 << std::endl;
		throw std::invalid_argument("");
	}
	if (!multi_thread) {
		thread_count = 1;
	}
	else if (thread_count == 0) {
		unsigned hardware_threads = std::thread::hardware_concurrency();
		thread_count = (hardware_threads != 0) ? hardware_threads : 2;
	}
	LoadFullPaths();
	file_specs.sort(CompareFileSpec);
	lzma2.LoadCompressLevel();
}

void RadyxOptions::ParseCommand(int argc, _TCHAR* argv[])
{
	if (argc < 2) {
		std::Tcerr << Strings::kNoCommandSpecified << std::endl;
		throw std::invalid_argument("");
	}
	if (argv[1][1] == '\0') {
		switch (argv[1][0]) {
		case 'a':
			return;
		case 'e':
		case 'x':
		case 'l':
		case 't':
			std::Tcerr << Strings::kExtractionUnsupported << std::endl;
			throw std::invalid_argument("");
		default:
			break;
		}
	}
	std::Tcerr << Strings::kInvalidCommand_ << '\'' << argv[1] << '\'' << std::endl;
	throw std::invalid_argument("");
}

void RadyxOptions::ReadFileList(const _TCHAR* path, Recurse recurse, std::list<FileSpec>& spec_list)
{
	std::ifstream in(path);
	if (in.fail()) {
		throw IoException(Strings::kCannotOpenList, path);
	}
#ifdef _UNICODE
	std::array<char, 1024> buffer;
	in.read(buffer.data(), buffer.size());
	if (in.bad()) {
		throw IoException(Strings::kCannotReadList, path);
	}
	if (IsTextUnicode(buffer.data(), static_cast<int>(in.gcount()), NULL) != 0) {
		in.close();
		ReadFileListW(path, recurse, spec_list);
		return;
	}
	in.clear();
	in.seekg(0);
#endif
	std::string str;
	while (!in.eof()) {
		std::getline(in, str);
		if (in.bad()) {
			break;
		}
		while (str.length() > 0 && (str.back() == '\r' || str.back() == ' ')) {
			str.pop_back();
		}
		if (str.length() > 0) {
#ifdef _UNICODE
			_TCHAR wpath[kMaxPath + 1];
			int len = MultiByteToWideChar(CP_UTF8,
				0,
				str.c_str(),
				static_cast<int>(str.length()),
				wpath,
				kMaxPath);
			if (len == 0) {
				std::Tcerr << Strings::kErrorCol_;
				std::cerr << '"' << str.c_str() << "\": ";
				std::Tcerr << Strings::kUnableConvertUtf8to16 << std::endl;
				throw std::invalid_argument("");
			}
			wpath[len] = '\0';
			spec_list.push_back(FileSpec(wpath, recurse));
#else
			spec_list.push_back(FileSpec(str.c_str(), recurse));
#endif
		}
	}
}

#ifdef _UNICODE
void RadyxOptions::ReadFileListW(const _TCHAR* path, Recurse recurse, std::list<FileSpec>& spec_list)
{
	// Setting up a wifstream to use UTF-16 is pretty ugly so using a C stream
	FILE* in;
	if (_wfopen_s(&in, path, L"rb, ccs=UTF-16LE") != 0) {
		throw IoException(Strings::kCannotOpenList, path);
	}
	_TCHAR ch = static_cast<_TCHAR>(fgetwc(in));
	// Skip the byte order marker char
	if (ch != L'\uFFFE' && ch != L'\uFEFF') {
		rewind(in);
	}
	_TCHAR wpath[kMaxPath + 1];
	while (fgetws(wpath, kMaxPath + 1, in) != NULL) {
		_TCHAR* p = wpath + wcslen(wpath);
		while (p > wpath && (p[-1] == '\r' || p[-1] == '\n' || p[-1] == ' ')) {
			--p;
		}
		if (p > wpath) {
			*p = '\0';
			spec_list.push_back(FileSpec(wpath, recurse));
		}
	}
}
#endif

void RadyxOptions::ParseArg(const _TCHAR* arg)
{
	switch (arg[0]) {
	case 'a':
		switch (arg[1]) {
		case 'r': {
			arg += (arg[2] == '=') + 2;
			int on_off = CheckOnOff(arg);
			if (on_off >= 0) {
				async_read = (on_off > 0);
			}
			else {
				throw InvalidParameter(arg);
			}
			break;
		}
		default:
			throw InvalidParameter(arg);
		}
		break;
	case 'i': {
		HandleFilenames(arg, file_specs);
		break;
	}
	case 'm':
		HandleCompressionMethod(arg);
		break;
	case 'q':
		quiet_mode = true;
		break;
	case 'r':
		// Already done
		break;
	case 's':
		switch (arg[1]) {
		case 's':
			Handle_ss(arg);
			break;
		default:
			throw InvalidParameter(arg);
		}
		break;
	case 'w':
		if (arg[1] == '\0') {
			throw InvalidParameter(arg + 1);
		}
		working_dir = arg + 1;
		break;
	case 'x': {
		HandleFilenames(arg, exclusions);
		break;
	}
/*	case 'y':
		yes_to_all = true;
		break;*/
	case 'z': {
		arg += (arg[1] == '=') + 1;
		int on_off = CheckOnOff(arg);
		if (on_off == 0) {
			lzma2.random_filter = 0;
		}
		else if (on_off == 1) {
			lzma2.random_filter = kRandomFilterDefault;
		}
		else {
			lzma2.random_filter = ReadSimpleNumericParam(arg, 0, 1000);
		}
		break;
	}
	default:
		throw InvalidParameter(arg);
	}
}

void RadyxOptions::HandleFilenames(const _TCHAR* arg, std::list<FileSpec>& spec_list)
{
	++arg;
	Recurse recurse = default_recurse;
	if (arg[0] == 'r') {
		recurse = HandleRecurse(arg);
	}
	if (arg[0] == '@' || arg[0] == '!') {
		if (arg[1] == '\0') {
			throw InvalidParameter(arg + 1);
		}
		if (arg[0] == '@') {
			ReadFileList(arg + 1, recurse, spec_list);
		}
		else {
			spec_list.push_back(FileSpec(arg + 1, recurse));
		}
	}
	else {
		throw InvalidParameter(arg);
	}
}

void RadyxOptions::HandleCompressionMethod(const _TCHAR* arg)
{
	++arg;
	switch (*arg++) {
	case 'x':
		arg += (arg[0] == '=');
		lzma2.compress_level = ReadSimpleNumericParam(arg, 1, 9);
		break;
	case 's':
		HandleSolidMode(arg);
		break;
	case 'm':
		switch (arg[0]) {
		case 't':
		{
			arg += (arg[1] == '=') + 1;
			int on_off = CheckOnOff(arg);
			if (on_off == 0) {
				multi_thread = false;
			}
			else if (on_off == 1) {
				multi_thread = true;
			}
			else {
				thread_count = ReadSimpleNumericParam(arg, 1, UINT16_MAX);
				multi_thread = true;
			}
			break;
		}
		case 'c':
		{
			arg += (arg[1] == '=') + 1;
			lzma2.match_cycles = ReadSimpleNumericParam(arg, 1, 1000);
			break;
		}
		default:
			throw InvalidParameter(arg);
		}
		break;
	case 'a':
		arg += (arg[0] == '=');
		lzma2.encoder_mode = static_cast<Lzma2Options::Mode>(ReadSimpleNumericParam(arg, 0, 2));
		break;
	case 'd':
	{
		bool secondary = false;
		if (arg[0] == 's') {
			secondary = true;
			++arg;
		}
		arg += (arg[0] == '=');
		_TCHAR* end;
		unsigned long u = ReadDecimal(arg, end);
		if (end == arg) {
			throw InvalidParameter(end);
		}
		uint_least64_t value = ApplyMultiplier(end, u);
		if (secondary) {
			unsigned bits = 0; 
			while (value > 1) {
				value >>= 1;
				++bits;
			}
			if (bits < Lzma2Encoder::Get2ndDictionaryBitsMin()
				|| bits > Lzma2Encoder::Get2ndDictionaryBitsMax())
			{
				throw InvalidParameter(arg);
			}
			lzma2.second_dict_bits = bits;
		}
		else {
			if (value < Lzma2Encoder::GetUserDictionarySizeMin()
				|| value > Lzma2Encoder::GetUserDictionarySizeMax())
			{
				throw InvalidParameter(arg);
			}
			lzma2.dictionary_size = static_cast<size_t>(value);
		}
		break;
	}
	case 'b':
	{
		arg += (arg[0] == '=');
		_TCHAR* end;
		unsigned long u = ReadDecimal(arg, end);
		uint_least64_t value = u;
		if (end > arg) {
			value = ApplyMultiplier(end, u);
			lzma2.match_buffer_size = static_cast<size_t>(value);
		}
		break;
	}
	case 'f':
		if (arg[0] == 'b') {
			arg += (arg[1] == '=') + 1;
			lzma2.fast_length = ReadSimpleNumericParam(arg, 6, 254);
		}
		else {
			arg += (arg[0] == '=');
			int on_off = CheckOnOff(arg);
			if (on_off == 0) {
				bcj_filter = false;
			}
			else if (on_off == 1) {
				bcj_filter = true;
			}
			else {
				bcj_filter = _tcsicmp(arg, _T("BCJ")) == 0;
				if (!bcj_filter) {
					throw InvalidParameter(arg);
				}
			}
			break;
		}
		break;
	case 'l':
		switch (arg[0]) {
		case 'c':
			arg += (arg[1] == '=') + 1;
			lzma2.lc = ReadSimpleNumericParam(arg, 0, Lzma2Encoder::GetLiteralContextBitsMax());
			break;
		case 'p':
			arg += (arg[1] == '=') + 1;
			lzma2.lp = ReadSimpleNumericParam(arg, 0, Lzma2Encoder::GetLiteralPositionBitsMax());
			break;
		default:
			throw InvalidParameter(arg);
		}
		break;
	case 'o':
		arg += (arg[0] == '=');
		lzma2.block_overlap = ReadSimpleNumericParam(arg, 1, 14);
		break;
	case 'p':
		if (arg[0] == 'b') {
			arg += (arg[1] == '=') + 1;
			lzma2.pb = ReadSimpleNumericParam(arg, 0, Lzma2Encoder::GetPositionBitsMax());
		}
		else {
			throw InvalidParameter(arg);
		}
		break;
	case 't':
		if (arg[0] == 'c') {
			arg += (arg[1] == '=') + 1;
			int on_off = CheckOnOff(arg);
			if (on_off < 0) {
				throw InvalidParameter(arg);
			}
			store_creation_time = on_off != 0;
		}
		else {
			throw InvalidParameter(arg);
		}
		break;
	default:
		throw InvalidParameter(arg - 1);
	}
}

void RadyxOptions::HandleSolidMode(const _TCHAR* arg)
{
	arg += (arg[0] == '=');
	int on_off = CheckOnOff(arg);
	if (on_off == 0) {
		solid_file_count = 1;
	}
	else if (on_off == 1) {
		solid_file_count = UINT32_MAX;
	}
	else {
		if (arg[0] == '\0') {
			throw InvalidParameter(arg);
		}
		do {
			if (arg[0] == 'e') {
				solid_by_extension = true;
				++arg;
			}
			else {
				_TCHAR* end;
				unsigned long u = ReadDecimal(arg, end);
				if (end == arg || end[0] == '\0') {
					throw InvalidParameter(end);
				}
				arg = end + 1;
				if (end[0] == 'f') {
					solid_file_count = u + (u == 0);
				}
				else {
					solid_unit_size = ApplyMultiplier(end, u);
					solid_unit_size += solid_unit_size == 0;
				}
			}
		} while (arg[0] != '\0');
	}
}

RadyxOptions::Recurse RadyxOptions::HandleRecurse(const _TCHAR*& arg)
{
	++arg;
	if (arg[0] == '-') {
		++arg;
		return kRecurseNone;
	}
	else if (arg[0] == '0') {
		++arg;
		return kRecurseWildcard;
	}
	return kRecurseAll;
}

void RadyxOptions::Handle_ss(const _TCHAR* arg)
{
	if (arg[2] == 'w') {
		if (arg[3] != '\0') {
			throw InvalidParameter(arg + 3);
		}
		share_deny_none = true;
	}
	else {
		throw InvalidParameter(arg + 2);
	}
}

int RadyxOptions::CheckOnOff(const _TCHAR* arg) const
{
	FsString str(arg);
	if (str.find(_T("off")) == 0) {
		if (arg[3] != '\0') {
			throw InvalidParameter(arg + 3);
		}
		return 0;
	}
	else if (str.find(_T("on")) == 0) {
		if (arg[2] != '\0') {
			throw InvalidParameter(arg + 2);
		}
		return 1;
	}
	return -1;
}

unsigned RadyxOptions::ReadSimpleNumericParam(const _TCHAR* arg, unsigned min, unsigned max) const
{
	_TCHAR* end;
	unsigned long value = ReadDecimal(arg, end);
	if (end == arg || end[0] != '\0') {
		throw InvalidParameter(end);
	}
	if (value < min || value > max) {
		throw InvalidParameter(arg);
	}
	return static_cast<unsigned>(value);
}

uint_least64_t RadyxOptions::ApplyMultiplier(const _TCHAR* arg, unsigned value) const
{
	switch (arg[0]) {
	case 'b':
		return value;
	case 'k':
		return value * UINT64_C(1024);
	case 'm':
		return value * UINT64_C(1024) * 1024;
	case 'g':
		return value * UINT64_C(1024) * 1024 * 1024;
	case '\0':
		return UINT64_C(1) << value;
	default:
		throw InvalidParameter(arg);
	}
}

void RadyxOptions::LoadFullPaths()
{
	if (working_dir.length() != 0 && _tchdir(working_dir.c_str()) < 0) {
		throw IoException(Strings::kCannotChDir, working_dir.c_str());
	}
	std::unique_ptr<_TCHAR[]> full_path(new _TCHAR[kMaxPath]);
	Path temp;
	for (auto& fs : file_specs) {
#ifdef _WIN32
		DWORD len = GetFullPathName(fs.path.c_str(), kMaxPath, full_path.get(), NULL);
		if (len != 0 && len < kMaxPath) {
			fs.SetFullPath(full_path.get(), len);
		}
#else
		temp = fs.path;
		temp.SetName(".");
		if (realpath(temp.c_str(), full_path.get()) != NULL) {
			temp = fs.path.c_str() + fs.path.GetNamePos();
			fs.path = full_path.get();
			fs.path.AppendName(temp.c_str());
			fs.root = fs.path.GetNamePos();
	}
#endif
	}
}

bool RadyxOptions::SearchExclusions(const Path& path, size_t root, bool is_root) const
{
	for (const auto& it : exclusions ) {
		if (is_root || it.recurse) {
			if (it.root != 0) {
				// Exclusion contains a path
				size_t excl_name = it.path.GetNamePos();
				if (path.FsCompare(root, root + excl_name, it.path, 0, excl_name) == 0 &&
					Path::MatchFileSpec(path.c_str() + path.GetNamePos(), it.path.c_str() + excl_name)) {
					return true;
				}
			}
			else {
				// Compare only the names
				if (Path::MatchFileSpec(path.c_str() + path.GetNamePos(), it.path.c_str())) {
					return true;
				}
			}
		}
	}
	return false;
}

void RadyxOptions::GetFiles(ArchiveCompressor& arch_comp) const
{
	for (auto it = file_specs.cbegin(); it != file_specs.cend();) {
		auto end = it;
		// Find all file specs in the same dir
		for (++end; end != file_specs.cend()
			&& it->root == end->root
			&& it->path.compare(0, it->root, end->path, 0, it->root) == 0; ++end) {
		}
		// Search the dir for all of them
		SearchDir(it, end, arch_comp);
		it = end;
	}
}

void RadyxOptions::SearchDir(std::list<FileSpec>::const_iterator it_first,
	std::list<FileSpec>::const_iterator it_end,
	ArchiveCompressor& arch_comp) const
{
	Path dir;
	bool recurse = false;
	for (auto it = it_first; it != it_end; ++it) {
		recurse |= it->recurse;
	}
	bool all_match = false;
#ifdef _WIN32
	all_match = !recurse && it_first == std::prev(it_end);
	if (all_match) {
		dir = it_first->path;
	}
	else {
		dir = it_first->path.substr(0, it_first->root);
		dir.push_back(Path::wildcard_all);
	}
#else
	dir = it_first->path.substr(0, it_first->root);
	if (dir.length() == 0) {
		dir = ".";
	}
#endif
	SearchDir(dir, it_first, it_end, all_match, recurse, true, arch_comp);
}

void RadyxOptions::SearchDir(Path& dir,
	std::list<FileSpec>::const_iterator it_first,
	std::list<FileSpec>::const_iterator it_end,
	bool all_match,
	bool recurse,
	bool is_root,
	ArchiveCompressor& arch_comp) const
{
	DirScanner scan(dir);
	if (scan.NotFound()) {
		return;
	}
	do {
		const _TCHAR* name = scan.GetName();
		if (Path::IsRelativeAlias(name)) {
			continue;
		}
		dir.SetName(name);
		if (exclusions.size() > 0 && SearchExclusions(dir, it_first->root, is_root)) {
			continue;
		}
		if (scan.IsDirectory()) {
			if (recurse) {
				size_t length = dir.length();
				dir.AppendName(Path::dir_search_all);
				SearchDir(dir, it_first, it_end, false, true, false, arch_comp);
				dir.resize(length);
			}
		}
		else {
			if (all_match || IsMatch(it_first, it_end, name, is_root)) {
				arch_comp.Add(dir.c_str(), it_first->root, scan.GetFileSize());
			}
		}
	} while (!g_break && scan.Next());
}

bool RadyxOptions::IsMatch(std::list<FileSpec>::const_iterator it_first,
	std::list<FileSpec>::const_iterator it_end,
	const _TCHAR* name,
	bool is_root) const
{
	for (auto it = it_first; it != it_end; ++it) {
		if ((is_root || it->recurse) && Path::MatchFileSpec(name, it->path.c_str() + it->root)) {
			return true;
		}
	}
	return false;
}

}