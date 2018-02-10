#include "RadyxLzma2Enc.h"
#include "OutStream7z.h"

namespace Radyx {

bool g_break;

HRESULT RadyxLzma2Enc::Create(CLzma2EncProps& lzma2Props)
{
	g_break = false;
	in_processed = 0;
	out_processed = 0;
	try {
		threads.reset(new ThreadPool(lzma2Props.numTotalThreads - 1));

		Lzma2Options params;
		params.lc = lzma2Props.lzmaProps.lc;
		params.lp = lzma2Props.lzmaProps.lp;
		params.pb = lzma2Props.lzmaProps.pb;
		params.fast_length = lzma2Props.lzmaProps.fb;
		params.match_cycles = lzma2Props.lzmaProps.mc;
		params.encoder_mode = lzma2Props.lzmaProps.algo ? Lzma2Options::kBestMode : Lzma2Options::kFastMode;
		params.compress_level = lzma2Props.lzmaProps.level;
		params.dictionary_size = lzma2Props.lzmaProps.dictSize;
		
		if (params.dictionary_size > PackedMatchTable::kMaxDictionary
			|| (params.fast_length > PackedMatchTable::kMaxLength)) {
			compressor.reset(new Lzma2Compressor<StructuredMatchTable>(params));
		}
		else {
			compressor.reset(new Lzma2Compressor<PackedMatchTable>(params));
		}
		unit_comp.reset(new UnitCompressor(compressor->GetDictionarySize(),
			compressor->GetMaxBufferOverrun(),
			(params.dictionary_size * params.block_overlap) >> Lzma2Options::kOverlapShift,
			false,
			false));
	}
	catch (std::bad_alloc&) {
		return SZ_ERROR_MEM;
	}
	catch (std::exception&) {
		return S_FALSE;
	}

	return S_OK;
}

void RadyxEncPropsNormalize(CLzmaEncProps *p)
{
	int level = p->level;
	if (level < 0) level = 5;
	p->level = level;

	if (p->dictSize == 0) {
		p->dictSize = (level <= 5 ? (1 << (level * 2 + 15)) : (level <= 7 ? (1 << 26) : (1 << 27)));
	}
	else {
		if (p->dictSize < Lzma2Encoder::GetUserDictionarySizeMin()) {
			p->dictSize = (UInt32)Lzma2Encoder::GetUserDictionarySizeMin();
		}
		else if (p->dictSize > Lzma2Encoder::GetDictionarySizeMax()) {
			p->dictSize = (UInt32)Lzma2Encoder::GetDictionarySizeMax();
		}
	}
	if (p->dictSize > p->reduceSize)
	{
		unsigned i;
		UInt32 reduceSize = (UInt32)p->reduceSize;
		for (i = Lzma2Encoder::GetDictionaryBitsMin() - 1; i <= 30; i++)
		{
			if (reduceSize <= ((UInt32)2 << i)) { p->dictSize = ((UInt32)2 << i); break; }
			if (reduceSize <= ((UInt32)3 << i)) { p->dictSize = ((UInt32)3 << i); break; }
		}
	}

	if (p->lc < 0) p->lc = 3;
	if (p->lp < 0) p->lp = 0;
	if (p->pb < 0) p->pb = 2;

	if (p->algo < 0) p->algo = (level < 7 ? 0 : 1);
	if (p->fb < 0) p->fb = (level < 7 ? 32 : 64);
	if (p->mc == 0) p->mc = (16 + (p->fb >> 1)) >> (p->btMode ? 0 : 1);
}

void Lzma2EncPropsNormalize(CLzma2EncProps *p)
{
	UInt64 fileSize;

	if (p->numTotalThreads <= 0) {
		p->numTotalThreads = std::thread::hardware_concurrency();
	}

	fileSize = p->lzmaProps.reduceSize;

	if (p->blockSize != LZMA2_ENC_PROPS__BLOCK_SIZE__SOLID
		&& p->blockSize != LZMA2_ENC_PROPS__BLOCK_SIZE__AUTO
		&& (p->blockSize < fileSize || fileSize == (UInt64)(Int64)-1))
		p->lzmaProps.reduceSize = p->blockSize;

	RadyxEncPropsNormalize(&p->lzmaProps);

	p->lzmaProps.reduceSize = fileSize;

	if (p->blockSize == LZMA2_ENC_PROPS__BLOCK_SIZE__AUTO)
	{
		// blockSize is not an issue for radyx multithreading
		p->blockSize = LZMA2_ENC_PROPS__BLOCK_SIZE__SOLID;
	}
}

STDMETHODIMP RadyxLzma2Enc::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
	const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
	HRESULT err = S_OK;
	try {
		OutStream7z out_stream(outStream);
		do
		{
			if (unit_comp->GetAvailableSpace() == 0) {
				// Unit compressor is full so compress
				unit_comp->Compress(*compressor, *threads, out_stream, nullptr);
				unit_comp->Shift();
				in_processed += unit_comp->GetUnpackSize();
				out_processed += unit_comp->GetPackSize();

				if (progress)
				{
					err = progress->SetRatioInfo(&in_processed, &out_processed);
					if (err != S_OK)
						break;
				}
			}
			UInt32 inSize;
			do {
				inSize = 0;
				err = inStream->Read((void*)unit_comp->GetAvailableBuffer(), (UInt32)unit_comp->GetAvailableSpace(), &inSize);
				unit_comp->AddByteCount(inSize);
			} while (err == S_OK && inSize != 0 && unit_comp->GetAvailableSpace() != 0);
		} while (err == S_OK && unit_comp->Unprocessed());
	}
	catch (std::bad_alloc&) {
		err = SZ_ERROR_MEM;
	}
	catch (std::ios_base::failure&) {
		err = SZ_ERROR_WRITE;
	}
	catch (std::exception) {
		err = S_FALSE;
	}
	g_break = err != S_OK;
	unit_comp->Reset(false, false);
	return err;
}

CoderInfo RadyxLzma2Enc::GetCoderInfo() const noexcept
{
	if (compressor) {
		return compressor->GetCoderInfo();
	}
	return CoderInfo();
}

}