///////////////////////////////////////////////////////////////////////////////
//
// Radyx main function 
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
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#endif
#include <csignal>
#include "../winlean.h"
#include "../common.h"
#include "../OutputFile.h"
#include "../CharType.h"
#include "../Lzma2Compressor.h"
#include "../ArchiveCompressor.h"
#include "../ArchiveStreamIn.h"
#include "../Bcjx86.h"
#include "../Container7z.h"
#include "../RadyxOptions.h"
#include "../IoException.h"
#include "../Strings.h"
#include "../RadyxLzma2Enc.h"

using namespace Radyx;

static const char kProgName[] = "Radyx";
static const char kVersion[] = "0.9.2 beta";
static const char kCopyright[] = "  Copyright 2017 Conor McCarthy";
static const char kReleaseDate[] = "  2017-11-19";
static const char kLicense[] = "This software has NO WARRANTY and is released under the\nGNU General Public License: www.gnu.org/licenses/gpl.html\n";

static const uint_least64_t kMinMemory = 512 * 1024 * 1024;

volatile bool Radyx::g_break = false;

void SignalHandler(int /*param*/)
{
	g_break = true;
}

void PrintBanner()
{
	std::Tcerr << kProgName
		<< ((sizeof(void*) > 4) ? " [64] " : " [32] ")
		<< kVersion
		<< kCopyright
		<< kReleaseDate
		<< std::endl
		<< kLicense
		<< std::endl;
}

bool OpenOutputStream(const Path& archive_path, const RadyxOptions& options, OutputFile& file_stream, uint_least64_t avail_mem)
{
	bool is_dev_null = archive_path.IsDevNull();
#ifdef _WIN32
	if (!is_dev_null
		&& GetFileAttributes(archive_path.c_str()) != INVALID_FILE_ATTRIBUTES)
#else
	struct stat s;
	if (!is_dev_null && stat(archive_path.c_str(), &s) == 0)
#endif
	{
		std::Tcerr << Strings::kErrorCol_ << Strings::kArchiveFileExists << std::endl;
		throw std::invalid_argument("");
	}
#ifdef _WIN32
	bool no_caching = false;
	if (avail_mem != 0) {
		no_caching = (avail_mem < options.lzma2.dictionary_size ||
			(avail_mem - options.lzma2.dictionary_size < kMinMemory));
	}
	file_stream.open(archive_path.c_str(), no_caching);
#else
	file_stream.open(archive_path.c_str(), std::ios_base::trunc | std::ios_base::binary);
#endif
	if (file_stream.fail()) {
		throw IoException(Strings::kCannotCreateArchive, archive_path.c_str());
	}
	return !is_dev_null;
}

int CompressFiles(Path& archive_path, RadyxOptions& options, bool& created_file)
{
	uint_least64_t avail_mem = 0;
#ifdef _WIN32
	MEMORYSTATUSEX msx;
	msx.dwLength = sizeof(msx);
	if (GlobalMemoryStatusEx(&msx) == TRUE) {
		avail_mem = msx.ullAvailPhys;
	}
#endif
	Progress progress;
	ArchiveCompressor ar_comp(options, progress);
	std::Tcerr << Strings::kSearching;
	options.GetFiles(ar_comp);
	ar_comp.PrepareList();
	for (size_t i = _tcslen(Strings::kSearching); i > 0; --i) {
		std::Tcerr << '\b';
	}
	if (g_break) {
		return EXIT_FAILURE;
	}
	if (ar_comp.GetFileList().size() == 0) {
		std::Tcerr << Strings::kNoFilesFound << std::endl;
		return EXIT_SUCCESS;
	}
	FilterList filters;
	if (options.bcj_filter) {
		filters.emplace_back(std::unique_ptr<BcjX86>(new BcjX86(&ar_comp)));
	}
	OutputFile out_stream;
	RadyxLzma2Enc encoder(out_stream);
	size_t read_extra = 0;
	for (auto& f : filters) {
		read_extra = std::max(read_extra, f->GetMaxOverrun());
	}
	encoder.Create(options.lzma2, read_extra, options.thread_count);
	avail_mem -= encoder.GetMemoryUsage();
	created_file = OpenOutputStream(archive_path, options, out_stream, avail_mem);
	Container7z::ReserveSignatureHeader(out_stream);
#ifdef _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#else
	if (nice(1) < 0) {
	}
#endif
	progress.Init(ar_comp.GetTotalByteCount(), encoder.GetEncodeWeight());
	uint_least64_t packed = 0;
	std::list<CoderInfo> coder_info;
	while (!g_break && !ar_comp.Complete()) {
		ar_comp.InitUnit(out_stream);
		encoder.Encode(&ar_comp, &filters, coder_info, &progress);
		packed += encoder.GetPackSize();
		ar_comp.FinalizeUnit(coder_info, out_stream);
	}
	if (ar_comp.GetFileCount() != 0 && !g_break) {
		packed += Container7z::WriteDatabase(ar_comp, encoder, out_stream);
		if (!created_file) {
			std::Tcerr << "Compressed size: " << packed << " bytes" << std::endl;
		}
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	signal(SIGINT, SignalHandler);
	PrintBanner();
	bool created_file = false;
	Path archive_path;
	try {
		RadyxOptions options(argc, argv, archive_path);
		return CompressFiles(archive_path, options, created_file);
	}
	catch (std::invalid_argument& ex) {
		if (*ex.what() != '\0') {
			std::Tcerr << Strings::kErrorCol_ << ex.what() << std::endl;
		}
	}
	catch (std::bad_alloc&) {
		std::Tcerr << Strings::kErrorNotEnoughMem << std::endl;
	}
	catch (IoException& ex) {
		std::Tcerr << Strings::kErrorCol_ << ex.Twhat() << std::endl;
	}
	catch (std::exception& ex) {
		if (*ex.what() != '\0') {
			std::Tcerr << Strings::kErrorCol_ << ex.what() << std::endl;
		}
	}
	if (created_file) {
		_tremove(archive_path.c_str());
	}
	return EXIT_FAILURE;
}
