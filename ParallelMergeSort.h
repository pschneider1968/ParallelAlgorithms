// TODO: Place all of these algorithms in a parallel_algorithms namespace
// TODO: Provide the same interface to serial and parallel algorithms as standard C++ does, using the first argument as the execution policy
// TODO: Improve parallel in-place merge sort by using the same method as the not-in-place merge sort does, where it looks at how many processors are
//       available and adjusts the parallel threshold accordingly, as the current parallel threshold is set way too small.
// TODO: Use Selection Sort instead of Insertion Sort for faster bottom of the recursion tree.
// TODO: Consider bringing in and thoroughly testing (correctness and performance) https://keithschwarz.com/interesting/code/?dir=inplace-merge (Linear in-place merge implementation)
// TODO: Implement memswap() with the same interface as memcpy() https://stackoverflow.com/questions/109249/why-isnt-there-a-standard-memswap-function

// Parallel Merge Sort implementations

#ifndef _ParallelMergeSort_h
#define _ParallelMergeSort_h

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <thread>
#include <execution>
#include <ppl.h>
#else
#include <iostream>
#include <algorithm>
#include <chrono>
#include <random>
#include <ratio>
#include <vector>
#include <thread>
#include <execution>
#endif

#include "InsertionSort.h"
#include "BinarySearch.h"
#include "ParallelMerge.h"
#include "RadixSortLSD.h"
#include "RadixSortMSD.h"
#include "RadixSortLsdParallel.h"
#include "RadixSortMsdParallel.h"

// TODO: This extern should not be needed and root-cause needs to be found
extern void RadixSortLSDPowerOf2Radix_unsigned_TwoPhase(unsigned long* a, unsigned long* b, size_t a_size);
extern void RadixSortLSDPowerOf2Radix_unsigned_TwoPhase_DeRandomize(unsigned long* a, unsigned long* b, size_t a_size);

namespace ParallelAlgorithms
{
    // The simplest version of parallel merge sort that reverses direction of source and destination arrays on each level of recursion
    // to eliminate the use of an additional array.  The top-level of recursion starts in the source to destination direction, which is
    // what's needed and reverses direction at each level of recursion, handling the leaf nodes by using a copy when the direction is opposite.
    // Assumes l <= r on entrance, which is simple to check if really needed.
    // Think of srcDst as specifying the direction at this recursion level, and as recursion goes down what is passed in srcDst is control of
    // direction for that next level of recursion.
    // Will this work if the top-level srcToDst is set to false to begin with - i.e. we want the result to end up in the source buffer and use
    // the destination buffer as an auxilary buffer/storage.  It would be really cool if the algorithm just worked this way, and had these
    // two modes of usage.  I predict that it will just work that way, and then I may need to define two entrance point functions that make these
    // two behaviors more obvious and explicit and not even have srcToDst argument.
    // Indexes l and r must be int's to provide the ability to specify zero elements with l = 0 and r = -1.  Otherwise, specifying zero would be a little strange
    // and you'd have to do it as l = 1 and r = 0. !!! This may be the reason that STL does *src_start and *src_end, and then the wrapper function may not be needed!!!

