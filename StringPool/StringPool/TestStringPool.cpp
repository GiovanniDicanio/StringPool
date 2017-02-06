//////////////////////////////////////////////////////////////////////////////////////////
//
// Benchmarking the String Pool Allocator.
// 
// Compares allocating string vectors and sorting them with 
// std::wstring vs. pool-allocated strings (StringPool).
// 
// by Giovanni Dicanio
// 
//////////////////////////////////////////////////////////////////////////////////////////

#include "StringPool.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;


//========================================================================================
//                          Benchmark Infrastructure Code
//========================================================================================

class Stopwatch
{
public:
    Stopwatch() = default;

    void Start()
    {
        m_start = std::chrono::high_resolution_clock::now();
    }

    void Stop()
    {
        TimePoint finish = std::chrono::high_resolution_clock::now();
        m_elapsed = finish - m_start;
    }

    void PrintTime(const char* s)
    {
        std::cout << s << ": " << (m_elapsed.count() * 1000.0) << " ms\n";
    }

private:
    typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

    TimePoint m_start{};
    std::chrono::duration<double> m_elapsed{};
};


inline void PrintTestConditions() 
{
    cout << "(";
#if defined(_M_X64)
    cout << "64-bit";
#elif defined(_M_IX86)
    cout << "32-bit";
#endif

#ifdef TEST_SSO
    cout << "; testing with small strings)";
#else
    cout << ")";
#endif

    cout << "\n\n";
}


//========================================================================================
//                              Main Benchmark Code
//========================================================================================

void Benchmark()
{
    //------------------------------------------------------------------------------------
    // Build the test strings
    //------------------------------------------------------------------------------------

    cout << "Building the string vectors for testing...\n\n";
    const auto shuffled = []() -> vector<wstring>
    {
        const wstring lorem[] = {
            L"Lorem ipsum dolor sit amet, consectetuer adipiscing elit.",
            L"Maecenas porttitor congue massa. Fusce posuere, magna sed",
            L"pulvinar ultricies, purus lectus malesuada libero,",
            L"sit amet commodo magna eros quis urna.",
            L"Nunc viverra imperdiet enim. Fusce est. Vivamus a tellus.",
            L"Pellentesque habitant morbi tristique senectus et netus et",
            L"malesuada fames ac turpis egestas. Proin pharetra nonummy pede.",
            L"Mauris et orci. [*** add more chars to prevent SSO ***]"
        };

        vector<wstring> v;

#ifdef _DEBUG
        const int kCount = 2;
#else
        const int kCount = 200 * 1000;
#endif
        for (int i = 0; i < kCount; ++i)
        {
            for (auto& s : lorem)
            {

// 
// Define TEST_SSO to test with the Small String Optimization
// 

//#define TEST_SSO
#ifdef TEST_SSO
                UNREFERENCED_PARAMETER(s);
                v.push_back(L"#" + to_wstring(i));
#else
                v.push_back(s + L" (#" + to_wstring(i) + L")");
#endif
            }
        }

        mt19937 prng(1729);

        shuffle(v.begin(), v.end(), prng);

        return v;
    }();

    const auto shuffled_ptrs = [&]() -> vector<const wchar_t *> {
        vector<const wchar_t *> v;

        for (auto& s : shuffled) {
            v.push_back(s.c_str());
        }

        return v;
    }();


    //------------------------------------------------------------------------------------
    // Benchmark Building the String Vectors 
    //------------------------------------------------------------------------------------

    Stopwatch sw;

    sw.Start();
    vector<wstring> stl1 = shuffled;
    sw.Stop();
    sw.PrintTime("Alloc STL1 ");

    sw.Start();
    StringPool::Allocator poolAlloc1;
    vector<StringPool::String> pool1;
    pool1.reserve(shuffled_ptrs.size());
    for (const auto& s : shuffled_ptrs)
    {
        pool1.push_back(poolAlloc1.AllocString(s));
    }
    sw.Stop();
    sw.PrintTime("Alloc Pool1");


#define SANITY_CHECK_ON_STRING_VECTOR_CONTENT
#ifdef SANITY_CHECK_ON_STRING_VECTOR_CONTENT
    if (pool1.size() != stl1.size())
    {
        throw runtime_error("String vectors have different sizes.");
    }

    const size_t stringCount = stl1.size();
    for (size_t i = 0; i < stringCount; i++)
    {
        if (wstring(pool1[i].Str(), pool1[i].Length()) != stl1[i])
        {
            throw runtime_error("Mismatch between STL string and pool-allocated string.");
        }
    }
#endif

    sw.Start();
    vector<wstring> stl2 = shuffled;
    sw.Stop();
    sw.PrintTime("Alloc STL2 ");

    sw.Start();
    StringPool::Allocator poolAlloc2;
    vector<StringPool::String> pool2;
    pool2.reserve(shuffled_ptrs.size());
    for (const auto& s : shuffled_ptrs)
    {
        pool2.push_back(poolAlloc2.AllocString(s));
    }
    sw.Stop();
    sw.PrintTime("Alloc Pool2");


    sw.Start();
    vector<wstring> stl3 = shuffled;
    sw.Stop();
    sw.PrintTime("Alloc STL3 ");

    sw.Start();
    StringPool::Allocator poolAlloc3;
    vector<StringPool::String> pool3;
    pool3.reserve(shuffled_ptrs.size());
    for (const auto& s : shuffled_ptrs)
    {
        pool3.push_back(poolAlloc3.AllocString(s));
    }
    sw.Stop();
    sw.PrintTime("Alloc Pool3");


    //------------------------------------------------------------------------------------
    // Benchmark String Vector Sorting
    //------------------------------------------------------------------------------------

    cout << "\nSorting...\n\n";

    sw.Start();
    sort(stl1.begin(), stl1.end());
    sw.Stop();
    sw.PrintTime("STL1 ");

    sw.Start();
    sort(pool1.begin(), pool1.end());
    sw.Stop();
    sw.PrintTime("Pool1");

    cout << "\n";

    sw.Start();
    sort(stl2.begin(), stl2.end());
    sw.Stop();
    sw.PrintTime("STL2 ");

    sw.Start();
    sort(pool2.begin(), pool2.end());
    sw.Stop();
    sw.PrintTime("Pool2");

    cout << "\n";

    sw.Start();
    sort(stl3.begin(), stl3.end());
    sw.Stop();
    sw.PrintTime("STL3 ");

    sw.Start();
    sort(pool3.begin(), pool3.end());
    sw.Stop();
    sw.PrintTime("Pool3");
}

int main() 
{
    cout << "*** Testing String Performance (STL vs. Pool) ***\n\n";
    cout << "by Giovanni Dicanio\n\n";

    PrintTestConditions();

    constexpr int kExitOk = 0;
    constexpr int kExitError = 1;

    try
    {
        Benchmark();
    }
    catch (const exception& e)
    {
        cout << "\n\n*** ERROR: " << e.what() << "\n\n";
        return kExitError;
    }

    return kExitOk;
}

