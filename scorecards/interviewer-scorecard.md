# Interviewer Scorecard – Linux Memory and Process Lab

Candidate: ____________________
Date: ____________________
Interviewer: ____________________

---

## Section A – Practical setup (10 points)

| Item | Score | Notes |
|---|---:|---|
| Builds code without major help | /2 | |
| Runs both labs correctly | /2 | |
| Uses `/proc` or monitor tools correctly | /3 | |
| Records observations clearly | /3 | |

---

## Section B – Demand paging understanding (25 points)

| Skill | Score | Notes |
|---|---:|---|
| Explains virtual memory vs RSS | /5 | |
| Explains first-touch allocation | /5 | |
| Explains minor vs major faults | /5 | |
| Explains zero-page style anonymous read behavior | /5 | |
| Interprets warm re-scan correctly | /5 | |

---

## Section C – Process/thread understanding (25 points)

| Skill | Score | Notes |
|---|---:|---|
| Explains process vs thread clearly | /5 | |
| Knows what threads share | /5 | |
| Knows why processes are heavier | /5 | |
| Understands `task_struct` at a basic level | /5 | |
| Understands `mm_struct` at a basic level | /5 | |

---

## Section D – Page cache understanding (25 points)

| Skill | Score | Notes |
|---|---:|---|
| Explains round 1 vs round 2 timing difference | /5 | |
| Understands page cache vs private user memory | /5 | |
| Explains why isolated processes can still share file cache benefits | /5 | |
| Can compare threads/processes/hybrid modes | /5 | |
| Connects file-backed access to cache warmth | /5 | |

---

## Section E – Communication quality (15 points)

| Skill | Score | Notes |
|---|---:|---|
| Uses precise but simple language | /5 | |
| Explains observations before jargon | /5 | |
| Can reason from outputs instead of memorized phrases | /5 | |

---

## Overall score

Total: ______ / 100

## Suggested rating bands

- 85–100: strong systems understanding
- 70–84: good working understanding
- 50–69: partial understanding, needs guidance
- below 50: weak fundamentals or highly memorized answers

## Red flags

- Confuses virtual memory reservation with immediate physical allocation
- Says threads have separate address spaces by default
- Cannot explain why repeated file reads get faster
- Cannot distinguish page cache from process RSS
- Uses terms like `task_struct` or `mm_struct` without explaining their role
