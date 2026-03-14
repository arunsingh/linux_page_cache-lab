# Visualizing the End Result

These labs are meant to make the following Linux memory model visible and testable.

```mermaid
flowchart TD
    A[User program] --> B[Virtual address space]
    B --> C[Page tables]
    C --> D[Demand paging]
    D --> E[Anonymous pages in RAM]
    A --> F[read file]
    F --> G[Linux page cache]
    G --> H[Disk only on cache miss]
    A --> I[Threads]
    A --> J[Processes]
    I --> K[Shared address space]
    J --> L[Independent address spaces]
```

## Mental model

- `mmap()` usually reserves address space first
- first touch can trigger page faults
- writes to anonymous pages allocate real physical memory
- repeated file reads often become faster because of page cache
- threads usually share one address space
- separate processes usually do not share an address space, but can still benefit from shared kernel page cache
