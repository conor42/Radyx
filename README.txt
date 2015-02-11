===============================================================================
    Radyx 0.9 README
===============================================================================

Radyx is a file archiver which creates archives in the 7-zip format. It uses
only the LZMA2 compression algorithm. The major difference between Radyx and
7-zip is in the match finder algorithm. Radyx uses the binning step of a radix
sort to find string matches. This algorithm is "embarrassingly parallel"
meaning that very little communication between threads is required. As a
result, only one match finder is required for any number of CPU cores, whereas
7-zip must use a separate match finder for every 2 threads in normal mode and
each thread in fast mode. The memory requirement for Radyx increases relatively
little as extra threads are added. Compression speed depends on the type of
data. It generally seems to be up to 15% faster than 7-zip, but on some unusual
data types (eg. some DNA sequence files) it gains a 2X to 3X speed increase.

Because radix sorting works on blocks of data, Radyx cannot use a sliding
window for the LZMA2 dictionary. Instead, some overlap is used between
consecutive blocks. This requires double the dictionary size to get similar
compression performace to 7-zip, but the memory requirement is only 5-7 times
the dictionary size so this is not a problem. 

The radix match finder only finds the longest match (up to the fast length
value) which slightly decreases compression performance when using optimized
encoding (level 5 and up). To compensate, Radyx has a hybrid mode which adds a
small sliding window dictionary and hash chain. This is enabled at level 7 and
above.

Radyx has NO WARRANTY and is released under the GNU General Public License 3.0:
www.gnu.org/licenses/gpl.html
