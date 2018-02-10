// RadyxRegister.cpp

#include "../../7-zip-zstd/CPP/7zip/Common/RegisterCodec.h"
#include "RadyxEncoder.h"
#include "../../7-zip-zstd/CPP/7zip/Compress/Lzma2Decoder.h"

namespace NCompress {

namespace NRadyx {

REGISTER_CODEC_E(RADYX,
	NCompress::NLzma2::CDecoder,
	CEncoder(),
	0x21,
	"RADYX")

}

}
