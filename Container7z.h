///////////////////////////////////////////////////////////////////////////////
//
// Class:   Container7z
//          Write the .7z container format
//
// Authors: Conor McCarthy
//          Igor Pavlov
//
// Copyright 2015-present Conor McCarthy
// Based on 7-zip 9.20 copyright 2010 Igor Pavlov
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

#ifndef RADYX_CONTAINER_7Z_H
#define RADYX_CONTAINER_7Z_H

#include "ArchiveCompressor.h"
#include "UnitCompressor.h"
#include "CompressedUint64.h"
#include "OutputFile.h"
#include "CharType.h"

namespace Radyx {

class Container7z
{
public:
	class Database7z
	{
	private:
		struct SignatureHeader
		{
			uint8_t signature[6];
			uint8_t version_major;
			uint8_t version_minor;
			uint32_t crc32;
			uint64_t next_header_pos;
			uint64_t next_header_size;
			uint32_t next_header_crc32;
		};

		struct FolderCoderInfo
		{
			CoderInfo coder_info;
			uint_least64_t unpack_size;
		};

		struct SubStreamInfo
		{
			uint_least64_t unpack_size;
			uint_fast32_t crc32;
		};

		struct Folder
		{
			std::vector<FolderCoderInfo> coders_info;
			std::vector<std::pair<uint_least64_t, uint_least64_t>> bind_pairs;
			uint_fast32_t crc32;
			std::vector<SubStreamInfo> substreams_info;
		};

		struct StreamsInfo
		{
			uint_least64_t file_pos;
			std::vector<uint_least64_t> pack_sizes;
			std::vector<uint_fast32_t> crc32s;
			std::vector<Folder> folders;
		};

		struct FileInfo
		{
			FsString name;
			uint_least64_t size;
			OptionalSetting<uint_least64_t> creat_time;
			OptionalSetting<uint_least64_t> mod_time;
			OptionalSetting<uint_fast32_t> attributes;
			uint_fast32_t crc32;
			bool empty;
		};
	public:
		Database7z(int hndl, Path& path);
		void ReadHeader(int hndl, uint64_t pos, uint64_t size, uint32_t crc32);

	private:
		bool ReadStreamsInfo(const uint8_t* data, const uint8_t* end, StreamsInfo& streams_info);
		uint8_t* ReadFolderInfo(const uint8_t* data, const uint8_t* end, Folder& folder);
		std::vector<StreamsInfo> streams_info;
		std::vector<FileInfo> files_info;
	};

	static void ReserveSignatureHeader(OutputFile& out_stream);
	static uint_least64_t WriteDatabase(const ArchiveCompressor& arch_comp,
		UnitCompressor& unit_comp,
		OutputFile& out_stream);

private:
	enum PropertyId
	{
		kEnd,
		kHeader,
		kArchiveProperties,
		kAdditionalStreamsInfo,
		kMainStreamsInfo,
		kFilesInfo,
		kPackInfo,
		kUnpackInfo,
		kSubStreamsInfo,
		kSize,
		kCRC,
		kFolder,
		kCodersUnpackSize,
		kNumUnpackStream,
		kEmptyStream,
		kEmptyFile,
		kAnti,
		kName,
		kCTime,
		kATime,
		kMTime,
		kWinAttributes,
		kComment,
		kEncodedHeader,
		kStartPos
	};

	class CompressedStreamWrapper : public OutputStream
	{
	public:
		CompressedStreamWrapper(OutputStream& out_stream_, UnitCompressor& unit_comp_)
			: out_stream(out_stream_),
			unit_comp(unit_comp_)
		{ }
		OutputStream& Write(const void* data, size_t count) {
			unit_comp.Write(data, count, out_stream);
			return *this;
		}

		OutputStream& Put(char c) {
			Write(&c, 1);
			return *this;
		}
		void Flush() {
			if (unit_comp.Unprocessed()) {
				unit_comp.Compress(nullptr, out_stream, nullptr);
			}
			unit_comp.WaitCompletion();
		}
		void End() {
			Flush();
			unit_comp.Finalize(out_stream);
		}
		void DisableExceptions() {}
		void RestoreExceptions() {}
		bool Fail() const noexcept {
			return false;
		}
	private:
		OutputStream & out_stream;
		UnitCompressor& unit_comp;
	};

