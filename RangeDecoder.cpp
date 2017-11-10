#include "common.h"
#include "RangeDecoder.h"

namespace Radyx {

RangeDecoder::RangeDecoder() noexcept
	: range(0xFFFFFFFF), bound(0), code(0), data_src(nullptr), data_end(nullptr)
{
}

}