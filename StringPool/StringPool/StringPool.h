#ifndef INCLUDE_GIOVANNI_DICANIO_STRINGPOOL_H
#define INCLUDE_GIOVANNI_DICANIO_STRINGPOOL_H

//////////////////////////////////////////////////////////////////////////////////////////
//
// Implements a string pool allocator (StringPool::Allocator) and a string class 
// (StringPool::String) representing strings allocated with it.
// 
// Copyright (C) by Giovanni Dicanio
// 
//////////////////////////////////////////////////////////////////////////////////////////


#include <cstdint>      // For portable int types like uint8_t
#include <new>          // For std::bad_alloc
#include <string>       // For std::wstring
#include <utility>      // For std::swap
#include <vector>       // For std::vector


namespace StringPool 
{

// Forward declarations
class String;
class Allocator;


//========================================================================================
//                              String Class
//========================================================================================

//----------------------------------------------------------------------------------------
// String returned by the allocator.
// This is just made by a pointer to a C-style NUL-terminated string, 
// and the length of that string.
//
// Copying this class just means member-wise copy, and it's a cheap operation.
//
// Do *not* delete instances of this string class: the memory of this class is managed
// by the StringPool::Allocator, which is responsible for deleting the allocated memory
// blocks.
//----------------------------------------------------------------------------------------
class String
{
public:

    // Creates an empty string.
    String() noexcept = default;

    // Default member-wise copy is fine.
    String(const String& other) noexcept = default;
    String& operator=(const String& other) noexcept = default;

    // Moves from the other string, leaving it empty.
    String(String&& other) noexcept
        : m_ptr{ other.m_ptr }
        , m_length{ other.m_length }
    {
        other.m_ptr = nullptr;
        other.m_length = 0;
    }

    // Moves from the other string, leaving it empty.
    String& operator=(String&& other) noexcept
    {
        if (&other != this)
        {
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;

            m_length = other.m_length;
            other.m_length = 0;
        }
        return *this;
    }

    // Returns C-style NUL-terminated string pointer.
    const wchar_t* Str() const noexcept
    {
        if (!IsEmpty())
        {
            return m_ptr;
        }
        else
        {
            return &m_nul;
        }
    }

    // Number of wchar_ts in the string (excluding the terminating NUL).
    // An empty string has length 0.
    size_t Length() const noexcept
    {
        return m_length;
    }

    bool IsEmpty() const noexcept
    {
        return m_length == 0;
    }

    // Convert to std::wstring.
    std::wstring ToStdString() const
    {
        if (!IsEmpty())
        {
            return std::wstring{ m_ptr, m_length };
        }
        else
        {
            return std::wstring{};
        }
    }
    
    // Compare this with other.
    // +1 : this > other
    // 0  : this == other
    // -1 : this < other
    int Compare(const String& other) const noexcept
    {
        const size_t minLength = m_length < other.m_length ?
            m_length : other.m_length;

        const int result = wmemcmp(m_ptr, other.m_ptr, minLength);

        if (result != 0)
            return result;

        if (m_length < other.m_length)
            return -1;

        if (m_length > other.m_length)
            return 1;

        return 0;
    }

    // StringPool::Allocator creates instances of this String class.
    friend class Allocator;

    // STL-style non-throwing swap
    friend void swap(String& a, String& b) noexcept
    {
        using std::swap;
        swap(a.m_ptr,    b.m_ptr);
        swap(a.m_length, b.m_length);

        // No need to swap m_nul, as it's always NUL for each string instance.
    }


private:
    const wchar_t* m_ptr{};     // C-style raw pointer to a NUL-terminated string
    size_t m_length{};          // Length, in wchar_ts, excluding the terminating NUL
    
    // The NUL character for empty strings
    const wchar_t m_nul{};

    // Constructor is private, as only the friend StringPool::Allocator class
    // can allocate instances of this String class.
    String(const wchar_t* ptr, size_t length) noexcept
        : m_ptr{ ptr }
        , m_length{ length }
    {}
};


//
// Convenient overloaded relational operators for string comparisons
// 

inline bool operator==(const String& a, const String& b) noexcept
{
    return a.Compare(b) == 0;
}

inline bool operator!=(const String& a, const String& b) noexcept
{
    return a.Compare(b) != 0;
}

inline bool operator<(const String& a, const String& b) noexcept
{
    return a.Compare(b) < 0;
}

inline bool operator>(const String& a, const String& b) noexcept
{
    return a.Compare(b) > 0;
}

inline bool operator<=(const String& a, const String& b) noexcept
{
    return a.Compare(b) <= 0;
}

inline bool operator>=(const String& a, const String& b) noexcept
{
    return a.Compare(b) >= 0;
}


//========================================================================================
//                              Allocator Class
//========================================================================================

//----------------------------------------------------------------------------------------
// String Pool Allocator
// 
// Preallocates chunks of memory, and serves memory just *increasing a pointer* 
// inside a chunk.
//----------------------------------------------------------------------------------------
class Allocator
{
public:

