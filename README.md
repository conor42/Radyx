### Radyx 0.9.2

Radyx is a file archiver which creates archives in the 7-zip format using only
the LZMA2 compression algorithm provided by the [Fast LZMA2 Library] v0.9.1.

[Fast LZMA2 Library]: https://github.com/conor42/fast-lzma2

The library uses a parallel buffered radix matchfinder to enable multithreaded
compression without splitting the input into large chunks or duplicating the
entire matchfinder/encoder combination for every 2 threads. The RMF also has
greater time and memory efficiency, using about 5.5x the dictionary size
instead of 11.5 x for BT4. This rises to 6.5x for dictionary sizes above 64Mb.
The library also uses a modified LZMA2 encoder employing speed tweaks from
Zstandard. The result is faster compression in less RAM, especially when using
4+ threads.

The RMF is a block algorithm, which reduces the effective dictionary size. This
results in slightly less compression than BT4 for a given dictionary size. To
compensate, you can raise the compression level (the most efficient way) or set
high-compression mode using -ma=3 on the command line.

The radix match finder only finds the longest match (up to the fast length
value) which slightly decreases compression performance when using optimized
encoding (level 4 and up). To compensate, Radyx has a hybrid mode which adds a
small sliding window dictionary and hash chain. This is enabled at level 8 and
above.

### Build

A VS2017 project is included. The code also builds with gcc v5.x or higher on
Ubuntu Linux using the makefile.

### Status

Both Radyx and the library have passed heavy testing. However this is a beta
release unsuitable for production environments. This version has no 7z
extraction command or any other commands apart from addition of files to a new
archive. 7-Zip is required for extraction.

Radyx has NO WARRANTY and is released under the GNU General Public License 3.0:
www.gnu.org/licenses/gpl.html
