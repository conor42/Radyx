///////////////////////////////////////////////////////////////////////////////
//
// Class: FastLzma2
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
#include "FastLzma2.h"
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

FastLzma2::FastLzma2(RadyxOptions& options)
{
    fcs = FL2_createCStreamMt(options.thread_count, options.async_read);
    if (fcs == nullptr)
        throw std::bad_alloc();
	if (options.lzma2.encoder_mode == 3)
		FL2_CStream_setParameter(fcs, FL2_p_highCompression, 1);
	FL2_CStream_setParameter(fcs, FL2_p_compressionLevel, options.lzma2.compress_level);
    if (options.lzma2.dictionary_size.IsSet())
        FL2_CStream_setParameter(fcs, FL2_p_dictionarySize, options.lzma2.dictionary_size);
    if (options.lzma2.match_buffer_log.IsSet())
        FL2_CStream_setParameter(fcs, FL2_p_bufferLog, options.lzma2.match_buffer_log);
    if (options.lzma2.divide_and_conquer.IsSet())
        FL2_CStream_setParameter(fcs, FL2_p_divideAndConquer, options.lzma2.divide_and_conquer);
    unsigned overlap_fraction = options.lzma2.block_overlap;
    if (!options.lzma2.block_overlap.IsSet())
        overlap_fraction = unsigned(FL2_CCtx_getParameter(fcs, FL2_p_overlapFraction));
    FL2_CStream_setParameter(fcs, FL2_p_overlapFraction, overlap_fraction);
    if (options.lzma2.search_depth.IsSet())
        FL2_CStream_setParameter(fcs, FL2_p_searchDepth, options.lzma2.search_depth);
    if (options.lzma2.second_dict_size.IsSet())
        FL2_CStream_setParameter(fcs, FL2_p_chainLog, ValueToLog(options.lzma2.second_dict_size, FL2_CHAINLOG_MIN));
    if (options.lzma2.encoder_mode.IsSet())
        FL2_CStream_setParameter(fcs, FL2_p_strategy, std::min(static_cast<unsigned>(options.lzma2.encoder_mode), 2U));
	if (options.lzma2.fast_length.IsSet())
        FL2_CStream_setParameter(fcs, FL2_p_fastLength, options.lzma2.fast_length);
    if (options.lzma2.match_cycles.IsSet())
        FL2_CStream_setParameter(fcs, FL2_p_hybridCycles, options.lzma2.match_cycles);
    FL2_CStream_setParameter(fcs, FL2_p_literalCtxBits, options.lzma2.lc);
    FL2_CStream_setParameter(fcs, FL2_p_literalPosBits, options.lzma2.lp);
    FL2_CStream_setParameter(fcs, FL2_p_posBits, options.lzma2.pb);
    FL2_CStream_setParameter(fcs, FL2_p_omitProperties, 1);
    FL2_CStream_setParameter(fcs, FL2_p_doXXHash, 0);
}

FastLzma2::~FastLzma2()
{
	FL2_freeCCtx(fcs);
}

void FastLzma2::SetTimeout(unsigned ms)
{
    FL2_setCStreamTimeout(fcs, ms);
}

void FastLzma2::Begin(bool do_bcj)
{
	unpack_size = 0;
	pack_size = 0;
	if (do_bcj) {
		if (bcj.get() == nullptr) {
			bcj.reset(new BcjX86);
		}
		else {
			bcj->Reset();
		}
	}
	else bcj.reset(nullptr);
    CheckError(FL2_initCStream(fcs, 0));
    CheckError(FL2_getDictionaryBuffer(fcs, &dict));
    dict_pos = 0;
    bcj_trim = 0;
}

uint8_t* FastLzma2::GetAvailableBuffer(unsigned long& size)
{
    size = static_cast<unsigned long>(dict.size - dict_pos);
    return reinterpret_cast<uint8_t*>(dict.dst) + dict_pos;
}

