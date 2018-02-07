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

#include <algorithm>
#include <cstring>
#include "common.h"
#include "UnitCompressor.h"
#include "IoException.h"
#include "Strings.h"
#include "fast-lzma2/fl2_errors.h"

namespace Radyx {

static unsigned ValueToLog(size_t value, unsigned min_bits)
{
    unsigned bits = min_bits;
    while ((size_t(1) << bits) < value)
        ++bits;
    return bits;
}

UnitCompressor::UnitCompressor(RadyxOptions& options, bool do_bcj)
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
    data_buffer[0].reset(new uint8_t[dictionary_size]);
	if (async_read) {
		data_buffer[1].reset(new uint8_t[dictionary_size]);
	}
    Begin(do_bcj, async_read);
}

UnitCompressor::~UnitCompressor()
{
	FL2_freeCCtx(cctx);
}

void UnitCompressor::Begin(bool do_bcj)
{
    Begin(do_bcj, async_read);
}

void UnitCompressor::Begin(bool do_bcj, bool async_read_)
{
	CheckError();
	WaitCompletion();
	block_start = 0;
	block_end = 0;
	unpack_size = 0;
	pack_size = 0;
	unprocessed = 0;
	if (do_bcj) {
		if (bcj.get() == nullptr) {
			bcj.reset(new BcjX86);
		}
		else {
			bcj->Reset();
		}
	}
	else bcj.reset(nullptr);
	async_read = async_read_ && data_buffer[1].get() != nullptr;
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
        args->out_stream->write(reinterpret_cast<const char*>(src), srcSize);
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

void UnitCompressor::ThreadFn(void* pwork, int)
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
		throw IoException(Strings::kCannotWriteArchive, error_code.os_code, _T(""));
	}
	else {
		throw std::exception();
	}
}

void UnitCompressor::Compress(OutputStream& out_stream,
	Progress* progress)
{
	CheckError();
	if (block_end > block_start - unprocessed)
	{
		size_t processed_end = block_end;
		MutableDataBlock mut_block(data_buffer[buffer_index].get(), block_start - unprocessed, block_end);
		if (bcj.get() != nullptr) {
			processed_end = bcj->Transform(mut_block, true);
			// If the buffer is not full, there is no more data for this unit so
			// process it to the end.
			if (block_end < dictionary_size) {
				processed_end = block_end;
			}
			unprocessed = block_end - processed_end;
		}
        WaitCompletion();
        FL2_blockBuffer block = { data_buffer[buffer_index].get(), mut_block.start, processed_end, dictionary_size };
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
		unpack_size += block_end - block_start;
	}
}

void UnitCompressor::Shift()
{
	CheckError();
	if (async_read) {
		FL2_blockBuffer block = { data_buffer[buffer_index ^ 1].get(), block_start, block_end, dictionary_size };
		FL2_shiftBlock_switch(cctx, &block, data_buffer[buffer_index].get());
		block_start = block.start;
	}
	else {
		FL2_blockBuffer block = { data_buffer[0].get(), block_start, block_end, dictionary_size };
		FL2_shiftBlock(cctx, &block);
		block_start = block.start;
	}
	block_end = block_start;
}

void UnitCompressor::Write(OutputStream& out_stream)
{
	CheckError();
	size_t to_write = block_end - block_start;
	out_stream.write(reinterpret_cast<const char*>(data_buffer[buffer_index].get() + block_start), to_write);
	if (!g_break && out_stream.fail()) {
		throw IoException(Strings::kCannotWriteArchive, _T(""));
	}
	unpack_size += to_write;
	pack_size += to_write;
	block_start = 0;
	block_end = 0;
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