    // Initialize an empty allocator.
    // Call AllocString when you need a new string.
    Allocator() = default;

    // Release all the allocated chunks (if any).
    ~Allocator()
    {
        Clear();
    }

    // Ban copy
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    // Release every allocated chunk.
    void Clear()
    {
        for (auto& pChunk : m_chunks)
        {
            Free(pChunk);
            pChunk = nullptr;
        }
    
        m_chunks.clear();

        m_pNext = nullptr;
        m_pLimit = nullptr;
    }

    // Allocate a string using the pool allocator, deep-copying the string
    // from a C-style NUL-terminated string pointer.
    // Throws std::bad_alloc on allocation failure.
    String AllocString(const wchar_t* ptr)
    {
        return AllocString(ptr, ptr + wcslen(ptr));
    }

    // Allocate a string using the pool allocator, deep-copying the string
    // from a [start, finish) "string view".
    // As per the STL convention: start is included, finish is excluded.
    // Throws std::bad_alloc on allocation failure.
    String AllocString(const wchar_t* start, const wchar_t* finish)
    {
        const size_t length = finish - start;
        const size_t lengthWithNul = length + 1;
        wchar_t* ptr = AllocMemory(lengthWithNul);
        wmemcpy(ptr, start, length);
        ptr[length] = L'\0'; // terminating NUL

        return String{ ptr, length };
    }


private:

    // A memory chunk is made by this header followed by the allocated wchar_ts.
    struct ChunkHeader 
    {
        // Total chunk size, in bytes
        size_t SizeInBytes;

        // Followed by array of wchar_t characters 
        wchar_t Chars[1];
    };

    enum 
    {
        // An allocated chunk must be minimum this size, in bytes
        kMinChunkSizeInBytes = 600000,

        // Can't alloc strings larger than that (in wchar_ts)
        kMaxStringLength = 1024 * 1024
    };


    wchar_t*  m_pNext{};    // First available wchar_t slot in the current chunk
    wchar_t*  m_pLimit{};   // One past last available wchar_t slot in the current chunk

    // Keep a list of all allocated chunks.
    // NOTE: ChunkHeader pointers are *owning* raw pointers, 
    // that will be released by this class destructor.
    std::vector<ChunkHeader*> m_chunks{};


    //------------------------------------------------------------------------------------
    // Helper Methods
    //------------------------------------------------------------------------------------


    //
    // Helper functions to allocate and free memory.
    // Those could be factored out as a trait class, to experiment with pool allocators
    // using different alloc/free techniques/APIs.
    // 

    // Request a memory allocation.
    // Return nullptr on failure.
    static void* Allocate(size_t cbSize) noexcept
    {
        return malloc(cbSize);
    }

    // Free a previous allocation.
    static void Free(void* ptr) noexcept
    {
        return free(ptr);
    }

    // Helper function to allocate memory using the pool allocator.
    // 'length' is the number of wchar_ts requested.
    // 
    // First tries to carve memory from the current chunk.
    // If there's not enough space, allocates a new chunk.
    // Throws std::bad_alloc on allocation errors.
    wchar_t* AllocMemory(size_t length)
    {      
        // First let's try allocation in current chunk
        wchar_t* ptr = m_pNext;
        if (m_pNext + length <= m_pLimit)
        {
            // There's enough room in current chunk, so a simple pointer increase will do!
            m_pNext += length;
            return ptr;
        }

        // There's not enough room in current chunk. We need to allocate a new chunk.

        // Prevent request of too long strings
        if (length > kMaxStringLength)
        {
            throw std::bad_alloc();
        }

        // Allocate a new chunk, not smaller than minimum chunk size
        size_t chunkSizeInBytes = (length * sizeof(wchar_t)) + sizeof(ChunkHeader);
        if (chunkSizeInBytes < kMinChunkSizeInBytes)
        {
            chunkSizeInBytes = kMinChunkSizeInBytes;
        }

        uint8_t* pChunkStart = static_cast<uint8_t*>(Allocate(chunkSizeInBytes));
        if (pChunkStart == nullptr)
        {
            // Allocation failure: throw std::bad_alloc
            static std::bad_alloc outOfMemory;
            throw outOfMemory;
        }

        // Point one past the last available wchar_t in current chunk
        m_pLimit = reinterpret_cast<wchar_t*>(pChunkStart + chunkSizeInBytes);

        // Prepare the chunk header
        ChunkHeader* pNewChunk = reinterpret_cast<ChunkHeader*>(pChunkStart);
        pNewChunk->SizeInBytes = chunkSizeInBytes;
        
        // Set the pointer to point to the free bytes to serve the next allocation
        m_pNext = reinterpret_cast<wchar_t*>(pNewChunk + 1);

        // Keep track of the newly allocated chunk,
        // adding it to the chunk pointer vector
        m_chunks.push_back(pNewChunk);

        // Now that we have allocated a new chunk, 
        // we can retry the allocation with a simple pointer increase
        return AllocMemory(length);
    }
};


} // namespace StringPool


#endif // INCLUDE_GIOVANNI_DICANIO_STRINGPOOL_H
