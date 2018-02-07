objects = ../fast-lzma2/error_private.o \
../fast-lzma2/fl2_common.o \
../fast-lzma2/fl2_compress.o \
../fast-lzma2/fl2_decompress.o \
../fast-lzma2/lzma2_dec.o \
../fast-lzma2/lzma2_enc.o \
../fast-lzma2/pool.o \
../fast-lzma2/radix_bitpack.o \
../fast-lzma2/radix_mf.o \
../fast-lzma2/radix_struct.o \
../fast-lzma2/range_enc.o \
../fast-lzma2/threading.o \
../fast-lzma2/xxhash.o \
Radyx.o \
../ArchiveCompressor.o \
../BcjX86.o \
../CoderInfo.o \
../CompressedUint64.o \
../Container7z.o \
../Crc32.o \
../DirScanner.o \
../IoException.o \
../OutputFile.o \
../Path.o \
../Progress.o \
../RadyxOptions.o \
../Strings.o \
../Thread.o \
../UnitCompressor.o \

CFLAGS := -Wall -O3
CXXFLAGS := -Wall -O3 -Wl,--subsystem,console -std=c++11
CC := gcc
CXX := g++

radyx : $(objects)
	$(CXX) -pthread -o radyx $(objects) -lm