size_t FastLzma2::WaitAndReport(size_t res, Progress* progress)
{
    while (FL2_isTimedOut(res)) {
        if (g_break)
            return 0;
        if(progress)
            progress->Update(FL2_getCStreamProgress(fcs, nullptr));
        res = FL2_waitStream(fcs);
    }
    CheckError(res);
    return res;
}

void FastLzma2::AddByteCount(size_t count, OutputStream & out_stream, Progress* progress)
{
    dict_pos += count;
    if (dict_pos == dict.size) {
        if (bcj) {
            uint8_t* dst = reinterpret_cast<uint8_t*>(dict.dst);
            bcj_trim = bcj->Transform(dst, dict_pos, true);
            if (bcj_trim != 0) {
                dict_pos -= bcj_trim;
                memcpy(bcj_cache, dst + dict_pos, bcj_trim);
            }
        }
        unpack_size += dict_pos;
        size_t res = FL2_updateDictionary(fcs, dict_pos);
        res = WaitAndReport(res, progress);
        if (res != 0)
            WriteBuffers(out_stream);
        if (bcj_trim != 0) {
            WriteBuffers(out_stream);
            res = FL2_flushStream(fcs, nullptr);
            res = WaitAndReport(res, progress);
            if (res != 0)
                WriteBuffers(out_stream);
        }
        do {
            res = FL2_getDictionaryBuffer(fcs, &dict);
        } while (FL2_isTimedOut(res));
        CheckError(res);
        dict_pos = 0;
        if (bcj_trim != 0) {
            dict_pos = bcj_trim;
            memcpy(dict.dst, bcj_cache, bcj_trim);
            bcj_trim = 0;
        }
        if (progress)
            progress->Update(FL2_getCStreamProgress(fcs, nullptr));
    }
}

void FastLzma2::CheckError(size_t res)
{
    if (FL2_isError(res)) {
        size_t err = FL2_getErrorCode(res);
        // shouldn't fail on anything except bad_alloc
        if (err == FL2_error_memory_allocation)
            throw std::bad_alloc();
        else
            throw std::exception();
    }
}

void FastLzma2::WriteBuffers(OutputStream& out_stream)
{
    size_t csize;
    for (;;) {
        FL2_cBuffer cbuf;
        // Waits if compression in progress
        csize = FL2_getNextCStreamBuffer(fcs, &cbuf);
        CheckError(csize);
        if (csize == 0)
            break;
        out_stream.write(reinterpret_cast<const char*>(cbuf.src), cbuf.size);
        if (out_stream.fail())
            throw IoException(Strings::kCannotWriteArchive, _T(""));
        pack_size += csize;
    }
}

CoderInfo FastLzma2::GetCoderInfo()
{
    uint8_t dict_size_prop = fcs != nullptr ? FL2_getCCtxDictProp(fcs) : 0;
    return CoderInfo(&dict_size_prop, 1, 0x21, 1, 1);
}

uint_least64_t FastLzma2::Finalize(OutputStream& out_stream, Progress* progress)
{
    if (dict_pos) {
        unpack_size += dict_pos;
        if (bcj)
            bcj->Transform(reinterpret_cast<uint8_t*>(dict.dst), dict_pos, true);
        WaitAndReport(FL2_updateDictionary(fcs, dict_pos), progress);
    }

    size_t res = FL2_endStream(fcs, nullptr);
    res = WaitAndReport(res, progress);
    while (res) {
        WriteBuffers(out_stream);
        res = FL2_endStream(fcs, nullptr);
        res = WaitAndReport(res, progress);
    }
    return pack_size;
}

void FastLzma2::IncBufferCount(OutputStream & out_stream)
{
    ++dict_pos;
    if (dict_pos == dict.size)
        Write(out_stream);
}

void FastLzma2::Write(OutputStream & out_stream)
{
    pack_size += dict_pos;
    unpack_size = pack_size;
    out_stream.write(reinterpret_cast<const char*>(dict.dst), dict_pos);
    if (out_stream.fail())
        throw IoException(Strings::kCannotWriteArchive, _T(""));
    dict_pos = 0;
}

}