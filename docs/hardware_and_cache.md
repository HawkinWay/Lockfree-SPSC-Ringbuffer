# Hardware and Cache

key words:

- cache line
- false sharing
- alignas (memory alignment)

---

## 1. Cache Line

A smallest unit of data transfer between CPU cache and main memory.

On the vast majority of modern CPUs, the cache line size is typically 64 bytes 
(though some modern high-performance cores may use 128 bytes).  

When a CPU core wants to read a specific variable from memory (such as `int x`), 
it never fetches just those 4 bytes into the cache.
Instead, it loads a fixed-size block of memory all at once. This block is known as a `cache line`.

Due to the principle of `spatial locality`, the computer assumes that if you have accessed data at a specific address, 
there is a high probability you will soon access adjacent data (such as during the sequential traversal of an array). 
Fetching 64 bytes at once can significantly improve the cache hit rate.

---

## 2. False Sharing

False sharing occurs when threads running on different CPU cores modify different variables that happen to reside on the same cache line.   

Due to cache coherence protocols(like MESI protocol), each write forces the cache line to be invalidated and reloaded on other cores.   

Even though the variables are logically independent, their physical sharing causes excessive memory synchronization overhead, severely hurting concurrent performance.

---

## 3. `alignas`

`Memory Alignment` means that the starting address of a data object must be a multiple of its size (or a compiler-specified alignment value).   
Aligned accesses require a single memory operation; misaligned ones may need multiple accesses or cause faults, hurting performance and portability.

To solve the false sharing problem, we can adjust the starting address and spacing of data in memory.  

With `<new>` header since C++17, we can use a compile-time constant: `std::hardware_destructive_interference_size`
(represents the minimum offset between two objects to avoid destructive interference (i.e., false sharing))  
In practice, it equals the cache line size (typically 64 bytes) on most architectures.

```Cpp
#include <new>
#include <atomic>

struct SPSCIndices {
    // Forcefully align `write_index` to a cache line boundary (typically 64 bytes).
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> write_index{0};
    
    // Forcefully align `read_index` to the next cache line boundary again.
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> read_index{0};
};
```

After this operation, the memory layout:
```txt
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ [Cache Line 1] write_index (8B) + 56 bytes compile-time padding  ©¦ -> Exclusive L1 cache
©À©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©È
©¦ [Cache Line 2] read_index (8B)  + 56 bytes compile-time padding  ©¦ -> Exclusive L2 cache
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
```