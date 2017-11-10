#ifndef RADYX_RANGE_DECODER_H
#define RADYX_RANGE_DECODER_H

#include "RangeEncoder.h"

namespace Radyx {

class RangeDecoder
{
public:
	typedef RangeEncoder::Probability Probability;
	static const uint_fast32_t kTopValue = RangeEncoder::kTopValue;
	static const unsigned kNumBitModelTotalBits = RangeEncoder::kNumBitModelTotalBits;
	static const Probability kBitModelTotal = RangeEncoder::kBitModelTotal;
	static const unsigned kNumMoveBits = RangeEncoder::kNumMoveBits;

	RangeDecoder() noexcept;
	inline bool GetBit(Probability& rprob) noexcept;
	inline unsigned DecodeBitTree(Probability* prob_table, unsigned bit_count) noexcept;
	inline unsigned DecodeBitTreeReverse(Probability* prob_table, unsigned bit_count) noexcept;
	inline unsigned DecodeDirect(unsigned bit_count) noexcept;
	inline void SetData(uint8_t* data_src_, size_t len) noexcept;
	inline bool HaveData() const noexcept;

private:
	inline void Normalize() noexcept;
	inline void Update0(unsigned prob, Probability& rprob) noexcept;
	inline void Update1(unsigned prob, Probability& rprob) noexcept;

	uint_fast32_t range;
	uint_fast32_t bound;
	uint_fast32_t code;
	uint8_t* data_src;
	uint8_t* data_end;

	RangeDecoder(const RangeDecoder&) = delete;
	RangeDecoder& operator=(const RangeDecoder&) = delete;
	RangeDecoder(RangeDecoder&&) = delete;
	RangeDecoder& operator=(RangeDecoder&&) = delete;
};

inline bool RangeDecoder::GetBit(Probability& rprob) noexcept
{
	unsigned prob = rprob;
	Normalize();
	bound = (range >> kNumBitModelTotalBits) * prob;
	if (code < bound) {
		Update0(prob, rprob);
		return false;
	}
	else {
		Update1(prob, rprob);
		return true;
	}
}

inline unsigned RangeDecoder::DecodeBitTree(Probability* prob_table, unsigned bit_count) noexcept
{
	unsigned limit = 1 << bit_count;
	unsigned i = 1;
	do {
		i = (i << 1) + GetBit(prob_table[i]);
	} while (i < limit);
	return i - limit;
}

inline unsigned RangeDecoder::DecodeBitTreeReverse(Probability* prob_table, unsigned bit_count) noexcept
{
	assert(bit_count != 0);
	unsigned tree_index = 1;
	unsigned i = 0;
	do {
		unsigned bit = GetBit(prob_table[tree_index]);
		tree_index = (tree_index << 1) + bit;
		--bit_count;
		i |= bit << bit_count;
	} while (bit_count != 0);
	return i;
}

inline unsigned RangeDecoder::DecodeDirect(unsigned bit_count) noexcept
{
	unsigned result = 0;
	for (; bit_count != 0; --bit_count) {
		Normalize();
		range >>= 1;
		uint_fast32_t t = (code - range) >> 31;
		code -= range & (t - 1u);
		result = (result << 1) | unsigned(1u - t);
	}
	return result;
}

inline void RangeDecoder::SetData(uint8_t * data_src_, size_t len) noexcept
{
	data_src = data_src_;
	data_end = data_src_ + len;
	for (size_t i = 0; i < 5; ++i) {
		code = (code << 8) | *data_src;
		++data_src;
	}
}

inline bool RangeDecoder::HaveData() const noexcept
{
	return data_src <= data_end;
}

inline void RangeDecoder::Normalize() noexcept
{
	if (range < kTopValue) {
		range <<= 8;
		code = (code << 8) | (*data_src++);
	}
}

inline void RangeDecoder::Update0(unsigned prob, Probability& rprob) noexcept
{
	range = bound;
	rprob = static_cast<Probability>(prob + ((kBitModelTotal - prob) >> kNumMoveBits));
}

inline void RangeDecoder::Update1(unsigned prob, Probability& rprob) noexcept
{
	range -= bound;
	code -= bound;
	rprob = static_cast<Probability>(prob - (prob >> kNumMoveBits));
}

}

#endif // RADYX_RANGE_DECODER_H