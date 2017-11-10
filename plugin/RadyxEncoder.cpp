// RadyxEncoder.cpp

#include <cinttypes>

#include "RadyxEncoder.h"
#include "../Lzma2Compressor.h"
#include "../LzmaStates.h"
#include "../CoderInfo.h"

#include "../../7-zip-zstd/C/Lzma2Enc.h"
#include "../../7-zip-zstd/CPP/7zip/Common/StreamUtils.h"

namespace NCompress {

namespace NLzma2 {

HRESULT SetLzma2Prop(PROPID propID, const PROPVARIANT &prop, CLzma2EncProps &lzma2Props);

}

namespace NRadyx
{

CEncoder::CEncoder()
{
	Lzma2EncProps_Init(&lzma2Props);
}

CEncoder::~CEncoder()
{
}

STDMETHODIMP CEncoder::SetCoderProperties(const PROPID *propIDs,
	const PROPVARIANT *coderProps, UInt32 numProps)
{
	Lzma2EncProps_Init(&lzma2Props);

	for (UInt32 i = 0; i < numProps; i++)
	{
		RINOK(NLzma2::SetLzma2Prop(propIDs[i], coderProps[i], lzma2Props));
	}

	Radyx::Lzma2EncPropsNormalize(&lzma2Props);

	if (lzma2Props.lzmaProps.lc + lzma2Props.lzmaProps.lp > Radyx::LzmaStates::kLcLpMax)
		return SZ_ERROR_PARAM;

	return S_OK;
}

STDMETHODIMP CEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
	Radyx::CoderInfo ci = encoder.GetCoderInfo();
	return WriteStream(outStream, ci.props.c_str(), ci.props.length());
}

STDMETHODIMP CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
	const UInt64 * inSize, const UInt64 * outSize, ICompressProgressInfo *progress)
{
	if (!encoder) {
		RINOK(encoder.Create(lzma2Props));
	}

	return encoder.Code(inStream, outStream, inSize, outSize, progress);
}

} // NRadyx

} // NCompress
