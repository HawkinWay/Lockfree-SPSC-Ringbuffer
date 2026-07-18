# Memory Order

key words:

- memory order
- acquire
- release

---

## 1. Memory Order

Memory order specifies the ordering constraints on atomic operations, 
affecting compiler optimizations, CPU instruction reordering, 
and visibility of memory effects between threads.

for example:
```Cpp
int a = 1;	// operation 1
int b = 2;	// operation 2
```

The CPU determines that the order of execution between *operation 1* and *operation 2* has 
no impact on the final result, so it might execute *operation 2* before *operation 1*.  
This type of optimization is known as the `As-if` rule(meaning the outcome remains the same).

---

## 2. `std::memory_order`

C++ provides 6 different `std::memory_order` options, ranging from weak to strong constraints
on reordering, and from low to high in terms of overhead: 
```Cpp
enum memory_order
{
    memory_order_relaxed,   // *
    memory_order_consume,
    memory_order_acquire,   // *
    memory_order_release,   // *
    memory_order_acq_rel,
    memory_order_seq_cst    // *
};
```

### 2.1. std::memory_order_relaxed

This guarantees only the atomicity of the operation itself, 
not the execution order of any preceding or subsequent code.

Used when a thread accesses an atomic variable that only itself modifies: 
```Cpp
size_t curr_w = write_index.load(std::memory_order_relaxed); // 0 overhead
```

### 2.2. std::memory_order_release

Within the current thread, any *write operations* preceding it must absolutely not be reordered
to occur after it.

Used in write/store: 
```Cpp
buffer[curr_w] = item;  // 1. write/store low-level data
write_index.store(next_w, std::memory_order_release);   // 2. ensure 1 is never reordered to come after 2
```

### 2.3. std::memory_order_acquire

Within the current thread, any *read/write operations* positioned after it must absolutely
not be reordered to precede it.

Used in read/load:
```Cpp
size_t curr_w = write_index.load(std::memory_order_acquire);    // 1. ensure 2 is never reordered to precede to 1
item = buffer[curr_w];  // 2. read/load low-level data
```

### 2.4. std::memory_order_seq_cst

Cpp's **default behavior** (if you don't pass any arguments).  

This is the strictest constraint, requiring that the global order of atomic operations observed 
by all CPU cores be completely consistent.  

This takes highest overhead, severely undermining multi-core parallel performance. 
To squeeze the ultimate performance out of the lock-free queue, 
we can replace `seq_cst` with `acquire-release`.