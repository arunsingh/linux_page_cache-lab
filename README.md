# Container From Scratch – Linux Memory, Process, and Filesystem Lab Pack

A cleaner candidate-ready repo layout for self-paced Linux systems labs.

This pack now covers two practical memory labs that complement the container/filesystem track:

- `demand_paging` — anonymous memory, first-touch allocation, RSS growth, zero page behavior
- `page_cache_models` — page cache behavior across single process, threads, processes, and hybrid models

The structure is designed for both:

- **self-help candidate practice**
- **interviewer-led evaluation**

---

## Repo Layout

```text
container-from-scratch-clean/
├── README.md
├── assets/
│   └── visualization.md
├── answers/
│   ├── answer-key.md
│   └── expected-observations.md
├── bin/
│   ├── demand_paging
│   └── page_cache_models
├── labs/
│   ├── demand_paging.c
│   └── page_cache_models.c
├── scorecards/
│   └── interviewer-scorecard.md
├── scripts/
│   ├── build.sh
│   ├── monitor.sh
│   ├── run_demand_paging_lab.sh
│   ├── run_page_cache_lab.sh
│   └── setup_testdata.sh
└── steps/
    ├── 01-demand-paging-lab.md
    └── 02-page-cache-lab.md
```

---

## Visualizing the End Result

See `assets/visualization.md` for the candidate-friendly diagram section.

---

## Minimal System Requirements

Recommended home-lab setup:

- 2 CPU cores
- 4 GB RAM minimum
- 8 GB RAM preferred
- 5 GB free disk
- Linux host or Linux VM
- `gcc`, `bash`, `make`, `coreutils`, `pthread`

Works well on:

- Ubuntu 22.04+
- Debian 12+
- Vagrant VM
- local Linux laptop

---

## Quick Start

### 1. Build

```bash
cd scripts
./build.sh
```

### 2. Run lab 1

```bash
./run_demand_paging_lab.sh
```

### 3. Prepare file for lab 2

```bash
./setup_testdata.sh 128
```

### 4. Run lab 2

```bash
./run_page_cache_lab.sh ../testdata.bin
```

---

## Candidate Learning Goals

By the end of these labs, the candidate should be able to explain:

- process vs thread
- what threads share and what they do not
- why processes have isolated address spaces
- what `task_struct` and `mm_struct` broadly represent
- virtual memory vs resident memory
- demand paging and first-touch allocation
- anonymous memory vs file-backed memory
- page cache behavior across repeated reads
- why `/proc/<pid>` and `/proc/meminfo` show different views

---

## Suggested Learning Flow

Start here:

1. `steps/01-demand-paging-lab.md`
2. `steps/02-page-cache-lab.md`
3. Attempt the quiz without opening `answers/`
4. Use `answers/expected-observations.md` only after making your own notes

Interviewers should use:

- `scorecards/interviewer-scorecard.md`
- `answers/answer-key.md`

---

## What To Observe During The Labs

### Demand paging lab

Watch:

- `VmSize`
- `VmRSS`
- minor page faults
- major page faults

Expected pattern:

- `mmap()` increases virtual size first
- sparse reads may create minor faults but limited RSS growth
- writes cause RSS growth and many minor faults
- warm re-scan creates far fewer new faults

### Page cache lab

Compare:

- first run vs second run
- one process vs many threads
- many processes vs one process
- hybrid execution model

Expected pattern:

- later rounds are often faster because of page cache warmth
- threads usually have lower creation/coordination overhead
- processes remain isolated in address space but still benefit from shared page cache

---

## Interview Usage

This repo supports two interview styles.

### Style A — guided practical

Ask the candidate to run the steps and explain observations live.

### Style B — take-home sandbox

Ask the candidate to:

- run both labs
- fill in a short observation report
- answer the quiz in their own words
- explain why the observed behavior happens

---

## Safety Notes

For colder page-cache experiments, candidates may use:

```bash
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches
```

Only do this on a disposable VM or personal sandbox.

