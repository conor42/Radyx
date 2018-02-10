// RadyxEncoder.h

#ifndef __RADYX_ENCODER_H
#define __RADYX_ENCODER_H


#include "../../7-zip-zstd/CPP/Common/MyCom.h"
#include "../../7-zip-zstd/CPP/7zip/ICoder.h"

#include "RadyxLzma2Enc.h"

namespace NCompress {
	namespace NRadyx {

		class CEncoder :
			public ICompressCoder,
			public ICompressSetCoderProperties,
			public ICompressWriteCoderProperties,
			public ICompressSetCoderPropertiesOpt,
			public CMyUnknownImp
		{
		private:
			Radyx::RadyxLzma2Enc encoder;
			CLzma2EncProps lzma2Props;

		public:
			MY_UNKNOWN_IMP4(
				ICompressCoder,
				ICompressSetCoderProperties,
				ICompressWriteCoderProperties,
				ICompressSetCoderPropertiesOpt)

			STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
					const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
			STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
			STDMETHOD(WriteCoderProperties)(ISequentialOutStream *outStream);
			STDMETHOD(SetCoderPropertiesOpt)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);

			CEncoder();
			virtual ~CEncoder();
		};

	}
}

#endif