	class Writer
	{
	public:
		Writer(OutputStream& out_stream_)
			: out_stream(out_stream_) {}
		~Writer() {
			Flush();
		}
		inline void WriteByte(uint8_t byte);
		inline void WriteBytes(const uint8_t* buf, size_t count);
		inline void WriteUint32(uint_fast32_t value);
		inline void WriteUint64(uint_least64_t value);
		inline void WriteCompressedUint64(uint_least64_t value);
		void WriteName(const FsString& name, size_t root);
		void Flush();
		uint_fast32_t GetCrc32() const {
			return crc32;
		}

	private:
		OutputStream& out_stream;
		Crc32 crc32;
        bool compress;

		Writer(const Writer&) = delete;
		Writer& operator=(const Writer&) = delete;
		Writer(Writer&&) = delete;
		Writer& operator=(Writer&&) = delete;
	};

	class BoolWriter
	{
	public:
		BoolWriter(Writer& writer_) : writer(writer_), mask(0x80), value(0) {}
		~BoolWriter() {
			Flush();
		}
		inline void Write(bool b);
		inline void Flush();
		static inline size_t GetByteCount(size_t bool_count) {
			return (bool_count >> 3) + ((bool_count & 7) != 0);
		}

	private:
		Writer& writer;
		uint8_t mask;
		uint8_t value;

		BoolWriter(const BoolWriter&) = delete;
		BoolWriter& operator=(const BoolWriter&) = delete;
		BoolWriter(BoolWriter&&) = delete;
		BoolWriter& operator=(BoolWriter&&) = delete;
	};

	static const uint8_t kMajorVersion = 0;
	static const uint8_t kMinorVersion = 3;
	static const char kSignature[6];
	static const unsigned kSignatureHeaderSize = 32;
	static const unsigned kAttributesItemSize = 4;
	static const unsigned kFileTimeItemSize = 8;
	static const size_t kNameConverterBufferSize = 4096;

	static void WritePackInfo(const ArchiveCompressor& arch_comp, uint_least64_t file_pos, Writer& writer);
	static void WriteUnpackInfo(const ArchiveCompressor& arch_comp, Writer& writer);
	static void WriteUnitInfo(const CoderInfo& coder_info, Writer& writer);
	static void WriteSubStreamsInfo(const ArchiveCompressor& arch_comp, Writer& writer);
	template<typename AccessorFunc, typename WriterFunc>
	static void WriteOptionalAttribute(const ArchiveCompressor& arch_comp,
		size_t file_count,
		uint8_t property_id,
		unsigned item_size,
		AccessorFunc accessor,
		WriterFunc write_func,
		Writer& writer);
	static void WriteHeader(const ArchiveCompressor& arch_comp,
		OutputStream& out_stream);
	static uint_fast32_t WriteHeaderHeader(uint_least64_t header_offset,
		uint_least64_t header_pack_size,
		uint_least64_t header_unpack_size,
		CoderInfo& coder_info,
		OutputFile& out_stream);
	static void WriteSignatureHeader(uint_least64_t header_offset,
		uint_least64_t header_size,
		uint_fast32_t crc32,
		OutputFile& out_stream);
	static void WriteUint32(uint_fast32_t value, uint8_t* buffer);
	static void WriteUint64(uint_least64_t value, uint8_t* buffer);
};

void Container7z::BoolWriter::Write(bool b)
{
	if (b) {
		value |= mask;
	}
	mask >>= 1;
	if (mask == 0) {
		writer.WriteByte(value);
		mask = 0x80;
		value = 0;
	}
}

void Container7z::BoolWriter::Flush()
{
	if (mask != 0x80) {
		writer.WriteByte(value);
		mask = 0x80;
		value = 0;
	}
}

void Container7z::Writer::WriteByte(uint8_t byte)
{
	out_stream.Put(byte);
	crc32.Add(byte);
}

void Container7z::Writer::WriteBytes(const uint8_t* buf, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		crc32.Add(buf[i]);
	}
	out_stream.Write(buf, count);
}

void Container7z::Writer::WriteUint32(uint_fast32_t value)
{
	for (size_t i = 0; i < 4; ++i) {
		WriteByte(static_cast<uint8_t>(value));
		value >>= 8;
	}
}

void Container7z::Writer::WriteUint64(uint_least64_t value)
{
	for (size_t i = 0; i < 8; ++i) {
		WriteByte(static_cast<uint8_t>(value));
		value >>= 8;
	}
}

void Container7z::Writer::WriteCompressedUint64(uint_least64_t value)
{
	CompressedUint64 ui(value);
	WriteBytes(ui.GetValue(), ui.GetSize());
}

}

#endif // RADYX_CONTAINER_7Z_H