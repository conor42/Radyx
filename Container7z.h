///////////////////////////////////////////////////////////////////////////////
//
// Class:   Container7z
//          Write the .7z container format
//
// Authors: Conor McCarthy
//          Igor Pavlov
//
// Copyright 2015 Conor McCarthy
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
#include "CompressorInterface.h"
#include "CompressedUint64.h"
#include "ThreadPool.h"
#include "OutputFile.h"
#include "CharType.h"

namespace Radyx {

class Container7z
{
public:
	static void ReserveSignatureHeader(OutputStream& out_stream);
	static uint_least64_t WriteDatabase(const ArchiveCompressor& arch_comp,
		UnitCompressor& unit_comp,
		CompressorInterface& compressor,
		ThreadPool& threads,
		OutputStream& out_stream);

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

	class Writer
	{
	public:
		Writer(UnitCompressor& unit_comp_, CompressorInterface* compressor_, ThreadPool& threads_, OutputStream& out_stream_)
			: unit_comp(unit_comp_),
			compressor(compressor_),
			threads(threads_),
			out_stream(out_stream_) {}
		~Writer() { Flush(); }
		inline void WriteByte(uint8_t byte);
		inline void WriteBytes(const uint8_t* buf, size_t count);
		inline void WriteUint32(uint_fast32_t value);
		inline void WriteUint64(uint_least64_t value);
		inline void WriteCompressedUint64(uint_least64_t value);
		void WriteName(const FsString& name, size_t root);
		void Flush();
		uint_fast32_t GetCrc32() const { return crc32; }

	private:
		UnitCompressor& unit_comp;
		CompressorInterface* compressor;
		ThreadPool& threads;
		OutputStream& out_stream;
		Crc32 crc32;

		Writer(const Writer&) = delete;
		Writer& operator=(const Writer&) = delete;
	};

	class BoolWriter
	{
	public:
		BoolWriter(Writer& writer_) : writer(writer_), mask(0x80), value(0) {}
		~BoolWriter() { Flush(); }
		inline void Write(bool b);
		inline void Flush();
		static inline size_t GetByteCount(size_t bool_count) {
			return (bool_count >> 3) + ((bool_count & 7) != 0); }

	private:
		Writer& writer;
		uint8_t mask;
		uint8_t value;

		BoolWriter(const BoolWriter&) = delete;
		BoolWriter& operator=(const BoolWriter&) = delete;
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
		UnitCompressor& unit_comp,
		CompressorInterface &compressor,
		ThreadPool& threads,
		OutputStream& out_stream);
	static uint_fast32_t WriteHeaderHeader(UnitCompressor& unit_comp,
		CompressorInterface& compressor,
		uint_least64_t header_offset,
		uint_least64_t header_pack_size,
		uint_least64_t header_unpack_size,
		ThreadPool& threads,
		OutputStream& out_stream);
	static void WriteSignatureHeader(uint_least64_t header_offset,
		uint_least64_t header_size,
		uint_fast32_t crc32,
		OutputStream& out_stream);
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
	if (unit_comp.GetAvailableSpace() == 0) {
		Flush();
	}
	unit_comp.GetAvailableBuffer()[0] = byte;
	unit_comp.AddByteCount(1);
	crc32.Add(byte);
}

void Container7z::Writer::WriteBytes(const uint8_t* buf, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		WriteByte(buf[i]);
	}
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