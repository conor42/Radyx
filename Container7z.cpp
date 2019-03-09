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

#include <array>
#if (defined __GNUC__ && __GNUC__ >= 5) || (defined __clang_major__ && (__clang_major__ * 100 + __clang_minor__) >= 303)
#define HAVE_CODECVT
#include <codecvt>
#elif !defined(_UNICODE)
#include "../utf8cpp/source/utf8.h" // https://github.com/nemtrif/utfcpp
#endif
#include "common.h"
#include "Container7z.h"
#include "CompressedUint64.h"
#include "Crc32.h"
#include "IoException.h"
#include "Strings.h"

namespace Radyx {

const char Container7z::kSignature[6] = { '7', 'z', '\xBC', '\xAF', '\x27', '\x1C' };

Container7z::Writer::Writer(FastLzma2& unit_comp_, OutputStream& out_stream_, bool compress_)
    : unit_comp(unit_comp_),
    out_stream(out_stream_),
    compress(compress_)
{
    unit_comp_.Begin(false);
}

void Container7z::Writer::WriteName(const FsString& name, size_t root)
{
	if (root >= name.length())
		return;
#ifdef _UNICODE
	for (size_t i = root; i < name.length(); ++i) {
		WriteByte(static_cast<uint8_t>(name[i]));
		WriteByte(static_cast<uint8_t>(name[i] >> 8));
	}
#elif defined HAVE_CODECVT
	std::codecvt_utf8_utf16<char16_t> converter;
	char16_t buf_char_16[kNameConverterBufferSize];
	mbstate_t mbs = std::mbstate_t();
	const char* next1;
	char16_t* next2;
	std::codecvt_base::result res = converter.in(mbs,
		name.c_str() + root,
		name.c_str() + name.length(),
		next1,
		buf_char_16,
		buf_char_16 + kNameConverterBufferSize,
		next2);
	if (res == std::codecvt_base::noconv || next2 == buf_char_16) {
		throw std::runtime_error(Strings::kUnableConvertUtf8to16);
	}
	for (const char16_t* src = buf_char_16; src < next2; ++src) {
		WriteByte(static_cast<uint8_t>(*src));
		WriteByte(static_cast<uint8_t>(*src >> 8));
	}
#else
	std::u16string utf16name;
	utf16name.reserve(name.length() - root + 1);
	utf8::utf8to16(name.c_str() + root,
		name.c_str() + name.length(),
		std::back_inserter(utf16name));
	for (auto it : utf16name) {
		WriteByte(static_cast<uint8_t>(it));
		WriteByte(static_cast<uint8_t>(it >> 8));
	}
#endif
}

void Container7z::Writer::WriteByte(uint8_t byte)
{
    unsigned long size;
    unit_comp.GetAvailableBuffer(size)[0] = byte;
    assert(size != 0);
    if (compress) {
        unit_comp.AddByteCount(1, out_stream, nullptr);
    }
    else {
        unit_comp.IncBufferCount(out_stream);
    }
    crc32.Add(byte);
}

void Container7z::Writer::Flush()
{
	if (compress) {
		unit_comp.Finalize(out_stream, nullptr);
	}
	else {
		unit_comp.Write(out_stream);
	}
}

void Container7z::ReserveSignatureHeader(OutputStream& out_stream)
{
	std::array<char, 32> buf;
	buf.fill(0);
	out_stream.write(buf.data(), buf.size());
	if (out_stream.fail()) {
		throw IoException(Strings::kCannotWriteArchive, _T(""));
	}
}

void Container7z::WriteSignatureHeader(uint_least64_t header_offset,
	uint_least64_t header_size,
	uint_fast32_t header_crc32,
	OutputStream& out_stream)
{
	out_stream.seekp(0);
	out_stream.write(kSignature, sizeof(kSignature));
	std::array<uint8_t, kSignatureHeaderSize - 8> buf;
	buf[0] = kMajorVersion;
	buf[1] = kMinorVersion;
	out_stream.write(reinterpret_cast<char*>(buf.data()), 2);
	WriteUint64(header_offset, &buf[4]);
	WriteUint64(header_size, &buf[12]);
	WriteUint32(header_crc32, &buf[20]);
	Crc32 crc32;
	crc32.Add(&buf[4], 20);
	WriteUint32(crc32, buf.data());
	out_stream.write(reinterpret_cast<char*>(buf.data()), buf.size());
}

void Container7z::WritePackInfo(const ArchiveCompressor& arch_comp, uint_least64_t file_pos, Writer& writer)
{
	if (arch_comp.GetUnitList().size() == 0) {
		return;
	}
	writer.WriteByte(kPackInfo);
	writer.WriteCompressedUint64(file_pos);
	writer.WriteCompressedUint64(arch_comp.GetUnitList().size());
	writer.WriteByte(kSize);
	for (auto& it : arch_comp.GetUnitList()) {
		writer.WriteCompressedUint64(it.pack_size);
	}
	writer.WriteByte(kEnd);
}

void Container7z::WriteUnpackInfo(const ArchiveCompressor& arch_comp, Writer& writer)
{
	if (arch_comp.GetUnitList().size() == 0) {
		return;
	}
	writer.WriteByte(kUnpackInfo);
	writer.WriteByte(kFolder);
	writer.WriteCompressedUint64(arch_comp.GetUnitList().size());
	writer.WriteByte(0);
	for (auto& it : arch_comp.GetUnitList()) {
		writer.WriteCompressedUint64(1 + it.used_bcj);
		WriteUnitInfo(it.coder_info, writer);
		if (it.used_bcj) {
			WriteUnitInfo(it.bcj_info, writer);
			// Bind pair for encoder to BCJ
			writer.WriteCompressedUint64(1);
			writer.WriteCompressedUint64(0);
		}
	}
	writer.WriteByte(kCodersUnpackSize);
	for (auto& it : arch_comp.GetUnitList()) {
		writer.WriteCompressedUint64(it.unpack_size);
		if (it.used_bcj) {
			// Size is unchanged after BCJ
			writer.WriteCompressedUint64(it.unpack_size);
		}
	}
	writer.WriteByte(kEnd);
}

void Container7z::WriteUnitInfo(const CoderInfo &coder_info, Writer& writer)
{
	CoderInfo::MethodId::IdString method_id;
	size_t id_size = coder_info.method_id.GetIdString(method_id);
	uint8_t header = static_cast<uint8_t>(id_size);
	header |= coder_info.GetHeaderFlags();
	writer.WriteByte(header);
	writer.WriteBytes(method_id.data(), id_size);
	if (coder_info.IsComplex()) {
		writer.WriteCompressedUint64(coder_info.num_in_streams);
		writer.WriteCompressedUint64(coder_info.num_out_streams);
	}
	if (coder_info.props.length() > 0) {
		writer.WriteCompressedUint64(coder_info.props.length());
		writer.WriteBytes(coder_info.props.c_str(), coder_info.props.length());
	}
}

void Container7z::WriteSubStreamsInfo(const ArchiveCompressor& arch_comp, Writer& writer)
{
	if (arch_comp.GetUnitList().size() == 0) {
		return;
	}
	writer.WriteByte(kSubStreamsInfo);
	for (auto& it : arch_comp.GetUnitList()) {
		if (it.file_count != 1) {
			// At least one unit has > 1 file so write counts for them all
			writer.WriteByte(kNumUnpackStream);
			for (auto& unit : arch_comp.GetUnitList()) {
				writer.WriteCompressedUint64(unit.file_count);
			}
			break;
		}
	}
	bool need_id = true;
	for (auto& unit : arch_comp.GetUnitList()) {
		if (unit.file_count > 1) {
			if (need_id) {
				writer.WriteByte(kSize);
				need_id = false;
			}
			// Skip the last file size because it must be equal to the remaining bytes in the unit.
			for (auto file_it = unit.in_file_first; file_it != unit.in_file_last; ++file_it) {
				if (!file_it->IsEmpty()) {
					writer.WriteCompressedUint64(file_it->size);
				}
			}
		}
	}
	for (auto& it : arch_comp.GetFileList()) {
		if (!it.IsEmpty()) {
			writer.WriteByte(kCRC);
			writer.WriteByte(1);
			for (auto& file : arch_comp.GetFileList()) {
				if (!file.IsEmpty()) {
					writer.WriteUint32(file.crc32);
				}
			}
			break;
		}
	}
	writer.WriteByte(kEnd);
}

uint_least64_t Container7z::WriteDatabase(const ArchiveCompressor& arch_comp,
	FastLzma2& unit_comp,
	OutputStream& out_stream)
{
	out_stream.exceptions(std::ios_base::failbit | std::ios_base::badbit);
	uint_least64_t packed_size = 0;
	try {
		uint_least64_t header_offset = out_stream.tellp();
		header_offset -= 32;
		WriteHeader(arch_comp, unit_comp, out_stream);
		uint_least64_t header_unpack_size = unit_comp.GetUnpackSize();
        CoderInfo coder_info = unit_comp.GetCoderInfo();
		uint_least64_t header_header_offset = out_stream.tellp();
		header_header_offset -= 32;
		uint_least64_t header_pack_size = header_header_offset - header_offset;
        uint_least32_t crc32;
        {
            Writer writer(unit_comp, out_stream, false);
            WriteHeaderHeader(writer,
                header_offset,
                header_pack_size,
                header_unpack_size,
                coder_info);
            crc32 = writer.GetCrc32();
        }
		WriteSignatureHeader(header_header_offset, unit_comp.GetUnpackSize(), crc32, out_stream);
		packed_size += unit_comp.GetPackSize() + kSignatureHeaderSize;
	}
	catch (std::ios_base::failure&) {
		if (!g_break) {
			throw IoException(Strings::kCannotWriteArchive, _T(""));
		}
	}
	return packed_size;
}

void Container7z::WriteHeader(const ArchiveCompressor& arch_comp,
	FastLzma2& unit_comp,
	OutputStream& out_stream)
{
	Writer writer(unit_comp, out_stream, true);
	writer.WriteByte(kHeader);
	// Archive properties
	if (arch_comp.GetUnitList().size() != 0) {
		writer.WriteByte(kMainStreamsInfo);
		WritePackInfo(arch_comp, 0, writer);
		WriteUnpackInfo(arch_comp, writer);
		WriteSubStreamsInfo(arch_comp, writer);
		writer.WriteByte(kEnd);
	}
	size_t file_count = arch_comp.GetFileList().size();
	if (file_count == 0) {
		writer.WriteByte(kEnd);
		return;
	}
	writer.WriteByte(kFilesInfo);
	writer.WriteCompressedUint64(file_count);
	// Empty streams
	size_t empty_file_count = arch_comp.GetEmptyFileCount();
	if (empty_file_count > 0) {
		writer.WriteByte(kEmptyStream);
		writer.WriteCompressedUint64(BoolWriter::GetByteCount(file_count));
		BoolWriter bool_writer(writer);
		for (auto& it : arch_comp.GetFileList()) {
			bool_writer.Write(it.IsEmpty());
		}
		bool_writer.Flush();
		writer.WriteByte(kEmptyFile);
		writer.WriteCompressedUint64(BoolWriter::GetByteCount(empty_file_count));
		for (size_t i = 0; i < empty_file_count; ++i) {
			bool_writer.Write(true);
		}
	}
	// Names
	size_t names_byte_count = arch_comp.GetNameLengthTotal() * 2;
	if (names_byte_count != 0) {
		writer.WriteByte(kName);
		writer.WriteCompressedUint64(names_byte_count + 1);
		writer.WriteByte(0);
		for (auto& it : arch_comp.GetFileList()) {
			writer.WriteName(it.dir, it.root);
			writer.WriteName(it.name, 0);
			writer.WriteByte(0);
			writer.WriteByte(0);
		}
	}
	// std::mem_fn is technically unnecessary but there's a bug in VS2013
	std::function<void(Writer&, uint_least64_t)> time_writer = std::mem_fn(&Writer::WriteUint64);
	WriteOptionalAttribute(arch_comp, file_count, kCTime, kFileTimeItemSize,
		[](std::list<ArchiveCompressor::FileInfo>::const_reference it) {
			return it.creat_time; },
		time_writer,
		writer);
	WriteOptionalAttribute(arch_comp, file_count, kMTime, kFileTimeItemSize,
		[](std::list<ArchiveCompressor::FileInfo>::const_reference it) {
			return it.mod_time; },
		time_writer,
		writer);
	// Windows attributes
	WriteOptionalAttribute(arch_comp, file_count, kWinAttributes, kAttributesItemSize,
		[](std::list<ArchiveCompressor::FileInfo>::const_reference it) {
			return it.attributes; },
		std::mem_fn(&Writer::WriteUint32),
		writer);
	writer.WriteByte(kEnd); // for files
	writer.WriteByte(kEnd); // for headers
}

// Write an uncompressed header for the compressed header
void Container7z::WriteHeaderHeader(Writer& writer,
    uint_least64_t header_offset,
	uint_least64_t header_pack_size,
	uint_least64_t header_unpack_size,
    CoderInfo& coder_info)
{
	writer.WriteCompressedUint64(kEncodedHeader);
	writer.WriteByte(kPackInfo);
	writer.WriteCompressedUint64(header_offset);
	// Always 1 unit
	writer.WriteCompressedUint64(1);
	writer.WriteByte(kSize);
	writer.WriteCompressedUint64(header_pack_size);
	writer.WriteByte(kEnd);
	writer.WriteByte(kUnpackInfo);
	writer.WriteByte(kFolder);
	writer.WriteCompressedUint64(1);
	writer.WriteByte(0);
	// Number of coders
	writer.WriteCompressedUint64(1);
	WriteUnitInfo(coder_info, writer);
	writer.WriteByte(kCodersUnpackSize);
	writer.WriteCompressedUint64(header_unpack_size);
	writer.WriteByte(kEnd);
	writer.WriteByte(kEnd);
}

template<typename AccessorFunc, typename WriterFunc>
void Container7z::WriteOptionalAttribute(const ArchiveCompressor& arch_comp,
	size_t file_count,
	uint8_t property_id,
	unsigned item_size,
	AccessorFunc accessor,
	WriterFunc write_func,
	Writer& writer)
{
	size_t num_defined = 0;
	for (auto& it : arch_comp.GetFileList()) {
		num_defined += accessor(it).IsSet();
	}
	if (num_defined > 0) {
		const size_t bool_bytes = (num_defined == file_count) ? 0 : BoolWriter::GetByteCount(file_count);
		const uint_least64_t data_size = static_cast<uint_least64_t>(num_defined) * item_size + bool_bytes + 2;
		writer.WriteByte(property_id);
		writer.WriteCompressedUint64(data_size);
		if (num_defined == file_count) {
			writer.WriteByte(1);
		}
		else {
			writer.WriteByte(0);
			BoolWriter bw(writer);
			for (auto& it : arch_comp.GetFileList()) {
				bw.Write(accessor(it).IsSet());
			}
		}
		writer.WriteByte(0);
		for (auto& it : arch_comp.GetFileList()) {
			if (accessor(it).IsSet()) {
				write_func(writer, accessor(it));
			}
		}
	}
}

void Container7z::WriteUint32(uint_fast32_t value, uint8_t* buffer)
{
	for (int i = 0; i < 4; ++i) {
		buffer[i] = static_cast<uint8_t>(value);
		value >>= 8;
	}
}

void Container7z::WriteUint64(uint_least64_t value, uint8_t* buffer)
{
	for (int i = 0; i < 8; ++i) {
		buffer[i] = static_cast<uint8_t>(value);
		value >>= 8;
	}
}

}