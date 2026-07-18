# Concurrency and Atomic

key words:

- data race
- atomic operation
- happens-before

---

## 1. Data Race

When an evaluation of an expression writes to a memory and another evaluation reads or modifies
the same memory location, the expressions are said to `conflict`.  
A program that has two conflicting evaluations has a *data race* unless:

- both evaluations execute on the same thread or in the same `signal handler`, or
- both conflicting evaluations are *atomic operations*(`std::atomic`), or
- one of the conflicting evaluations `happens-before` another(`std::memory_order`).

If a data race occurs,the behavior of the program is undefined.

---

## 2. Atomic Operation

An operation that is never interrupted during execution: 
it either does not occur at all, or it completes fully.  

for example: 
```Cpp
int x = 0;
x++;	// The line of code involving the increment operation will be broken down into
	// three assembly instructions: read, modify, write
```

These three steps are separate. In a multithread environment, this could lead to `data race`.

`std::atomic` transforms the three steps into an **indivisible whole**.

Let's add `<atomic>` header and define an viriable:
```Cpp
std::atomic<int> x{value};
```

### 2.1. read operaton
```Cpp
x.load();
```

### 2.2. write operation
```Cpp
x.store(value);
```

### 2.3. atomically increments
```Cpp
x.fetch_add(1);
```

### 2.4. CAS(compared-and-swap) operation

```Cpp
x.compare_exchange_strong(expected, desired);
x.compare_exchange_weak(expected, desired);

// The weak version is allowed to fail spuriously (returning false even if the current value matches the expected value), 
// whereas the strong version is guaranteed to succeed if the values match
```

Execution logic:

```Cpp
if(current == expected)
{
    current = desired;
    return true;
}
else
{
    expected = current;
    return false;
}
```

### 2.5. `std::memory_order`

go to [memory_order.md](memory_order.md)

---

## 3. happens-before

Used to describe whether an operation in one thread is guaranteed to have completed 
before being observed by another thread.

### Sequenced-before

**Within the same thread**, the effects produced by earlier lines of code are guaranteed 
to be visible to later lines of code.

### Synchronizes-with

An atomic write operation (Release) by Thread 1 can be observed by 
an atomic read operation (Acquire) by Thread 2.