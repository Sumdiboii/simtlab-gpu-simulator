# SimtLab Technical Specification

**Status:** Phase 0.1 baseline. This document is the architectural source of truth; changes require an explicit specification revision.

## 1. Scope and design philosophy

SimtLab is an educational CPU-based simulator for understanding kernel execution on a simplified SIMT machine. It loads a small assembly mini-ISA, launches blocks over a grid and executes instructions warp by warp. Simulation is single-threaded on the host; parallelism is represented by deterministic interleaving of warp state.

Included are 32-lane warps, predication, divergence and reconvergence, global and per-block shared memory, coalescing and bank-conflict accounting, configurable instruction costs and scheduling, SM resource-limited occupancy, conceptual tile MMA, small representative workloads, and text/JSON statistics. The executable is `simtlab`; the C++ namespace is `simtlab`.

Excluded are PTX/SASS compatibility, detailed cache hierarchy, DRAM controllers, accurate NVIDIA timing or tensor-core behavior, multi-GPU execution, real parallel simulation and GUI development. Simulated cycles are internal scheduling units and do not map directly to nanoseconds or real GPU cycles. Phase 7 will calibrate placeholder costs using T4 microbenchmarks; the model remains cycle-approximate, not cycle-accurate.

Keep the implementation understandable and testable: prefer deterministic rules, a small ISA, explicit assumptions, small workloads, and no premature optimization or unplanned features.

## 2. Mini-ISA

### 2.1 Assembly conventions

Use one instruction per line. Mnemonics/registers are case-insensitive; commas separate operands; labels are identifiers followed by `:` on a line; `#` begins a comment. Blank lines are ignored. GPRs are `r0`–`r31`, each a 32-bit untagged bit pattern; predicate registers are `p0`–`p3`. Integer literals are decimal or `0x` hexadecimal, with optional sign. Floating literals are decimal and rounded to IEEE-754 binary32. Integer arithmetic wraps modulo 2^32. The consuming opcode determines whether a GPR bit pattern is treated as integer or f32; comparisons never infer a type.

Memory is byte-addressable. A memory operand is `[base]`, `[base+imm]`, `[base-imm]`, or `[base+rN]`; base and register offsets are unsigned 32-bit addresses, and immediate displacements are signed 32-bit values. Address addition is evaluated as a mathematical signed sum and must lie in `[0, 2^32-1]`; otherwise it raises an address-overflow error. The global address space has size at most 2^32 bytes (4 GiB), with valid byte addresses 0 through 2^32−1 and an exclusive allocation end no greater than 2^32. Access widths for scalar memory instructions are exactly 1, 2 or 4 bytes. Loads zero-extend; stores use the low bytes of the source register. All values are little-endian. There are no vector memory instructions.

An optional `@pN` or `@!pN` prefix predicates ordinary instructions lane by lane. A false predicate causes no architectural operation or memory request, though the warp instruction still issues and consumes its normal issue slot. At issue, **active lanes** are those in the warp's active mask; **participating lanes** are active lanes whose instruction predicate is true; **retired lanes** are permanently removed from execution. Control-flow predication is defined in §4. `EXIT` retires selected lanes; any remaining active lanes continue at the next PC. All GPRs initialize to zero except registers bound to launch arguments; predicates initialize false. Invalid register references, operand forms, or writes to read-only special registers are errors.

In the table, `d` is a GPR destination, `p` a predicate destination, `a`/`b`/`c` GPR sources, `imm` an immediate, and `width` one of 1, 2, 4. Unless stated otherwise, operations apply independently to active lanes. Cost categories resolve through §7.

