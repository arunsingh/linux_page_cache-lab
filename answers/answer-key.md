# Answer Key

## Demand Paging Quiz

### 1. Why did `VmSize` increase before `VmRSS`?
Because `mmap()` typically reserves virtual address space immediately, while physical memory is generally committed when pages are first touched.

### 2. Why can sparse reads show minor faults without major RSS growth?
Untouched anonymous pages may be mapped to the kernel's shared zero page for read access, which avoids allocating separate private writable pages immediately.

### 3. Why are major faults usually near zero in this lab?
This is anonymous memory. First-touch allocation of zero-filled anonymous pages usually does not require disk I/O.

### 4. What happens on the first write to an anonymous page?
A page fault occurs and the kernel allocates a private physical page, updates the page tables, and resumes execution.

### 5. Why is the warm scan cheaper?
Because many pages are already resident and mapped, so fewer new faults are needed.

## Page Cache Quiz

### 1. Why is round 2 often faster?
The file's data may already be in the Linux page cache, reducing storage access.

### 2. Why can multiple processes still benefit from the same page cache?
The page cache is maintained by the kernel per file/page state, not as a private user-space cache per process.

### 3. What do threads share that processes usually do not?
Threads in the same process typically share:
- `mm_struct` / address space
- open file table references
- signal dispositions
- code and heap mappings

### 4. Why can processes be heavier than threads?
Processes need separate memory-management state, separate page tables, and more setup/teardown work.

### 5. Difference between address-space isolation and page-cache sharing?
Address-space isolation means one process cannot directly access another process's private virtual memory. Page-cache sharing means the kernel can satisfy file-backed reads from the same cached file pages for multiple processes.

## Kernel Subsystem Basics

### `task_struct`
A kernel structure representing a schedulable task, including process/thread execution context and metadata.

### `mm_struct`
A kernel structure representing a process memory map / address space.

### Do threads share the same `mm_struct`?
Usually yes, for threads within the same process.

### `/proc/<pid>/status` vs `/proc/meminfo`
`/proc/<pid>/status` is process-specific. `/proc/meminfo` is a system-wide memory summary.
