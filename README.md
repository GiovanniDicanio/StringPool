# StringPool
Custom C++ String Pool Allocator, including benchmark code to compare vs. STL strings.

by Giovanni Dicanio

Reusable code can be found in the [`StringPool.h` header](https://github.com/GiovanniDicanio/StringPool/blob/master/StringPool/StringPool/StringPool.h).

This repo constains a VS2015 solution; anyway, I wrote the C++ code to be _cross-platform_ (e.g. there are no dependencies from `<Windows.h>` or other Windows-specific headers).

The basic idea of this custom string pool allocator is to allocate big chunks of memory, and then serve single string allocations carving memory from inside those blocks, with a simple _fast_ pointer increase.

Moreover, the custom string class implemented to work with this allocator is very fast to sort as well, as it just contains a string pointer and a length data members, and its copy semantics is simple member-wise copy.
