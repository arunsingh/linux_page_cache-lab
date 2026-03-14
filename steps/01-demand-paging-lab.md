# Step 1 – Demand Paging Lab

## Goal

Understand the difference between:

- virtual address reservation
- physical page allocation
- anonymous read behavior
- anonymous write behavior
- warm access after first touch

## Build

```bash
cd scripts
./build.sh
```

## Run

Open two terminals.

### Terminal 1

```bash
cd bin
./demand_paging
```

### Terminal 2

```bash
cd scripts
./monitor.sh <PID>
```

## What to observe

### Phase 1: `mmap()`

Observe:

- `VmSize` jumps
- `VmRSS` barely moves

Why:

- address space is reserved before pages are physically backed

### Phase 2: sparse reads

Observe:

- minor faults may increase
- RSS may remain much smaller than reservation size

Why:

- read faults on untouched anonymous pages can be satisfied using the shared zero page

### Phase 3: writes

Observe:

- `VmRSS` rises chunk by chunk
- minor faults rise sharply
- major faults usually remain near zero

Why:

- writes allocate private anonymous pages in RAM

### Phase 4: warm scan

Observe:

- fewer additional faults
- stable RSS

Why:

- pages are already present and mapped

### Phase 5: `munmap()`

Observe:

- `VmSize` falls
- `VmRSS` falls

Why:

- mapping and its backing anonymous memory are released

## Candidate tasks

1. Record what changes first: `VmSize` or `VmRSS`
2. Explain why anonymous writes look different from reads
3. Explain why major faults are usually low here
4. Explain what “first touch” means