| Mnemonic | Operand format and interpretation | Semantics | Predication | Cost |
|---|---|---|---|---|
| ADD | `d,a,b` or `d,a,imm`; u32 bit patterns | wrapping addition | Yes | integer |
| SUB | `d,a,b` or `d,a,imm`; u32 bit patterns | wrapping subtraction | Yes | integer |
| MUL | `d,a,b`; u32 bit patterns | low 32 bits of product | Yes | integer |
| FADD | `d,a,b`; f32 | binary32 addition | Yes | float |
| FMUL | `d,a,b`; f32 | binary32 multiplication | Yes | float |
| FMA | `d,a,b,c`; f32 | `a*b+c`, one rounding (fused multiply-add) | Yes | FMA |
| AND | `d,a,b` or `d,a,imm`; u32 | bitwise AND | Yes | integer |
| OR | `d,a,b` or `d,a,imm`; u32 | bitwise OR | Yes | integer |
| SHL | `d,a,b/imm`; u32 | left shift by count's low 5 bits | Yes | integer |
| SHR | `d,a,b/imm`; u32 | logical right shift by count's low 5 bits | Yes | integer |
| SETP.EQ | `p,a,b`; integer | true when 32-bit patterns equal | Yes | integer |
| SETP.NE | `p,a,b`; integer | true when 32-bit patterns differ | Yes | integer |
| SETP.LT | `p,a,b`; signed i32 | signed less-than | Yes | integer |
| SETP.LE | `p,a,b`; signed i32 | signed less-or-equal | Yes | integer |
| SETP.GT | `p,a,b`; signed i32 | signed greater-than | Yes | integer |
| SETP.GE | `p,a,b`; signed i32 | signed greater-or-equal | Yes | integer |
| SETP.FEQ | `p,a,b`; f32 | IEEE ordered equality | Yes | float |
| SETP.FNE | `p,a,b`; f32 | IEEE inequality, true if unordered | Yes | float |
| SETP.FLT | `p,a,b`; f32 | IEEE ordered less-than | Yes | float |
| SETP.FLE | `p,a,b`; f32 | IEEE ordered less-or-equal | Yes | float |
| SETP.FGT | `p,a,b`; f32 | IEEE ordered greater-than | Yes | float |
| SETP.FGE | `p,a,b`; f32 | IEEE ordered greater-or-equal | Yes | float |
| LDG | `d,[addr],width`; unsigned result | load global bytes, zero-extended | Yes | global memory |
| STG | `[addr],a,width`; low bytes | store to global memory | Yes | global memory |
| LDS | `d,[addr],width`; unsigned result | load block shared bytes, zero-extended | Yes | shared memory |
| STS | `[addr],a,width`; low bytes | store to block shared memory | Yes | shared memory |
| BRA | `target,reconv` | split lanes by predicate; §4 | Lane selection | branch |
| JMP | `target` | all-active transfer, all-or-none predicated transfer; §4 | Yes, mixed predicate errors | branch |
| BAR.SYNC | `imm`; barrier ID 0–15 | block barrier; §6 | Full participating warp only | barrier |
| EXIT | no operands | retire selected lanes | Yes | branch |
| SREG | `d,special` | read special register as u32; §2.2 | Yes | integer |
| MMA.M16N16K16 | `d,a,b`; base GPRs | 16×16×16 tile multiply-accumulate; §8 | Unpredicated full warp only | MMA |

There are 32 concrete forms: ten arithmetic/logic instructions, twelve `SETP` forms, four memory instructions, four control instructions, `SREG`, and `MMA.M16N16K16`.

Special registers are `LANEID`, `WARPID`, `TID.X/Y/Z`, `CTAID.X/Y/Z`, `NTID.X/Y/Z`, and `GRIDID.X/Y/Z`. Coordinates are u32; `TID` is the original block coordinate, `CTAID` the block coordinate, and `NTID`/`GRIDID` the block/grid dimensions. Their coordinate flattening and warp/lane mapping are defined in §10. Global pointers are 32-bit GPR byte addresses supplied through launch allocation bindings; no special register contains a pointer.

Example:

```asm
; r0 = thread x; load and increment one word
SREG r0, TID.X
MUL r1, r0, 4
LDG r2, [r10+r1], 4
ADD r2, r2, 1
STG [r11+r1], r2, 4
EXIT
```

This example assumes the input and output allocation bases are bound to `r10` and `r11`.

## 3. Registers and execution model

Each thread owns 32 GPRs and four predicates. Each warp has one PC, per-lane register/predicate state, a 32-bit active mask, reconvergence stack, ready cycle, and state. A block is nonempty and contains a positive multiple of 32 threads; partial warps are disallowed. Warps and lanes follow increasing flattened thread order, lane ID equal to flattened thread index modulo 32. A retired lane is permanently inactive. A warp completes when no live lanes remain. Invalid register references are diagnosed, not clamped or defaulted.

## 4. Divergence, reconvergence, and EXIT

`BRA target,reconv` uses the current active mask. For `@pN BRA`, `T` is the subset with `pN=true` and `F` the remaining active lanes; for `@!pN`, these sets reverse. An unprefixed `BRA` takes all active lanes and has empty `F`. If one set is empty, continue at target for `T` or at the next PC for `F`, without a stack entry. If both are nonempty, push `{reconv_pc, deferred_pc=pc+1, deferred_mask=F, reconv_mask=T}`, execute `T` first at target. A reconvergence label must be a valid instruction PC and both nonterminated paths must reach it.

