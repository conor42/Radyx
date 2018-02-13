///////////////////////////////////////////////////////////////////////////////
//
// Class:   ArchiveCompressor
//          Reads input files into the unit compressor and collects
//          information for the archive database
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

#include "common.h"
#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#endif
#include <fstream>
#include <iostream>
#include <mutex>
#include "CharType.h"
#include "UnitCompressor.h"
#include "ArchiveCompressor.h"
#include "Strings.h"
#include "IoException.h"

namespace Radyx {

const _TCHAR ArchiveCompressor::extensions[] =
_T("chm\0hxi\0hxs")
_T("\0gif\0jpeg\0jpg\0jp2\0png\0tiff\0bmp\0ico\0psd\0psp")
_T("\0awg\0ps\0eps\0cgm\0dxf\0svg\0vrml\0wmf\0emf\0ai\0md")
_T("\0cad\0dwg\0pps\0key\0sxi")
_T("\0max\0")_T("3ds")
_T("\0iso\0bin\0nrg\0mdf\0img\0pdi\0tar\0cpio\0xpi")
_T("\0vfd\0vhd\0vud\0vmc\0vsv")
_T("\0vmdk\0dsk\0nvram\0vmem\0vmsd\0vmsn\0vmss\0vmtm")
_T("\0inl\0inc\0idl\0acf\0asa\0h\0hpp\0hxx\0c\0cpp\0cxx\0m\0mm\0go\0swift\0rc\0java\0cs\0rs\0pas\0bas\0vb\0cls\0ctl\0frm\0dlg\0def")
_T("\0f77\0f\0f90\0f95")
_T("\0asm\0s\0sql\0manifest\0dep")
_T("\0mak\0clw\0csproj\0vcproj\0sln\0dsp\0dsw")
_T("\0class")
_T("\0bat\0cmd\0bash\0sh")
_T("\0xml\0xsd\0xsl\0xslt\0hxk\0hxc\0htm\0html\0xhtml\0xht\0mht\0mhtml\0htw\0asp\0aspx\0css\0cgi\0jsp\0shtml")
_T("\0awk\0sed\0hta\0js\0json\0php\0php3\0php4\0php5\0phptml\0pl\0pm\0py\0pyo\0rb\0tcl\0ts\0vbs")
_T("\0text\0txt\0tex\0ans\0asc\0srt\0reg\0ini\0doc\0docx\0mcw\0dot\0rtf\0hlp\0xls\0xlr\0xlt\0xlw\0ppt\0pdf")
_T("\0sxc\0sxd\0sxi\0sxg\0sxw\0stc\0sti\0stw\0stm\0odt\0ott\0odg\0otg\0odp\0otp\0ods\0ots\0odf")
_T("\0abw\0afp\0cwk\0lwp\0wpd\0wps\0wpt\0wrf\0wri")
_T("\0abf\0afm\0bdf\0fon\0mgf\0otf\0pcf\0pfa\0snf\0ttf")
_T("\0dbf\0mdb\0nsf\0ntf\0wdb\0db\0fdb\0gdb")
_T("\0pdb\0pch\0idb\0ncb\0opt")
_T("\0")_T("3gp\0avi\0mov\0mpeg\0mpg\0mpe\0wmv")
_T("\0aac\0ape\0fla\0flac\0la\0mp3\0m4a\0mp4\0ofr\0ogg\0pac\0ra\0rm\0rka\0shn\0swa\0tta\0wv\0wma\0wav")
_T("\0swf")
_T("\0lzma\0")_T("7z\0xz\0ace\0arc\0arj\0bz\0bz2\0deb\0lzo\0lzx\0gz\0pak\0rpm\0sit\0tgz\0tbz\0tbz2\0tgz\0cab\0ha\0lha\0lzh\0rar\0zoo")
_T("\0zip\0jar\0ear\0war\0msi")
_T("\0obj\0lib\0tlb\0o\0a\0so")
_T("\0exe\0dll\0ocx\0vbx\0sfx\0sys\0awx\0com\0out\0");

#ifdef _WIN32

#ifdef _MSC_VER
#  pragma warning(disable : 4996)        /* disable: C4996: Call to 'std::basic_string::copy' with parameters that may be unsafe */
#endif

bool ArchiveCompressor::FileReader::Open(const FileInfo& fi, bool share_deny_none)
{
	std::array<_TCHAR, MAX_PATH> path;
	const _TCHAR* p = path.data();
	Path long_path;
	if (fi.dir.length() + fi.name.length() >= path.size()) {
		long_path.reserve(fi.dir.length() + fi.name.length() + 5);
		long_path.SetExtendedLength(fi.dir);
		long_path.append(fi.name);
		p = long_path.c_str();
	}
	else {
		size_t offs = fi.dir.copy(path.data(), path.size() - 1);
		offs += fi.name.copy(&path[offs], path.size() - 1 - offs);
		path[offs] = '\0';
	}
	handle = CreateFile(p,
		GENERIC_READ,
		FILE_SHARE_READ | (share_deny_none ? FILE_SHARE_WRITE : 0),
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	return IsValid();
}

bool ArchiveCompressor::FileReader::IsValid() const
{
	return handle != INVALID_HANDLE_VALUE;
}

bool ArchiveCompressor::FileReader::Read(void* buffer, uint_fast32_t byte_count, unsigned long& bytes_read)
{
	return ReadFile(handle, buffer, byte_count, &bytes_read, NULL) != 0;
}

void ArchiveCompressor::FileReader::GetAttributes(FileInfo& fi, bool get_creation_time)
{
	BY_HANDLE_FILE_INFORMATION bhfi;
	if (GetFileInformationByHandle(handle, &bhfi)) {
		fi.attributes.Set(bhfi.dwFileAttributes);
		fi.mod_time.Set(((uint_least64_t)bhfi.ftLastWriteTime.dwHighDateTime << 32)
			| bhfi.ftLastWriteTime.dwLowDateTime);
		if (get_creation_time && (bhfi.ftCreationTime.dwLowDateTime | bhfi.ftCreationTime.dwHighDateTime) != 0) {
			fi.creat_time.Set(((uint_least64_t)bhfi.ftCreationTime.dwHighDateTime << 32)
				| bhfi.ftCreationTime.dwLowDateTime);
		}
		fi.size = (static_cast<uint_least64_t>(bhfi.nFileSizeHigh) << 32) + bhfi.nFileSizeLow;
	}
}

void ArchiveCompressor::FileReader::Close()
{
	if (handle != INVALID_HANDLE_VALUE) {
		CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
	}
}

#else

#include <unistd.h>

ArchiveCompressor::FileReader::FileReader(const FileInfo& fi, bool share_deny_none)
	: fd(-1)
{
	std::array<char, PATH_MAX> path;
	if (fi.dir.length() + fi.name.length() < path.size()) {
		size_t offs = fi.dir.copy(path.data(), path.size() - 1);
		offs += fi.name.copy(&path[offs], path.size() - 1 - offs);
		path[offs] = '\0';
		fd = open(path.data(), O_RDONLY | O_NOATIME);
		if (fd < 0 && O_NOATIME) {
			fd = open(path.data(), O_RDONLY);
		}
	}
}

bool ArchiveCompressor::FileReader::IsValid() const
{
	return fd >= 0;
}

bool ArchiveCompressor::FileReader::Read(void* buffer, uint_fast32_t byte_count, unsigned long& bytes_read)
{
	ssize_t nread = read(fd, buffer, byte_count);
	if (nread < 0) {
		return false;
	}
	else {
		bytes_read = static_cast<unsigned long>(nread);
		return true;
	}
}

void ArchiveCompressor::FileReader::GetAttributes(FileInfo& fi, bool get_creation_time)
{
	static const uint_fast32_t kTicksPerSecond = 10000000;
	static const uint_least64_t kPosixEpochInFiletime = 11644473600LL;
	struct stat st;
	if (fstat(fd, &st) == 0) {
		fi.mod_time.Set(static_cast<uint_least64_t>(st.st_mtime) * kTicksPerSecond + kPosixEpochInFiletime);
		fi.size = st.st_size;
	}
}

void ArchiveCompressor::FileReader::Close()
{
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
}

#endif // _WIN32

ArchiveCompressor::FileReader::~FileReader()
{
	Close();
}

void ArchiveCompressor::Add(const _TCHAR* path, size_t root, uint_least64_t size)
{
#ifndef _WIN32
	struct stat s;
	if (stat(path, &s) == 0) {
		size = s.st_size;
	}
#endif
	size_t name_pos = Path::GetNamePos(path);
	const Path& dir = *path_set.emplace(path, name_pos).first;
	file_list.push_back(FileInfo(dir, path + name_pos, root, size));
	initial_total_bytes += size;
}

uint_least64_t ArchiveCompressor::FinalizeUnit(std::list<CoderInfo>& coder_info, OutputFile& out_stream) {
	unit.pack_size = out_stream.tellp() - unit.out_file_pos;
	// Add the unit
	unit_list.push_back(unit);
	auto& u = unit_list.back();
	u.coder_info = coder_info;
	unit.Reset();
	return u.pack_size;
}

size_t ArchiveCompressor::Read(uint8_t* const buffer, size_t const length)
{
	size_t read_length = 0;
	while (!g_break && read_length < length && cur_file != file_list.end()) {
		if (reader.IsValid()) {
			size_t const to_read = length - read_length;
			unsigned long read_count = 0;
			if (AddFile(buffer + read_length, to_read, read_count)) {
				read_length += read_count;
				if (read_count < to_read) {
					reader.Close();
					// Adjust the total bytes to add if the size was different from when it was opened
					if (cur_file->size != initial_size) {
						progress.Adjust(cur_file->size - initial_size);
					}
					// Only added to the unit if not empty
					if (cur_file->size != 0) {
						unit.unpack_size += cur_file->size;
						++unit.file_count;
						unit.in_file_last = cur_file;
					}
					++cur_file;
				}
			}
			else {
				// Read failure
				reader.Close();
				if (cur_file->size) {
					// Can't recover if some of the file was compressed to the output
					throw IoException(Strings::kUnrecoverableErrorReading, cur_file->name.c_str());
				}
				const _TCHAR* os_msg = IoException::GetOsMessage();
				std::unique_lock<std::mutex> lock(progress.GetMutex());
				progress.RewindLocked();
				file_warnings.emplace_back(Strings::kCannotRead_ + cur_file->dir + cur_file->name + _T(" : ") + os_msg);
				std::Tcerr << file_warnings.back() << std::endl;
				// Adjust the total bytes to add
				progress.Adjust(-static_cast<int_least64_t>(initial_size));
			}
		}
		if (!reader.IsValid() && cur_file != file_list.end()) {
			unsigned prev_ext_index = ext_index;
			ext_index = cur_file->ext_index;
			if(unit.unpack_size >= options.solid_unit_size
				|| unit.file_count >= options.solid_file_count
				|| (options.bcj_filter && prev_ext_index < exe_group && cur_file->ext_index >= exe_group)
				|| (options.solid_by_extension && prev_ext_index != cur_file->ext_index))
			{
				return read_length;
			}
			initial_size = cur_file->size;
			if (!reader.Open(*cur_file, options.share_deny_none)) {
				const _TCHAR* os_msg = IoException::GetOsMessage();
				std::unique_lock<std::mutex> lock(progress.GetMutex());
				progress.RewindLocked();
				file_warnings.emplace_back(Strings::kCannotOpen_ + cur_file->dir + cur_file->name + _T(" : ") + os_msg);
				std::Tcerr << file_warnings.back() << std::endl;
				progress.Adjust(-static_cast<int_least64_t>(initial_size));
				continue;
			}
			reader.GetAttributes(*cur_file, options.store_creation_time);
			// Size may have changed if open for writing
			if (cur_file->size != initial_size) {
				progress.Adjust(cur_file->size - initial_size);
				initial_size = cur_file->size;
			}
			if (!options.quiet_mode) {
				std::unique_lock<std::mutex> lock(progress.GetMutex());
				progress.RewindLocked();
				std::Tcerr << Strings::kAdding_ << (cur_file->dir.c_str() + cur_file->root) << cur_file->name.c_str() << std::endl;
			}
			cur_file->size = 0;
		}
	}
	return read_length;
}

void ArchiveCompressor::EliminateDuplicates()
{
	file_list.sort([](const ArchiveCompressor::FileInfo& first, const ArchiveCompressor::FileInfo& second)
	{
		if (&first.dir == &second.dir) {
			return first.name.FsCompare(second.name) < 0;
		}
		ptrdiff_t comp = first.dir.FsCompare(second.dir);
		if (comp == 0) {
			return first.name.FsCompare(second.name) < 0;
		}
		return comp < 0;
	});
	auto it = file_list.begin();
	auto prev = it;
	for (++it; it != file_list.end();) {
		auto next = std::next(it);
		if ((&it->dir == &prev->dir || it->dir.FsCompare(prev->dir) == 0)
			&& it->name.FsCompare(prev->name) == 0)
		{
			file_list.erase(it);
		}
		else prev = it;
		it = next;
	}
}

void ArchiveCompressor::DetectCollisions()
{
	file_list.sort([](const ArchiveCompressor::FileInfo& first, const ArchiveCompressor::FileInfo& second)
	{
		if (&first.dir == &second.dir) {
			return first.name.FsCompare(second.name) < 0;
		}
		ptrdiff_t comp = first.dir.FsCompare(first.root, second.dir, second.root);
		if (comp == 0) {
			return first.name.FsCompare(second.name) < 0;
		}
		return comp < 0;
	});
	auto it = file_list.begin();
	auto prev = it;
	for (++it; it != file_list.end();) {
		auto next = std::next(it);
		if ((&it->dir == &prev->dir || it->dir.FsCompare(it->root, prev->dir, prev->root) == 0)
			&& it->name.FsCompare(prev->name) == 0)
		{
			std::Tcerr << Strings::kNameCollision_ << (it->dir.c_str() + it->root) << it->name << std::endl;
			throw std::invalid_argument("");
		}
		else prev = it;
		it = next;
	}
}

bool ArchiveCompressor::AddFile(uint8_t* buffer,
	size_t length,
	unsigned long& read_count)
{
	unsigned long to_read = static_cast<unsigned long>(
		std::min<size_t>(length, ~0ul));
	read_count = 0;
	if (!reader.Read(buffer, to_read, read_count)) {
		return false;
	}
	// Update the CRC
	cur_file->crc32.Add(buffer, read_count);
	// Update file size
	cur_file->size += read_count;
	return true;
}

ArchiveCompressor::ArchiveCompressor(const RadyxOptions& options_, Progress& progress_)
	: options(options_),
	progress(progress_),
	initial_size(0),
	initial_total_bytes(0),
	exe_group(GetExtensionIndex(_T("exe"))),
	ext_index(~0UL)
{
}

void ArchiveCompressor::InitUnit(OutputFile& out_stream)
{
	unit.in_file_first = cur_file;
	if (unit_list.size() > 0) {
		unit.out_file_pos = unit_list.back().out_file_pos + unit_list.back().pack_size;
	}
	else {
		unit.out_file_pos = out_stream.tellp();
	}
}

static bool CompareFileInfo(const ArchiveCompressor::FileInfo& first, const ArchiveCompressor::FileInfo& second)
{
	if (first.ext_index != second.ext_index) {
		return first.ext_index < second.ext_index;
	}
	ptrdiff_t comp = first.name.FsCompare(first.ext, std::string::npos, second.name, second.ext, std::string::npos);
	if (comp == 0) {
		return first.name.FsCompare(second.name) < 0;
	}
	return comp < 0;
}

void ArchiveCompressor::PrepareList()
{
	EliminateDuplicates();
	if (options.store_full_paths) {
		for (auto& fs : file_list) {
			fs.root = 0;
		}
	}
	else {
		DetectCollisions();
	}
	// Sort the file list by extension index then name
	file_list.sort(CompareFileInfo);
	cur_file = file_list.begin();
	ext_index = cur_file->ext_index;
}

// Get the index of this extension in the list
unsigned ArchiveCompressor::GetExtensionIndex(const _TCHAR* ext)
{
	unsigned index = 1;
	for (const _TCHAR* p = extensions; *p != 0; ++p) {
		if (_tcsicmp(ext, p) == 0) {
			return index;
		}
		while (*p != 0) {
			++p;
		}
		++index;
	}
	return 0;
}

// The number of empty files
size_t ArchiveCompressor::GetEmptyFileCount() const
{
	size_t count = 0;
	for (const auto& it : file_list) {
		count += it.IsEmpty();
	}
	return count;
}

// Total character length of all file names including terminating nuls
size_t ArchiveCompressor::GetNameLengthTotal() const
{
	size_t total = 0;
	for (const auto& it : file_list) {
		total += it.dir.length() - it.root + it.name.length() + 1;
	}
	return total;
}

} // Radyx