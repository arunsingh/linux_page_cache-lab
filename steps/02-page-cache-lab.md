# Step 2 – Page Cache Lab

## Goal

Understand page cache and compare file reads across:

- single process
- threads
- processes
- hybrid execution

## Prepare test data

```bash
cd scripts
./setup_testdata.sh 128
```

This creates `../testdata.bin`.

## Run examples

```bash
cd scripts
./run_page_cache_lab.sh ../testdata.bin
```

Or run individual modes:

```bash
cd bin
./page_cache_models ../testdata.bin single 1 1 2
./page_cache_models ../testdata.bin threads 1 4 2
./page_cache_models ../testdata.bin processes 4 1 2
./page_cache_models ../testdata.bin hybrid 2 2 2
```

## What to observe

### Round 1 vs Round 2

Observe:

- round 2 is often faster

Why:

- file data may already be resident in page cache

### Threads vs processes

Observe:

- threads often show lower overhead than processes

Why:

- threads share address space and many runtime resources
- processes require more kernel bookkeeping and separate memory-management state

### Shared page cache despite process isolation

Observe:

- multiple processes still often read faster in later rounds

Why:

- page cache is a kernel-wide file cache, not a private per-process user address space feature

## Candidate tasks

1. Compare mode timings
2. Explain why multiple processes can still benefit from the page cache
3. Explain what threads share that processes do not
4. Explain why file-backed warm reads differ from anonymous memory faults