An unprefixed `JMP` transfers all active lanes. For predicated `JMP`, all active lanes satisfying the predicate jump and none satisfying it continue at the next PC; if the predicate is mixed across active lanes, raise a runtime error. `JMP` never creates a divergence or reconvergence entry.

When a path reaches its saved reconvergence PC, switch to the deferred path if one remains. When the deferred path reaches that PC, pop the entry and restore the union of its saved masks, with all already-retired lanes removed. If `EXIT` retires only part of the current active mask, the remaining active lanes continue at the next PC. If it retires all lanes on the current path, the scheduler unwinds the innermost outstanding entry: run its deferred path if it has live lanes, otherwise complete that entry and continue unwinding. Retired lanes are removed from all masks in all outstanding entries and can never reactivate. Thus, if either divergent path terminates, its deferred/sibling path still executes when live lanes remain. Nested divergence uses a stack and is processed last-in, first-out. A jump that skips or crosses the top entry's reconvergence PC is invalid, except a `JMP` directly to that PC. Reaching the end of the program with live lanes is invalid control flow. Invalid target/reconvergence flow and stack overflow (default maximum depth 32, configurable) produce runtime errors. Divergent-branch count increments only when both split masks are nonempty.

Example: lanes with `p0=true` take the then path; the others execute the else path. Both explicitly reach `done`.

```asm
SETP.LT p0, r0, 16
@p0 BRA then_path, done
else_path:
ADD r1, r1, 2
JMP done
then_path:
ADD r1, r1, 1
done:
EXIT
```

## 5. Memory model

### Global memory

Global memory is one flat byte array of at most 4 GiB, divided into bounded allocations with stable base addresses. Host input copies bytes into an allocation; host output copies a selected range back. Allocations do not overlap. Blocks share this space. Effective addresses use the 32-bit base-plus-offset rule in §2.1; any arithmetic overflow or participating-lane access outside its allocation is a runtime error. Nonparticipating lanes perform no access.

Each participating lane access is one global request. For each warp memory instruction, collect all 128-byte segments touched by participating-lane byte ranges: segment number is `floor(byte_address/128)`, and an access crossing a boundary touches both segments. Global segment count sums distinct segments per instruction, not across the whole kernel. Coalescing efficiency is total useful bytes from participating lanes divided by 128 times total counted segments, bounded by 1; it is 1 when there are no segments, with zero requests reported separately. Functional loads/stores occur at issue in increasing lane-ID order; overlapping stores leave the highest lane's bytes last. This deterministic rule does not define hardware race behavior.

### Shared memory

Each block owns a zero-initialized byte array sized at launch, private to that block. Participating-lane byte ranges map to bank `floor(address/4) mod 32` and aligned word `floor(address/4)`. A lane spanning multiple words contributes to each corresponding bank/word. For reads, lanes requesting the same word are one broadcast; distinct words in one bank conflict. For writes, one lane contributes one request per distinct word address it touches; different lanes writing the same word remain distinct requests and are applied in increasing lane ID, highest lane last. Write conflict degree is the maximum number of distinct `(lane, word address)` requests in any bank; bytes within one lane/word are never counted separately. Read conflict degree is the maximum number of distinct word addresses in any bank after same-address broadcasts. The instruction conflict degree is the maximum of these bank counts (zero if no requests); the counter adds `max(0, degree−1)` per shared instruction. Bounds violations are runtime errors. Accounting affects cost/counters only, not values or a detailed hardware pipeline.

## 6. Synchronization

`BAR.SYNC id` is a block-wide barrier at a dynamic encounter. A fully retired warp no longer participates in future barriers. A warp with some, but not all, lanes retired cannot participate and attempting to reach a barrier in that state is an error. Each remaining live warp must reach the same ID and encounter; partial-warp predication at a barrier is invalid. A warp arrival waits and cannot issue. The barrier releases only after every remaining live warp in the block arrives; all arrivals then advance past it and become ready at release cycle plus barrier cost. Instruction-latency-delayed warps arrive when they issue the barrier. Different IDs/encounters, inconsistent participation, or live warps that terminate or cannot reach the required barrier produce a diagnostic identifying the block, barrier, and waiting/missing warps. Barrier IDs may be reused after release; a per-ID encounter count distinguishes reuse. A block with no live warps completes.

## 7. Timing, scheduling, and execution limits

All costs are configurable cycle values; defaults are placeholders: integer ALU 1, float ALU 2, FMA 2, shared memory base 2, global memory base 20, branch 1, barrier 1, and MMA 16. A global instruction adds 4 cycles per distinct segment after the first. Shared memory adds 1 cycle per conflict degree above one. These values are parameters, not constants embedded in execution logic.

