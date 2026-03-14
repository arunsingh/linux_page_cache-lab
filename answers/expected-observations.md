# Expected Observations

## Demand paging

- `VmSize` should increase right after `mmap()`
- `VmRSS` should remain relatively low until write-touch begins
- sparse reads may increase minor faults without full RSS growth
- write-touch should increase RSS and minor faults substantially
- major faults should often remain near zero because anonymous zero-filled pages do not require disk reads
- warm scans should cause far fewer new faults

## Page cache

- round 2 should often be faster than round 1
- threads often have lower coordination overhead than processes
- processes remain isolated in user address space but still benefit from shared kernel page cache
- hybrid mode often resembles real-world servers with workers + internal concurrency