    // Listing 1
    template< class _Type >
    inline void parallel_merge_sort_simplest_r(_Type* src, size_t l, size_t r, _Type* dst, bool srcToDst = true)	// srcToDst specifies direction for this level of recursion
    {
        if (r == l) {    // termination/base case of sorting a single element
            if (srcToDst)  dst[l] = src[l];    // copy the single element from src to dst
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_merge_sort_simplest_r(src, l,     m, dst, !srcToDst); },		// reverse direction of srcToDst for the next level of recursion
            [&] { parallel_merge_sort_simplest_r(src, m + 1, r, dst, !srcToDst); }		// reverse direction of srcToDst for the next level of recursion
        );
        if (srcToDst)   merge_parallel_L5(src, l, m, m + 1, r, dst, l);
        else	        merge_parallel_L5(dst, l, m, m + 1, r, src, l);
    }

    template< class _Type >
    inline void parallel_merge_sort_simplest(_Type* src, int l, int r, _Type* dst, bool srcToDst = true)	// srcToDst specifies direction for this level of recursion
    {
        if (r < l) return;
        parallel_merge_sort_simplest_r(src, l, r, dst, srcToDst);
    }

    // Listing 2
    template< class _Type >
    inline void parallel_merge_sort(_Type* src, int l, int r, _Type* dst)
    {
        parallel_merge_sort_hybrid(src, l, r, dst, true);  // srcToDst = true
    }

    template< class _Type >
    inline void parallel_merge_sort_pseudo_inplace(_Type* srcDst, int l, int r, _Type* aux)
    {
        parallel_merge_sort_hybrid(srcDst, l, r, aux, false);  // srcToDst = false
    }

    // Listing 3
    template< class _Type >
    inline void parallel_merge_sort_hybrid_rh(_Type* src, size_t l, size_t r, _Type* dst, bool srcToDst = true)
    {
        if (r < l)  return;
        if (r == l) {    // termination/base case of sorting a single element
            if (srcToDst)  dst[l] = src[l];    // copy the single element from src to dst
            return;
        }
        if ((r - l) <= 48) {
            insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);        // in both cases sort the src
            //stable_sort( src + l, src + r + 1 );  // STL stable_sort can be used instead, but is slightly slower than Insertion Sort
            if (srcToDst) for (size_t i = l; i <= r; i++)    dst[i] = src[i];    // copy from src to dst, when the result needs to be in dst
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_merge_sort_hybrid_rh(src, l,     m, dst, !srcToDst); },    // reverse direction of srcToDst for the next level of recursion
            [&] { parallel_merge_sort_hybrid_rh(src, m + 1, r, dst, !srcToDst); }     // reverse direction of srcToDst for the next level of recursion
        );
        if (srcToDst) merge_parallel_L5(src, l, m, m + 1, r, dst, l);
        else          merge_parallel_L5(dst, l, m, m + 1, r, src, l);
    }

    // Listing 4
    template< class _Type >
    inline void parallel_merge_sort_hybrid_rh_1(_Type* src, size_t l, size_t r, _Type* dst, bool srcToDst = true)
    {
        if (r < l)  return;
        if (r == l) {    // termination/base case of sorting a single element
            if (srcToDst)  dst[l] = src[l];    // copy the single element from src to dst
            return;
        }
        if ((r - l) <= 48 && !srcToDst) {     // 32 or 64 or larger seem to perform well
            insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);    // want to do dstToSrc, can just do it in-place, just sort the src, no need to copy
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_merge_sort_hybrid_rh_1(src, l,     m, dst, !srcToDst); },      // reverse direction of srcToDst for the next level of recursion
            [&] { parallel_merge_sort_hybrid_rh_1(src, m + 1, r, dst, !srcToDst); }       // reverse direction of srcToDst for the next level of recursion
        );
        if (srcToDst) merge_parallel_L5(src, l, m, m + 1, r, dst, l);
        else          merge_parallel_L5(dst, l, m, m + 1, r, src, l);
    }

    template< class _Type >
    inline void parallel_merge_sort_hybrid_rh_2(_Type* src, size_t l, size_t r, _Type* dst, bool stable = true, bool srcToDst = true, size_t parallelThreshold = 32 * 1024)
    {
        if (r < l)  return;
        if (r == l) {   // termination/base case of sorting a single element
            if (srcToDst)  dst[l] = src[l];    // copy the single element from src to dst
            return;
        }
        if ((r - l) <= parallelThreshold && !srcToDst) {
            if (!stable)
                std::sort(src + l, src + r + 1);
                //std::sort(std::execution::par_unseq, src + l, src + r + 1);
            else
                std::stable_sort( src + l, src + r + 1 );
            //if (srcToDst)
            //    for (int i = l; i <= r; i++)    dst[i] = src[i];
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_merge_sort_hybrid_rh_2(src, l,     m, dst, stable, !srcToDst); },      // reverse direction of srcToDst for the next level of recursion
            [&] { parallel_merge_sort_hybrid_rh_2(src, m + 1, r, dst, stable, !srcToDst); }       // reverse direction of srcToDst for the next level of recursion
        );
        if (srcToDst) merge_parallel_L5(src, l, m, m + 1, r, dst, l);
        else          merge_parallel_L5(dst, l, m, m + 1, r, src, l);
    }

    template< class _Type >
    inline void parallel_merge_merge_sort_hybrid_inner(_Type* src, size_t l, size_t r, _Type* dst, bool srcToDst = true, size_t parallelThreshold = 32 * 1024)
    {
        if (r < l)  return;
        if (r == l) {   // termination/base case of sorting a single element
            if (srcToDst)  dst[l] = src[l];    // copy the single element from src to dst
            return;
        }
        if ((r - l) <= parallelThreshold) {
            merge_sort_hybrid(src, l, r, dst, srcToDst);
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_merge_merge_sort_hybrid_inner(src, l,     m, dst, !srcToDst); },      // reverse direction of srcToDst for the next level of recursion
            [&] { parallel_merge_merge_sort_hybrid_inner(src, m + 1, r, dst, !srcToDst); }       // reverse direction of srcToDst for the next level of recursion
        );
        if (srcToDst) merge_parallel_L5(src, l, m, m + 1, r, dst, l);
        else          merge_parallel_L5(dst, l, m, m + 1, r, src, l);
    }

    template< class _Type >
    inline void parallel_merge_merge_sort_hybrid(_Type* src, size_t l, size_t r, _Type* dst, bool srcToDst = true, size_t parallelThreshold = 32 * 1024)
    {
        // may return 0 when not able to detect
        const auto processor_count = std::thread::hardware_concurrency();
        //printf("Number of cores = %u \n", processor_count);

        if ((int)(parallelThreshold * processor_count) < (r - l + 1))
            parallelThreshold = (r - l + 1) / processor_count;

        parallel_merge_merge_sort_hybrid_inner(src, l, r, dst, srcToDst, parallelThreshold);
    }

    template< class _Type >
    inline void parallel_merge_sort_hybrid(_Type* src, size_t l, size_t r, _Type* dst, bool srcToDst = true, size_t parallelThreshold = 16 * 1024)
    {
        // may return 0 when not able to detect
        //const auto processor_count = std::thread::hardware_concurrency();
        //printf("Number of cores = %u \n", processor_count);

        //if ((int)(parallelThreshold * processor_count) < (r - l + 1))
        //    parallelThreshold = (r - l + 1) / processor_count;

        parallel_merge_sort_hybrid_rh_2(src, l, r, dst, false, srcToDst, parallelThreshold);
        //parallel_merge_sort_hybrid_rh_1(src, l, r, dst, srcToDst);
    }

    inline void parallel_merge_sort_hybrid_radix_inner(unsigned long* src, size_t l, size_t r, unsigned long* dst, bool srcToDst = true, size_t parallelThreshold = 32 * 1024)
    {
        //printf("l = %zd   r = %zd   parallelThreshold = %zd\n", l, r, parallelThreshold);
        if (r < l)  return;
        if (r == l) {   // termination/base case of sorting a single element
            if (srcToDst)  dst[l] = src[l];    // copy the single element from src to dst
            return;
        }
        if ((r - l) <= parallelThreshold && !srcToDst) {
            //RadixSortLSDPowerOf2Radix_unsigned_TwoPhase(src + l, dst + l, r - l + 1);
            RadixSortLSDPowerOf2Radix_unsigned_TwoPhase_DeRandomize(src + l, dst + l, r - l + 1);             // fastest with 8-cores on 48-core CPU
            //RadixSortLSDPowerOf2RadixParallel_unsigned_TwoPhase(src + l, dst + l, (unsigned long)(r - l + 1));  // fastest with 4-cores on  6-core CPU
            //RadixSortLSDPowerOf2RadixParallel_unsigned_TwoPhase_DeRandomize(src + l, dst + l, r - l + 1);
            //if (srcToDst)
            //    for (int i = l; i <= r; i++)    dst[i] = src[i];
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_merge_sort_hybrid_radix_inner(src, l,     m, dst, !srcToDst, parallelThreshold); },      // reverse direction of srcToDst for the next level of recursion
            [&] { parallel_merge_sort_hybrid_radix_inner(src, m + 1, r, dst, !srcToDst, parallelThreshold); }       // reverse direction of srcToDst for the next level of recursion
        );
        if (srcToDst) merge_parallel_L5(src, l, m, m + 1, r, dst, l);
        else          merge_parallel_L5(dst, l, m, m + 1, r, src, l);
    }

    inline void parallel_merge_sort_hybrid_radix(unsigned long* src, size_t l, size_t r, unsigned long* dst, bool srcToDst = true, size_t parallelThreshold = 24 * 1024)
    {
        // may return 0 when not able to detect
        //const auto processor_count = std::thread::hardware_concurrency();
        //printf("Number of cores = %u   parallelThreshold = %d\n", processor_count, parallelThreshold);

        //if ((parallelThreshold * processor_count) < (r - l + 1))
        //    parallelThreshold = (r - l + 1) / processor_count;

        parallel_merge_sort_hybrid_radix_inner(src, l, r, dst, srcToDst, parallelThreshold);
    }

    inline void parallel_merge_sort_hybrid_radix_single_buffer(unsigned long* src, size_t l, size_t r, unsigned long* dst, bool srcToDst = true, size_t parallelThreshold = 24 * 1024)
    {
        // may return 0 when not able to detect
        const auto processor_count = std::thread::hardware_concurrency();
        //printf("Number of cores = %u   parallelThreshold = %d\n", processor_count, parallelThreshold);

        if ((parallelThreshold * processor_count) < (r - l + 1))
            parallelThreshold = (r - l + 1) / processor_count;

        parallel_merge_sort_hybrid_radix_inner(src, l, r, dst, srcToDst, parallelThreshold);
    }

    // Pure Serial Merge Sort, using divide-and-conquer algorthm
    template< class _Type >
    inline void merge_sort(_Type* src, size_t l, size_t r, _Type* dst, bool srcToDst = true)
    {
        if (r < l)  return;
        if (r == l) {    // termination/base case of sorting a single element
            if (srcToDst)  dst[l] = src[l];    // copy the single element from src to dst
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow

        merge_sort(src, l,     m, dst, !srcToDst);      // reverse direction of srcToDst for the next level of recursion
        merge_sort(src, m + 1, r, dst, !srcToDst);      // reverse direction of srcToDst for the next level of recursion

        if (srcToDst) merge_dac(src, l, m, m + 1, r, dst, l);
        else          merge_dac(dst, l, m, m + 1, r, src, l);
    }

    // Serial Merge Sort, using divide-and-conquer algorthm
    template< class _Type >
    inline void merge_sort_hybrid(_Type* src, size_t l, size_t r, _Type* dst, bool srcToDst = true)
    {
        if (r < l)  return;
        if (r == l) {    // termination/base case of sorting a single element
            if (srcToDst)  dst[l] = src[l];    // copy the single element from src to dst
            return;
        }
        if ((r - l) <= 48 && !srcToDst) {     // 32 or 64 or larger seem to perform well
            insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);    // want to do dstToSrc, can just do it in-place, just sort the src, no need to copy
            //stable_sort( src + l, src + r + 1 );  // STL stable_sort can be used instead, but is slightly slower than Insertion Sort. Threshold needs to be bigger
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow

        merge_sort_hybrid(src, l,     m, dst, !srcToDst);      // reverse direction of srcToDst for the next level of recursion
        merge_sort_hybrid(src, m + 1, r, dst, !srcToDst);      // reverse direction of srcToDst for the next level of recursion

        if (srcToDst) merge_dac_hybrid(src, l, m, m + 1, r, dst, l);
        else          merge_dac_hybrid(dst, l, m, m + 1, r, src, l);
    }

    template< class _Type >
    inline void merge_sort_inplace_hybrid_with_sort(_Type* src, size_t l, size_t r, bool stable = false, int threshold = 1024)
    {
        if (r <= l) {
            return;
        }
        if ((r - l) <= threshold) {
            if (!stable)
                std::sort(src + l, src + r + 1);
            else
                std::stable_sort(src + l, src + r + 1);
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow

        merge_sort_inplace_hybrid_with_sort(src, l,     m, true, threshold);
        merge_sort_inplace_hybrid_with_sort(src, m + 1, r, true, threshold);

        std::inplace_merge(src + l, src + m + 1, src + r + 1);
    }

    template< class _Type >
    inline void parallel_inplace_merge_sort_hybrid_inner(_Type* src, size_t l, size_t r, bool stable = false, size_t parallelThreshold = 1024)
    {
        if (r <= l) {
            return;
        }
#if 0
        if ((r - l) <= parallelThreshold) {             // Faster than Insertion Sort for use in parallel in-place merge sort
            if (!stable)
                std::sort(src + l, src + r + 1);
            else
                std::stable_sort(src + l, src + r + 1);
            return;
        }
#endif
#if 0
        if ((r - l) <= parallelThreshold) {             // This seems to be the fastest version
            //merge_sort_inplace_hybrid_with_insertion(src, l, r);
            merge_sort_inplace_hybrid_with_sort(src, l, r, stable);
            return;
        }
#endif
#if 1
        if ((r - l) <= 48) {     // 32 or 64 or larger seem to perform well. Don't want users to be able to set threshold too large, as O(N^2)
            insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);
            return;
        }
#endif
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_inplace_merge_sort_hybrid_inner(src, l,     m, stable, parallelThreshold); },
            [&] { parallel_inplace_merge_sort_hybrid_inner(src, m + 1, r, stable, parallelThreshold); }
        );
        //std::inplace_merge(src + l, src + m + 1, src + r + 1);
        //merge_in_place(src, l, m, r);       // merge the results
        //std::inplace_merge(std::execution::par_unseq, src + l, src + m + 1, src + r + 1);
        p_merge_in_place_2(src, l, m, r);
        //p_merge_truly_in_place(src, l, m, r);
    }

    template< class _Type >
    inline void parallel_inplace_merge_sort_hybrid(_Type* src, size_t l, size_t r, bool stable = false, size_t parallelThreshold = 24 * 1024)
    {
        // may return 0 when not able to detect
        const auto processor_count = std::thread::hardware_concurrency();
        //printf("Number of cores = %u \n", processor_count);

        if ((parallelThreshold * processor_count) < (r - l + 1))
            parallelThreshold = (r - l + 1) / processor_count;

        parallel_inplace_merge_sort_hybrid_inner(src, l, r, stable, parallelThreshold);
    }

    template< class _Type >
    inline void parallel_inplace_merge_sort_radix_hybrid_inner(_Type* src, size_t l, size_t r, size_t parallelThreshold = 1024)
    {
        if (r <= l) {
            return;
        }
        if ((r - l) <= parallelThreshold) {
            //hybrid_inplace_msd_radix_sort(src + l, r - l + 1);     // truly In-Place MSD Radix Sort
            parallel_hybrid_inplace_msd_radix_sort(src + l, r - l + 1);
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_inplace_merge_sort_radix_hybrid_inner(src, l,     m, parallelThreshold); },
            [&] { parallel_inplace_merge_sort_radix_hybrid_inner(src, m + 1, r, parallelThreshold); }
        );
        //std::inplace_merge(src + l, src + m + 1, src + r + 1);
        //merge_in_place(src, l, m, r);       // merge the results
        //std::inplace_merge(std::execution::par_unseq, src + l, src + m + 1, src + r + 1);
        p_merge_in_place_2(src, l, m, r);       // truly in-place parallel merge
        //p_merge_in_place_adaptive(src, l, m, r);
    }

    template< class _Type >
    inline void preventative_adaptive_inplace_merge_sort(_Type* src, size_t l, size_t r, double physical_memory_threshold = 0.75, size_t threshold = 48)
    {
        if (r <= l) {
            return;
        }
        if ((r - l) <= threshold) {     // 32 or 64 or larger seem to perform well. Need to avoid setting threshold too large, as O(N^2)
            insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);  // truly in-place
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow

        preventative_adaptive_inplace_merge_sort(src, l,     m, physical_memory_threshold, threshold);
        preventative_adaptive_inplace_merge_sort(src, m + 1, r, physical_memory_threshold, threshold);
        
        merge_inplace_preventative_adaptive(src, l, m, r, physical_memory_threshold);
    }

    template< class _Type >
    inline void parallel_preventative_adaptive_inplace_merge_sort(_Type* src, size_t l, size_t r, double physical_memory_threshold = 0.75, size_t parallelThreshold = 48)
    {
        if (r <= l) {
            return;
        }
        if ((r - l) <= parallelThreshold) {     // 32 or 64 or larger seem to perform well. Need to avoid setting threshold too large, as O(N^2)
            insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);  // truly in-place
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_preventative_adaptive_inplace_merge_sort(src, l,     m, physical_memory_threshold, parallelThreshold); },
            [&] { parallel_preventative_adaptive_inplace_merge_sort(src, m + 1, r, physical_memory_threshold, parallelThreshold); }
        );
        p_merge_in_place_preventative_adaptive(src, l, m, r, physical_memory_threshold);
    }

    template< class _Type >
    inline void parallel_preventative_adaptive_inplace_merge_sort(_Type* src, size_t l, size_t r, bool stable = false, double physical_memory_threshold = 0.75, size_t parallelThreshold = 48)
    {
        if (r <= l) {
            return;
        }
        if ((r - l) <= parallelThreshold) {     // 32 or 64 or larger seem to perform well. Need to avoid setting threshold too large, as O(N^2)
            if (stable)
                insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);  // truly in-place
            else
                std::sort(src + l, src + r + 1);
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        Concurrency::parallel_invoke(
#else
        tbb::parallel_invoke(
#endif
            [&] { parallel_preventative_adaptive_inplace_merge_sort(src, l,     m, stable, physical_memory_threshold, parallelThreshold); },
            [&] { parallel_preventative_adaptive_inplace_merge_sort(src, m + 1, r, stable, physical_memory_threshold, parallelThreshold); }
        );
        p_merge_in_place_preventative_adaptive(src, l, m, r, physical_memory_threshold);
    }

    // Adaptivity at a higher level to minimize the overhead of memory allocation and OS paging-in of newly allocated arrays
    // Allocate the full array once and reuse it during the merge sort ping-pong operation over lg(N) recursion levels
    // TODO: Memory allocation size could be reduced to be (r - l), where swapping of the source and work_buff would need to be done carefully since
    //       the boundaries of one would be l and r, and the other 0 and (r - l), followed by a copy to l to r within the src
template< class _Type >
inline void parallel_preventative_adaptive_inplace_merge_sort_2(_Type* src, size_t l, size_t r, double physical_memory_threshold_post = 0.75, size_t parallelThreshold = 48)
{
    size_t src_size = r + 1;
    size_t anticipated_memory_usage = sizeof(_Type) * src_size / (size_t)(1024 * 1024) + physical_memory_used_in_megabytes();
    double physical_memory_fraction = (double)anticipated_memory_usage / (double)physical_memory_total_in_megabytes();
    //printf("p_merge_in_place_preventative_adaptive: physical memory used = %llu   physical memory total = %llu   anticipated memory used = %llu\n",
    //	physical_memory_used_in_megabytes(), physical_memory_total_in_megabytes(), anticipated_memory_usage);

    if (physical_memory_fraction > physical_memory_threshold_post)
    {
        //printf("Running purely in-place parallel merge sort\n");
        parallel_inplace_merge_sort_hybrid_inner(src, l, r, false, parallelThreshold);
    }
    else
    {
        _Type* work_buff = new(std::nothrow) _Type[src_size];

        if (!work_buff)
            parallel_inplace_merge_sort_hybrid_inner(src, l, r, false, parallelThreshold);
        else
        {
            //printf("Running not-in-place parallel merge sort\n");
            //parallel_merge_sort_hybrid_rh_2(src, l, r, work_buff, stable, true, parallelThreshold); // TODO: test if having it end up in Src and without a copy is faster
#if 0
            parallel_merge_sort_hybrid_rh_1(src, l, r, work_buff, true);    // stable. Not copying is faster, since copy is not parallel - i.e. copying in parallel within the algorithm
            std::copy(work_buff + 0, work_buff + src_size, src + l);
#else
            parallel_merge_sort_hybrid_rh_1(src, l, r, work_buff, false);    // stable. Not copying is faster, since std::copy is not parallel - i.e. copying in parallel within the algorithm is faster
#endif
            delete[] work_buff;
        }
    }
}

    template< class _Type >
    inline void parallel_inplace_merge_sort_radix_hybrid(_Type* src, size_t l, size_t r, size_t parallelThreshold = 24 * 1024)
    {
        // may return 0 when not able to detect
        //const auto processor_count = std::thread::hardware_concurrency();
        //printf("Number of cores = %u \n", processor_count);

        //if ((parallelThreshold * processor_count) < (r - l + 1))
        //    parallelThreshold = (r - l + 1) / processor_count;

        parallel_inplace_merge_sort_radix_hybrid_inner(src, l, r, parallelThreshold);
    }

inline void parallel_linear_in_place_preventative_adaptive_sort(unsigned long* src, size_t src_size, bool stable = true, double physical_memory_threshold_post = 0.75, size_t parallelThreshold = 24 * 1024)
{
    size_t anticipated_memory_usage = sizeof(unsigned long) * src_size / (size_t)(1024 * 1024) + physical_memory_used_in_megabytes();
    double physical_memory_fraction = (double)anticipated_memory_usage / (double)physical_memory_total_in_megabytes();
    //printf("p_merge_in_place_preventative_adaptive: physical memory used = %llu   physical memory total = %llu\n",
    //	physical_memory_used_in_megabytes(), physical_memory_total_in_megabytes());

    if (physical_memory_fraction > physical_memory_threshold_post)
    {
        // In-Place and Stable => no known linear-time sort
        parallel_inplace_merge_sort_hybrid_inner(src, 0, src_size - 1, stable, parallelThreshold);  // not-linear
    }
    else
    {
        unsigned long* work_buff = new(std::nothrow) unsigned long[src_size];

        if (!work_buff)
            parallel_inplace_merge_sort_hybrid_inner(src, 0, src_size - 1, stable, parallelThreshold);  // not-linear
        else
        {
            parallel_merge_sort_hybrid_radix(src, 0, src_size - 1, work_buff, false, parallelThreshold);  // linear
            delete[] work_buff;
        }
    }
}

    template< class _Type >
    inline void merge_sort_inplace(_Type* src, size_t l, size_t r)
    {
        if (r <= l) return;

        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow

        merge_sort_inplace(src, l,     m);
        merge_sort_inplace(src, m + 1, r);

        std::inplace_merge(src + l, src + m + 1, src + r + 1);
    }

    template< class _Type >
    inline void merge_sort_bottom_up_inplace(_Type* src, size_t start, size_t length)
    {
        if (length <= 1)  return;       // nothing to sort since in-place
        size_t l = start;
        size_t r = l + length - 1;      // l and r are inclusive
        for (size_t m = 1; m <= r - l; m = m + m)
            for (size_t i = l; i <= r - m; i += m + m)
                std::inplace_merge(src + i, src + i + m, src + __min(i + m + m, r + 1));
                //merge_in_place(src, i, i + m - 1, __min(i + m + m - 1, r));     // slower than C++ standard inplace_merge
    }

    template< class _Type >
    inline void merge_sort_inplace_hybrid_with_insertion(_Type* src, size_t l, size_t r)
    {
        if (r <= l) return;
        if ((r - l) <= 48) {
            insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);
            return;
        }
        size_t m = r / 2 + l / 2 + (r % 2 + l % 2) / 2;     // average without overflow

        merge_sort_inplace_hybrid_with_insertion(src, l,     m);
        merge_sort_inplace_hybrid_with_insertion(src, m + 1, r);

        //merge_in_place_L1(src, l, m, r);       // merge the results
        std::inplace_merge(src + l, src + m + 1, src + r + 1);
    }

    // TODO: It seems like this algorithm implementation could be simplified, possibly eliminating the first if statement
    template< class _Type >
    inline void merge_sort_bottom_up_inplace_hybrid(_Type* src, size_t start, size_t length)
    {
        if (length <= 1)  return;       // nothing to sort since in-place
        size_t l = start;
        size_t r = l + length - 1;      // l and r are inclusive
        if (length <= 32) {
            insertionSortSimilarToSTLnoSelfAssignment(src + l, r - l + 1);
            return;
        }
        size_t m = 32;
        for (size_t i = l; i <= r; i += m)
            insertionSortSimilarToSTLnoSelfAssignment(src + i, __min(m, r - m + 1));
        for (; m <= r - l; m = m + m)
            for (size_t i = l; i <= r - m; i += m + m)
                std::inplace_merge(src + i, src + i + m, src + __min(i + m + m, r + 1));
                    //merge_in_place(src, i, i + m - 1, __min(i + m + m - 1, r));     // slower than using C++ standard inplace_merge, because standard one is adaptive. I could make mine adaptive too. Performance order is definitely noticable for 100M element array
                    // TODO: Create an adaptive version of my own in-place merge and see if it's faster
            // TODO: This leads to a terrific idea of implementing an adaptive in-place merge sort, which performs not-in-place parallel merge sort when there is sufficient memory, and falls back to the truly in-place merge sort when it has to,
            //       and even then the parallel in-place merge sort is faster than C++ parallel sort.
    }
}
#endif