Each warp has `ready_cycle`. An instruction issued at cycle `c` makes a non-barrier warp ready at `c + max(1, base_cost + access_penalty)`. Each SM has one issue slot per cycle. At each cycle, SMs are processed in ascending SM ID; each independently chooses one eligible resident warp by round-robin block-local warp ID, starting after that SM's prior choice. No SM can issue more than once in a cycle. This defines same-cycle global ordering, including deterministic ordering of functional memory effects, and is independent of host container iteration order. The host implementation remains single-threaded. When no warp is ready now, advance to the next event cycle with a ready warp or barrier release; cycles advance monotonically.

Warp-cycle stall counters count resident live warp cycles that do not issue, at most one reason per warp per cycle. Assign the reason by this precedence: barrier wait; memory wait for a pending global/shared instruction; instruction latency wait for a non-memory instruction; otherwise no warp stall (the warp is eligible). These are disjoint per-warp counters and can sum to more than elapsed kernel cycles because multiple warps stall concurrently. Aggregate stall cycles are their sum. Idle SM cycles are counted separately as SM cycles in which no warp issues; idle cycles are not added to warp stall counters. Memory waits are separated into global and shared memory. Barrier waiting takes precedence even if a warp's last operation also had latency.

Configurable safety limits are a maximum total warp-instruction issue count and maximum elapsed simulated cycles. Defaults are 100,000,000 issues and 1,000,000,000 cycles; either can be overridden by CLI/configuration. The issue count increments for every issued warp instruction, including predicated-false instructions. If either limit would be exceeded before all blocks complete, stop deterministically and report a limit-exceeded error with the limit, cycle, issue count, and active block/warp states; do not return partial statistics as a successful result.

## 8. Tensor-tile MMA

Syntax is `MMA.M16N16K16 d, a, b`; each operand is a base GPR. Each lane holds eight consecutive binary32 values beginning at that base, so every operand requires registers `[base, base+7]` with `base+7 <= 31`. The D, A and B spans must be pairwise disjoint. A span outside `r0`–`r31` or any overlap is a validation/runtime error. For row-major element index `i` in a 256-value tile, lane `i mod 32` holds slot `floor(i/32)` in its operand register span. Thus each of A, B and accumulator D is a 16×16 row-major tile; semantics are `D = D + A×B`.

The instruction must be unpredicated, all 32 lanes active, and all lanes at the same warp PC; otherwise it is an error. Each output accumulates in binary32 in increasing K order with one fused multiply-add per term. Cost is one configurable MMA cost per warp instruction; operations count 2×16×16×16 FLOPs. This is a conceptual educational operation, not a Tensor Core emulator: formats, fragments, throughput, pipelining and NVIDIA accuracy are not modeled.

## 9. Statistics

Counters are per kernel launch and aggregate all SMs/blocks unless their definition states otherwise. Text and JSON output use identical values.

| Statistic | Unit and calculation |
|---|---|
| Total simulated cycles | elapsed global cycles from cycle 0 through completion of the final block; includes idle SM cycles while work remains |
| Instructions issued | warp instruction issues, including predicated-false instructions; each issue consumes one SM slot |
| Issue slot utilization | total issues / (SM count × total simulated cycles); zero if duration is zero |
| Divergent branches | `BRA` issues whose taken and deferred masks are both nonempty |
| Average active threads per warp | sum of active-mask lane counts at issue / instruction issues; zero if none |
| Global requests | lane accesses, once per participating lane access |
| Global segments | distinct 128-byte segments per warp memory instruction, summed across instructions (§5) |
| Coalescing efficiency | useful bytes from participating lanes / (128 × global segments); 1 if no segments |
| Shared bank conflicts | sum of `max(0, degree−1)` per shared warp instruction (§5) |
| Stall cycles by reason | warp-cycle counts as defined in §7; disjoint across reasons, may exceed elapsed cycles in aggregate |
| Idle SM cycles | SM-cycle count with no issue; separate from warp stalls |
| Aggregate stall cycles | sum of instruction-latency, global-memory, shared-memory, and barrier warp-cycle counts |
| Achieved occupancy | `sum_c R(c) / (duration × SM_count × max_resident_warps_per_SM)` for cycles `c=0..duration−1`, where `R(c)` is total resident live warps across all SMs; also report peak fraction `max_c R(c)/(SM_count × max_resident_warps_per_SM)` |
| Bytes moved | bytes successfully loaded/stored by participating lanes, global and shared separately; repeated lane accesses count repeatedly |
| Operations executed | one per participating-lane integer/float arithmetic or compare; FMA counts 2; MMA counts 8192 FLOPs per warp instruction; memory/control count 0 |

