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
#ifdef RADYX_RANDOM_TEST
#include <random>
#endif
#include "CharType.h"
#include "ArchiveCompressor.h"
#include "Strings.h"
#include "IoException.h"
#include "fast-lzma2/fl2_errors.h"

namespace Radyx {

#ifdef RADYX_RANDOM_TEST
extern uint_least64_t g_testSize = 256U << 20;
#endif

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

ArchiveCompressor::FileReader::FileReader(const FileInfo& fi, bool share_deny_none)
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
}

ArchiveCompressor::FileReader::~FileReader()
{
	if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
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

#else

ArchiveCompressor::FileReader::FileReader(const FileInfo& fi, bool share_deny_none)
	: fd(-1)
{
	std::array<char, PATH_MAX> path;
	if (fi.dir.length() + fi.name.length() < path.size()) {
		size_t offs = fi.dir.copy(path.data(), path.size() - 1);
		offs += fi.name.copy(&path[offs], path.size() - 1 - offs);
		path[offs] = '\0';
		fd = open(path.data(), O_RDONLY | O_NOATIME);
		if(fd < 0 && O_NOATIME) {
			fd = open(path.data(), O_RDONLY);
		}
	}
}

ArchiveCompressor::FileReader::~FileReader()
{
	if (fd >= 0) {
		close(fd);
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

#endif // _WIN32

ArchiveCompressor::ArchiveCompressor()
	: initial_total_bytes(0)
{
	assert(GetExtensionIndex(_T("out")) != 0);
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

void ArchiveCompressor::PrepareFileList(const RadyxOptions& options)
{
    if (file_list.size() == 0)
        return;

#ifdef RADYX_RANDOM_TEST
    for (auto it = file_list.begin(); it != file_list.end(); ) {
        FileReader reader(*it, options.share_deny_none);
        auto old_it = it;
        ++it;
        if (!reader.IsValid())
            file_list.erase(old_it);
    }
#endif

    EliminateDuplicates();
    if (options.store_full_paths) {
        for (auto& fi : file_list) {
            fi.root = 0;
        }
    }
    else {
        DetectCollisions();
    }
    // Sort the file list by extension index then name
    file_list.sort(CompareFileInfo);
}

#ifdef RADYX_RANDOM_TEST

template<typename T>
T RandomLogDistribution(std::mt19937& gen, unsigned logMin, unsigned logMax)
{
    auto value = T(1) << std::uniform_int_distribution<T>(logMin, logMax - 1)(gen);
    return value + std::uniform_int_distribution<T>(0, value)(gen);
}

static void SetRandomOptions(std::mt19937& gen, Lzma2Options& lzma2)
{
    static std::array<unsigned, 26> overlaps = { 0,1,2,2,2,2,2,3,3,3,3,3,4,4,4,5,5,6,7,8,9,10,11,12,13,14 };
    std::uniform_int_distribution<unsigned> dist_lc(FL2_LC_MIN, FL2_LC_MAX);
    lzma2.lc = dist_lc(gen);
    std::uniform_int_distribution<unsigned> dist_lp(FL2_LP_MIN, FL2_LCLP_MAX - lzma2.lc);
    lzma2.lp = dist_lp(gen);
    lzma2.pb = dist_lc(gen);
    lzma2.fast_length = std::uniform_int_distribution<unsigned>(FL2_FASTLENGTH_MIN, FL2_FASTLENGTH_MAX)(gen);
    lzma2.search_depth = std::min(FL2_SEARCH_DEPTH_MIN - 4 + RandomLogDistribution<unsigned>(gen, 2, 8), unsigned(FL2_SEARCH_DEPTH_MAX));
    lzma2.match_cycles = std::uniform_int_distribution<unsigned>(FL2_HYBRIDCYCLES_MIN, FL2_HYBRIDCYCLES_MAX)(gen);
    lzma2.encoder_mode = Lzma2Options::Mode(std::uniform_int_distribution<int>(Lzma2Options::kFastMode, Lzma2Options::kBestMode)(gen));
    lzma2.second_dict_size = 1U << std::uniform_int_distribution<unsigned>(FL2_CHAINLOG_MIN, FL2_CHAINLOG_MAX)(gen);
    if(dist_lc(gen) > 1)
        lzma2.dictionary_size = RandomLogDistribution<size_t>(gen, FL2_DICTLOG_MIN, 28);
    else
        lzma2.dictionary_size = size_t(1) << std::uniform_int_distribution<size_t>(FL2_DICTLOG_MIN, 28)(gen);
    lzma2.match_buffer_log = std::uniform_int_distribution<unsigned>(FL2_BUFFER_SIZE_LOG_MIN, FL2_BUFFER_SIZE_LOG_MAX)(gen);
    lzma2.divide_and_conquer = std::uniform_int_distribution<unsigned>(0, 1)(gen);
    lzma2.block_overlap = overlaps[std::uniform_int_distribution<size_t>(0, overlaps.size() - 1)(gen)];
    fprintf(stderr, "lc=%u lp=%u pb=%u fb=%u sd=%u mc=%u mode=%u dict=%u dict_2=%u buf=%u q=%u ov=%u\r\n",
        lzma2.lc, lzma2.lp, lzma2.pb, lzma2.fast_length.Get(), lzma2.search_depth.Get(), lzma2.match_cycles.Get(), lzma2.encoder_mode.Get(),
        (unsigned)lzma2.dictionary_size.Get(), lzma2.second_dict_size.Get(), lzma2.match_buffer_log.Get(), lzma2.divide_and_conquer.Get(), lzma2.block_overlap.Get());
}

#endif

uint_least64_t ArchiveCompressor::Compress(FastLzma2& enc,
	const RadyxOptions& options,
	OutputStream& out_stream)
{
	if (file_list.size() == 0) {
		return 0;
	}
#ifdef RADYX_RANDOM_TEST
    std::vector<FileInfo*> file_vec;
    file_vec.reserve(file_list.size());
    for (auto& fi : file_list) {
        file_vec.push_back(&fi);
        fi.include = false;
    }
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    std::Tcerr << "Seed: " << li.LowPart << std::endl;
    std::mt19937 gen(li.LowPart);
    Lzma2Options lzma2;
    SetRandomOptions(gen, lzma2);
    enc.SetOptions(lzma2);
    std::uniform_int_distribution<size_t> file_num(0, file_vec.size() - 1);
    uint_least64_t total = 0;
    for (size_t i = 0; i < (file_vec.size() << 3) && total < g_testSize; ++i) {
        size_t index = file_num(gen);
        if (!file_vec[index]->include && file_vec[index]->size <= g_testSize) {
            file_vec[index]->include = true;
            total += file_vec[index]->size;
        }
    }
    file_list_copy = std::move(file_list);
    file_list = std::list<FileInfo>();
    initial_total_bytes = 0;
    for (auto& fi : file_list_copy) {
        if (fi.include) {
            file_list.push_back(fi);
            initial_total_bytes += fi.size;
        }
    }
#endif
    enc.SetTimeout(500);
	auto it = file_list.begin();
	DataUnit unit;
	unit.out_file_pos = out_stream.tellp();
	uint_least64_t packed_size = 0;
	std::Tcerr << Strings::kFound_ << file_list.size();
	std::Tcerr << (file_list.size() > 1 ? Strings::k_files : Strings::k_file) << std::endl;
	unsigned exe_group = GetExtensionIndex(_T("exe"));
    enc.Begin(options.bcj_filter && it->ext_index >= exe_group);
	Progress progress(initial_total_bytes);
    for (;;) {
		unsigned ext_index = it->ext_index;
		if(!AddFile(*it, enc, options, progress, out_stream)) {
			auto old_it = it;
			++it;
			//Delete it from the file list if not read
			file_list.erase(old_it);
		}
		else if (!g_break) {
			// Only added to the unit if not empty
			if (it->size != 0) {
				unit.unpack_size += it->size;
				if (unit.file_count == 0) {
					unit.in_file_first = it;
				}
				++unit.file_count;
				unit.in_file_last = it;
			}
			++it;
		}
        else {
            // Break signaled
            // Compression could be occuring asynchronously
            enc.Cancel();
            progress.Erase();
            throw std::runtime_error(Strings::kBreakSignaled);
        }
		// Criteria for ending the solid unit and maybe starting a new one
		if (unit.unpack_size >= options.solid_unit_size
			|| unit.file_count >= options.solid_file_count
			|| it == file_list.end()
			|| (options.bcj_filter && ext_index < exe_group && it->ext_index >= exe_group)
			|| (options.solid_by_extension && ext_index != it->ext_index))
		{
			// If any data was added, compress what remains and add the unit to the list
			if (unit.unpack_size != 0) {
				progress.Show();
                packed_size += enc.Finalize(out_stream, &progress);
				unit.used_bcj = enc.UsedBcj();
				if (unit.used_bcj) {
					unit.bcj_info = enc.GetBcjCoderInfo();
				}
                unit.coder_info = enc.GetCoderInfo();
				// Get final file position and the unit packed size
				uint_least64_t out_file_pos = out_stream.tellp();
				unit.pack_size = out_file_pos - unit.out_file_pos;
				// Add the unit
				unit_list.push_back(unit);
				// Starting pos for the next unit
				unit.out_file_pos = out_file_pos;
			}
			if (it == file_list.end()) {
				break;
			}
			// Reset the unit compressor, turning on BCJ if adding executables
            enc.Begin(options.bcj_filter && it->ext_index >= exe_group);
            progress.AddUnit(unit.unpack_size);
            unit.file_count = 0;
			unit.unpack_size = 0;
		}
	}
    progress.Erase();
	// Warn if any files couldn't be read
	if (!g_break && !file_warnings.empty()) {
		if (!options.quiet_mode && !file_list.empty()) {
			std::Tcerr << std::endl << Strings::kWarningsForFiles << std::endl;
			for (auto& msg : file_warnings) {
				std::Tcerr << msg << std::endl;
			}
		}
		std::Tcerr << std::endl
			<< Strings::kWarningCouldntOpen_
			<< file_warnings.size()
			<< (file_warnings.size() > 1 ? Strings::k_files : Strings::k_file)
			<< std::endl;
	}
    return packed_size;
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

bool ArchiveCompressor::AddFile(FileInfo& fi,
    FastLzma2& enc,
    const RadyxOptions& options,
	Progress& progress,
	OutputStream& out_stream)
{
	uint_least64_t initial_size = fi.size;
	FileReader reader(fi, options.share_deny_none);
	if (!reader.IsValid()) {
		const _TCHAR* os_msg = IoException::GetOsMessage();
		std::unique_lock<std::mutex> lock(progress.GetMutex());
		progress.RewindLocked();
		file_warnings.emplace_back(Strings::kCannotOpen_ + fi.dir + fi.name + _T(" : ") + os_msg);
		std::Tcerr << file_warnings.back() << std::endl;
		progress.Adjust(-static_cast<int_least64_t>(initial_size));
		return false;
	}
	reader.GetAttributes(fi, options.store_creation_time);
	// Size may have changed if open for writing
	if (fi.size != initial_size) {
		progress.Adjust(fi.size - initial_size);
		initial_size = fi.size;
	}
	if (!options.quiet_mode) {
		std::unique_lock<std::mutex> lock(progress.GetMutex());
		progress.RewindLocked();
		std::Tcerr << Strings::kAdding_ << (fi.dir.c_str() + fi.root) << fi.name.c_str() << std::endl;
	}
	fi.size = 0;
	bool did_read = false;
	while (!g_break) {
        unsigned long size;
        uint8_t* dst = enc.GetAvailableBuffer(size);
        unsigned long read_count;
		if (!reader.Read(dst, size, read_count)) {
			// Read failure
			if (did_read) {
				// Can't recover if some of the file was compressed to the output
				throw IoException(Strings::kUnrecoverableErrorReading, fi.name.c_str());
			}
			const _TCHAR* os_msg = IoException::GetOsMessage();
			file_warnings.emplace_back(Strings::kCannotRead_ + fi.dir + fi.name + _T(" : ") + os_msg);
			std::Tcerr << file_warnings.back() << std::endl;
			return false;
		}
		if (read_count == 0)
			break;
        if (g_break)
            return true;
        // Update the CRC
		fi.crc32.Add(dst, read_count);
		// Update file size and the unit compressor's buffer pos
		fi.size += read_count;

        enc.AddByteCount(read_count, out_stream, &progress);

        did_read = true;
	}
	// Adjust the total bytes to add if the size was different from when it was opened
	if (!g_break && fi.size != initial_size) {
		progress.Adjust(fi.size - initial_size);
	}
	return true;
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

#ifdef RADYX_RANDOM_TEST

void ArchiveCompressor::RestoreFileList()
{
    file_list = std::move(file_list_copy);
    unit_list.clear();
    file_warnings.clear();
}

#endif

}