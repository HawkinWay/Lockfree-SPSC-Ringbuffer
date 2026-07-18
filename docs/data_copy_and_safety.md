# Data Copy and Safety

key words:

- trivially copyable
- memcpy(SIMD)
- static_assert
- std::runtime_error

---

## 1. Trivially Copyable

A type is trivially copyable if its objects can be safely copied by `byte-by-byte memory copying` (e.g., using memcpy) 
without invoking copy constructors, move constructors, copy assignment operators, or move assignment operators.

Conditions (**all must hold**)
- No user‑provided copy/move constructors or copy/move assignment operators
- Trivial destructor (implicit or explicitly defaulted)
- All non‑static data members and base classes are also trivially copyable
- No virtual functions or virtual base classes

```Cpp
struct Point {
    int x, y;          // built‑in types are trivially copyable
};

struct Complex {
    double re, im;
    Complex(double r, double i) : re(r), im(i) {}  // custom constructor is fine
    // no custom copy/move/dtor → still trivially copyable
};

static_assert(std::is_trivially_copyable_v<Point>);   // true
static_assert(std::is_trivially_copyable_v<Complex>); // true


struct NonTrivial {
    std::string s;     // std::string is NOT trivially copyable
};

struct HasVirtual {
    virtual ~HasVirtual() {}  // virtual destructor → non‑trivial
};
```

--- 

## 2. `memcpy` and SIMD

`memcpy` is the C standard library function for copying memory blocks byte‑by‑byte. 
Modern compilers optimize memcpy into SIMD instructions (e.g., SSE, AVX) that copy 16, 32, 
or even 64 bytes per instruction, dramatically improving throughput.

### SIMD(Single Instruction, Multiple Data)

A CPU feature that allows one instruction to perform the same operation on multiple data elements simultaneously.

```Cpp
#include <cstring>

float src[1024], dst[1024];
memcpy(dst, src, sizeof(src));  
// Compiler may generate:
// vmovups ymm0, [src]   ; load 32 bytes at once
// vmovups [dst], ymm0   ; store 32 bytes at once
// Loop unrolled to copy 256 bytes per iteration
```

### Critical Prerequisites
- Source and destination memory must be properly aligned (especially for SIMD requirements)
- The type must be `trivially copyable` — otherwise memcpy is undefined behavior

### Performance Comparison (copying 1 MB)

| Method | Approx.Time | Notes |
|--|----|--|
|Byte‑by‑byte loop | ~5 ms | Slowest |
|8‑byte chunk loop | ~2 ms | Manual unrolling |
|memcpy (compiler‑optimized) |	~0.3 ms |Auto‑vectorized SIMD |
|Hand‑written AVX intrinsics |	~0.2 ms	| Near‑peak performance |

 In modern development, simply using `memcpy` automatically leverages SIMD — 
 no need to write assembly or intrinsics manually.

---

## 3. `static_assert`

static_assert (introduced in C++11) is a **compile‑time assertion**. 
If the constant expression evaluates to false, the compilation fails with a user‑specified error message.

Syntax:
```Cpp
static_assert(constant_expression, "error message");
```


## 4. `std::runtime_error`

Need `<stdexcept>` header.  
`std::runtime_error` is a standard exception class in C++ (inheriting from `std::exception`). 
It represents errors that can only be detected at runtime, such as file I/O failures, network timeouts, or out‑of‑range numerical operations.  

```cpp
#include <stdexcept>
#include <iostream>

void readFile(const std::string& path) {
    if (path.empty()) {
        throw std::runtime_error("File path cannot be empty");
    }
    // Simulate failure
    if (path == "/dev/null") {
        throw std::runtime_error("Cannot open file: " + path);
    }
}

int main() {
    try {
        readFile("/dev/null");
    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Other exception: " << e.what() << std::endl;
    }
}
```