For zero-duration simulations, achieved and peak occupancy are both zero. For instruction-specific counters, effects, requests, useful bytes and operations use participating lanes; the active-thread average uses active-mask lanes at issue. Predicated-false ordinary instructions still issue as specified in §2.1.

## 10. Kernel launch and SM model

Launch dimensions `--grid X,Y[,Z]` and `--block X,Y[,Z]` are positive integers; omitted trailing dimensions are 1. Block thread count must be divisible by 32. Thread/warp counts are the dimension product and thread count divided by 32. For block dimensions `(NTID.X,NTID.Y,NTID.Z)`, coordinates map to `threadIndex = x + y * NTID.X + z * NTID.X * NTID.Y` (X fastest); `WARPID=floor(threadIndex/32)` and `LANEID=threadIndex mod 32`. `TID.X/Y/Z` report the original `(x,y,z)` coordinates. Block IDs flatten X fastest, then Y, then Z; special registers follow these dimensions.

`registers_per_thread` is derived from the largest GPR index referenced anywhere in the parsed program, plus one; a program with no GPR references uses zero. For MMA, every register in each base-plus-seven span is considered referenced. A launch binding must name a valid referenced GPR (`r0`–`r31`), and each GPR may have at most one binding; invalid, duplicate, or unreferenced-register bindings are launch errors. A valid binding initializes that register to the allocation's 32-bit base in every thread and does not otherwise alter the derived register count. There is no register allocator or launch override. Per-block register allocation is `threads × registers_per_thread`, rounded up to the configurable allocation granularity (default 256 registers). Shared-memory allocation is requested bytes rounded up to 256 bytes.

Default SM limits are 64 resident warps, 16 resident blocks, 65,536 32-bit registers and 98,304 shared-memory bytes; a kernel has a configurable SM count. Each SM's total register and shared-memory capacities are also the per-block ceilings; a candidate whose rounded use exceeds either ceiling fails launch. Maximum resident blocks is the minimum of floor(capacity/use) over nonzero per-block resource uses, additionally bounded by block and warp limits. A zero-use resource imposes no constraint. If every use is zero, block and warp limits still bound residency. Occupancy is resident warps divided by maximum resident warps. No block is partially resident.

Assign blocks in flattened ID order to the lowest-numbered SM with capacity; after a block completes, assign the next block there. Multiple SMs advance on one global cycle timeline using the ordering in §7. Kernel completion is when all blocks' warps have no live lanes and no barrier remains pending. No progress with unfinished work is a deadlock error with block/warp state.

Representative eventual CLI:

```text
./simtlab run kernels/vector_add.asm --grid 1,1 --block 32,1
./simtlab run kernels/vector_add.asm --grid 64,1 --block 256,1 --format json
```

The eventual CLI exposes grid, block, allocations and host copies, allocation-base-to-GPR bindings, shared-memory bytes, SM configuration, cost overrides, output format, and execution limits. Exact option spellings beyond the examples are implementation details.

## 11. Errors and limitations

Parsing/execution errors include source location and, where applicable, block/warp/lane. Unknown instructions, invalid operand forms/registers/labels, unsupported operations, invalid control flow or reconvergence, address overflow, out-of-bounds access, barrier misuse/deadlock, stack overflow, resource exhaustion and execution-limit exhaustion are errors. Do not silently substitute values or skip invalid active lanes. Non-finite floats follow IEEE-754; division is absent. Overlapping writes have deterministic lane/SM order for reproducibility but are not validated as race-free. The simulator is not intended to predict real hardware performance without later calibration.

## 12. Architecture invariants

1. Warp size is exactly 32; every block contains an integer number of warps.
2. Shared memory is private to a block; global memory is shared across blocks and bounded by 4 GiB.
3. Memory addresses and GPRs are 32-bit; checked address arithmetic cannot wrap.
4. Predication intersects the active mask; retired lanes never reactivate.
5. Saved reconvergence masks exclude retired lanes; nested entries are LIFO.
6. Every remaining live warp participates in the same block barrier encounter; fully retired warps do not.
7. Stalled warps do not prevent other ready resident warps from issuing.
8. Occupancy respects every configured SM resource and block/warp limit.
9. All costs are configurable placeholders; simulation and counter aggregation are deterministic.
10. Functional memory behavior is distinct from performance accounting.
11. MMA requires uniform full-warp execution and makes no tensor-core accuracy claim.
12. Execution limits terminate unfinished simulations with diagnostics, never successful partial results.
