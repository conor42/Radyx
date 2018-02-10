#ifndef __RADYX_LZMA2ENC_H
#define __RADYX_LZMA2ENC_H

#include "../Lzma2Compressor.h"
#include "../../7-zip-zstd/CPP/7zip/ICoder.h"
#include "../../7-zip-zstd/C/Lzma2Enc.h"

namespace Radyx {

void Lzma2EncPropsNormalize(CLzma2EncProps *p);

class RadyxLzma2Enc
{
public:
	HRESULT Create(CLzma2EncProps& lzma2Props);
	HRESULT Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
		const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
	CoderInfo GetCoderInfo() const noexcept;
	operator bool() {
		return compressor.operator bool();
	}
private:
    std::unique_ptr<CompressorInterface> compressor;
    std::unique_ptr<UnitCompressor> unit_comp;
	std::unique_ptr<ThreadPool> threads;
	UInt64 in_processed;
	UInt64 out_processed;
};

}
#endif