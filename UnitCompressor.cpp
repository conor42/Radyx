 ///////////////////////////////////////////////////////////////////////////////
//
// Class: UnitCompressor
//        Management of solid data units and data block overlap
//
// Copyright 1998-2000, 2015-present Conor McCarthy
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

#include <cstring>
#include "common.h"
#include "UnitCompressor.h"
#include "fast-lzma2/fl2_errors.h"

#ifdef UI_EXCEPTIONS
#include "IoException.h"
#include "Strings.h"
#endif

namespace Radyx {

static unsigned ValueToLog(size_t value, unsigned min_bits)
{
    unsigned bits = min_bits;
    while ((size_t(1) << bits) < value)
        ++bits;
    return bits;
}

#ifdef UI_EXCEPTIONS

inline void throw_write_exception(int os_error)
{
	throw IoException(Strings::kCannotWriteArchive, os_error, _T(""));
}

#else

inline void throw_write_exception(int /*os_error*/)
{
	throw std::ios_base::failure("");
}

#endif

UnitCompressor::UnitCompressor(RadyxOptions& options, size_t read_extra)
	: dictionary_size(0),
	unprocessed(0),
	buffer_index(0),
	async_read(options.async_read),
	working(false)
{
    cctx = FL2_createCCtxMt(options.thread_count);
    if (cctx == nullptr)
        throw std::bad_alloc();
	if (options.lzma2.encoder_mode == 3)
		FL2_CCtx_setParameter(cctx, FL2_p_highCompression, 1);
	FL2_CCtx_setParameter(cctx, FL2_p_compressionLevel, options.lzma2.compress_level);
    if (options.lzma2.dictionary_size.IsSet()) {
        dictionary_size = options.lzma2.dictionary_size;
        FL2_CCtx_setParameter(cctx, FL2_p_dictionaryLog, ValueToLog(dictionary_size, 12));
    }
    else {
        dictionary_size = size_t(1) << FL2_CCtx_setParameter(cctx, FL2_p_dictionaryLog, 0);
    }
    if (options.lzma2.match_buffer_log.IsSet())
        FL2_CCtx_setParameter(cctx, FL2_p_bufferLog, options.lzma2.match_buffer_log);
    if (options.lzma2.divide_and_conquer.IsSet())
        FL2_CCtx_setParameter(cctx, FL2_p_divideAndConquer, options.lzma2.divide_and_conquer);
    unsigned overlap_fraction = options.lzma2.block_overlap;
    if (!options.lzma2.block_overlap.IsSet())
        overlap_fraction = unsigned(FL2_CCtx_setParameter(cctx, FL2_p_overlapFraction, unsigned(-1)));
    overlap_fraction += !overlap_fraction; /* overlap_fraction > 0 so that overlap >= BcjTransform::kMaxUnprocessed */
    FL2_CCtx_setParameter(cctx, FL2_p_overlapFraction, overlap_fraction);
    if (options.lzma2.search_depth.IsSet())
        FL2_CCtx_setParameter(cctx, FL2_p_searchDepth, options.lzma2.search_depth);
    if (options.lzma2.second_dict_size.IsSet())
        FL2_CCtx_setParameter(cctx, FL2_p_chainLog, ValueToLog(options.lzma2.second_dict_size, FL2_CHAINLOG_MIN));
    if (options.lzma2.encoder_mode.IsSet())
        FL2_CCtx_setParameter(cctx, FL2_p_strategy, std::min(static_cast<unsigned>(options.lzma2.encoder_mode), 2U));
	if (options.lzma2.fast_length.IsSet())
        FL2_CCtx_setParameter(cctx, FL2_p_fastLength, options.lzma2.fast_length);
    if (options.lzma2.match_cycles.IsSet())
        FL2_CCtx_setParameter(cctx, FL2_p_searchLog, ValueToLog(options.lzma2.match_cycles, FL2_SEARCHLOG_MIN));
    FL2_CCtx_setParameter(cctx, FL2_p_literalCtxBits, options.lzma2.lc);
    FL2_CCtx_setParameter(cctx, FL2_p_literalPosBits, options.lzma2.lp);
    FL2_CCtx_setParameter(cctx, FL2_p_posBits, options.lzma2.pb);
    FL2_CCtx_setParameter(cctx, FL2_p_omitProperties, 1);
    FL2_CCtx_setParameter(cctx, FL2_p_doXXHash, 0);
    data_buffers[0].reset(dictionary_size, read_extra);
	if (async_read) {
		data_buffers[1].reset(dictionary_size, read_extra);
	}
    Begin(async_read);
}

UnitCompressor::~UnitCompressor()
{
	FL2_freeCCtx(cctx);
}

void UnitCompressor::Begin()
{
    Begin(async_read);
}

void UnitCompressor::Begin(bool async_read_)
{
	CheckError();
	WaitCompletion();
	unpack_size = 0;
	pack_size = 0;
	async_read = async_read_ && data_buffers[1];
	buffer_index = 0;
    FL2_beginFrame(cctx);
}

size_t UnitCompressor::GetAvailableSpace() const
{
	CheckError();
	return dictionary_size - block_end;
}

void UnitCompressor::CompressTranslateError()
{
	if (g_break)
		return;
	size_t csize = FL2_compressCCtxBlock_toFn(cctx,
        WriterFn,
        &args,
        &args.data_block,
        ProgressFn);
    if (g_break)
        return;
    if (FL2_isError(csize)) {
        size_t err = FL2_getErrorCode(csize);
        // The compressor shouldn't fail on anything except bad_alloc
        if (err == FL2_error_memory_allocation)
            error_code.type = ErrorCode::kMemory;
        else
            error_code.type = ErrorCode::kUnknown;
    }
    else {
        if (args.progress) {
            args.progress->Update(args.data_block.end - args.data_block.start - args.prev_done);
        }
        pack_size += csize;
    }
}

int FL2LIB_CALL UnitCompressor::WriterFn(const void* src, size_t srcSize, void* opaque)
{
    ThreadArgs* args = reinterpret_cast<ThreadArgs*>(opaque);
    try {
        args->out_stream->Write(src, srcSize);
    }
    catch (std::exception&) {
        return 1;
    }
    return 0;
}

int FL2LIB_CALL UnitCompressor::ProgressFn(size_t done, void* opaque)
{
    if (g_break)
        return 1;
    ThreadArgs* args = reinterpret_cast<ThreadArgs*>(opaque);
    if (args->progress) {
        args->progress->Update(done - args->prev_done);
    }
    args->prev_done = done;
    return 0;
}

void UnitCompressor::ThreadFn(void* pwork, int /*unused*/)
{
	UnitCompressor* unit_comp = reinterpret_cast<UnitCompressor*>(pwork);
    unit_comp->CompressTranslateError();
	unit_comp->working = false;
}

void UnitCompressor::CheckError() const
{
	if (g_break || error_code.type == ErrorCode::kGood) {
		return;
	}
	if (error_code.type == ErrorCode::kMemory) {
		throw std::bad_alloc();
	}
	else if (error_code.type == ErrorCode::kWrite) {
		throw_write_exception(error_code.os_code);
	}
	else {
		throw std::exception();
	}
}

void UnitCompressor::Compress(ArchiveStreamIn* in_stream,
	OutputStream& out_stream,
	FilterList* filters,
	std::list<CoderInfo>& coder_info,
	Progress* progress)
{
	pack_size = 0;
	while (!g_break)
	{
		size_t in_size = Read(in_stream);

		if (Unprocessed()) {
			Compress(filters, out_stream, progress);
			if (IsFull()) {
				Shift();
			}
			else {
				WaitCompletion();
				pack_size = GetPackSize() + Finalize(out_stream);
				break;
			}
		}
	}
	coder_info.clear();
	for (auto& f : *filters) {
		if (f->DidEncode()) {
			coder_info.push_front(f->GetCoderInfo());
		}
		f->Reset();
	}
	coder_info.push_front(GetCoderInfo());
}

void UnitCompressor::Write(const void* data,
	size_t count,
	OutputStream& out_stream)
{
	const char* src = reinterpret_cast<const char*>(data);
	while (count && !g_break) {
		size_t to_write = std::min(count, data_buffers[buffer_index].GetAvailableSpace());
		data_buffers[buffer_index].Write(src, to_write);
		if (to_write < count) {
			Compress(nullptr, out_stream, nullptr);
			Shift();
		}
		src += to_write;
		count -= to_write;
	}
}

void UnitCompressor::Compress(FilterList* filters,
	OutputStream& out_stream,
	Progress* progress)
{
	CheckError();
	if (data_buffers[buffer_index].Unprocessed())
	{
		DataBlock data_block = data_buffers[buffer_index].RunFilters(filters);
		WaitCompletion();
        FL2_blockBuffer block = { data_buffers[buffer_index].get(), data_block.start, data_block.end, dictionary_size };
        args = ThreadArgs(cctx,
            &out_stream,
            progress,
            block);
        if (async_read) {
            working = true;
            compress_thread.SetWork(ThreadFn, this, 0);
            buffer_index ^= 1;
        }
		else {
            ThreadFn(this, 0);
			CheckError();
		}
		unpack_size += data_block.Length();
	}
}

void UnitCompressor::Shift()
{
	CheckError();
	if (async_read) {
		FL2_blockBuffer block = { data_buffers[buffer_index ^ 1].get(), block_start, block_end, dictionary_size };
		FL2_shiftBlock_switch(cctx, &block, data_buffers[buffer_index].get());
		block_start = block.start;
	}
	else {
		FL2_blockBuffer block = { data_buffers[0].get(), block_start, block_end, dictionary_size };
		FL2_shiftBlock(cctx, &block);
		block_start = block.start;
	}
}

void UnitCompressor::Write(OutputStream& out_stream)
{
	CheckError();
	DataBlock data_block = data_buffers[buffer_index].GetAllData();
	size_t to_write = data_block.Length();
	out_stream.Write(data_block.data + data_block.start, to_write);
	if (!g_break && out_stream.Fail()) {
		error_code.LoadOsErrorCode();
		throw_write_exception(error_code.os_code);
	}
	unpack_size += to_write;
	pack_size += to_write;
	data_buffers[buffer_index].Reset();
}

CoderInfo UnitCompressor::GetCoderInfo()
{
    uint8_t dict_size_prop = cctx != nullptr ? FL2_dictSizeProp(cctx) : 0;
    return CoderInfo(&dict_size_prop, 1, 0x21, 1, 1);
}

size_t UnitCompressor::Finalize(OutputStream&)
{
    size_t csize = FL2_endFrame_toFn(cctx, WriterFn, &args);
    if (FL2_isError(csize))
        throw IoException(Strings::kCannotWriteArchive, _T(""));
    return csize;
}

}