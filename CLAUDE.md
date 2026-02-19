# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
make                    # build all kernels + converter
CXX=g++-13 make        # override compiler
SERIAL=1 make          # build without OpenMP (serial)
make test              # run full test suite (build, generate, load, verify)
make clean             # remove all binaries and test output
```

Run a single kernel on a small synthetic graph:

```bash
./bfs -g 10 -n 1       # BFS on 2^10-vertex Kronecker graph, 1 trial
./bfs -h               # show all flags for any kernel
```

Run only the verification step for one kernel:

```bash
make test-verify-bfs-g10
```

Run only the graph loading tests:

```bash
make test-load
```

## Architecture

All source lives in `src/`. Each kernel is a single `.cc` file that compiles to a standalone binary. The shared infrastructure is entirely header-only.

### Header infrastructure (src/*.h)

| Header | Role |
|---|---|
| `graph.h` | `CSRGraph<NodeID_, DestID_, MakeInverse>` - the core graph container in CSR format. Weighted graphs use `DestID_ = NodeWeight<NodeID_, WeightT_>`. |
| `builder.h` | `BuilderBase` - constructs a `CSRGraph` from an edge list (from file or generator). |
| `generator.h` | Synthetic graph generators: Kronecker (Graph500 RMAT) and uniform random. |
| `reader.h` | Reads graph files: `.el`, `.wel`, `.gr`, `.graph`, `.mtx`, `.sg`, `.wsg`. |
| `writer.h` | Writes serialized `.sg`/`.wsg` graph files. |
| `benchmark.h` | Convenience typedefs (`Graph`, `WGraph`, `Builder`, etc.), `SourcePicker`, `BenchmarkKernel()` harness. |
| `command_line.h` | `CLBase` and subclasses (`CLApp`, `CLPageRank`, etc.) for argument parsing via inheritance. |
| `pvector.h` | `pvector<T>` - like `std::vector` but uninitialized on resize and parallel-fill capable. |
| `sliding_queue.h` | Thread-safe sliding queue used by BFS frontier. |
| `bitmap.h` | Atomic bitmap used by BFS bottom-up step. |
| `platform_atomics.h` | Portable compare-and-swap / fetch-and-add wrappers. |
| `timer.h` | Simple wall-clock timer. |
| `util.h` | `Range<T>`, `UniDist`, `PrintTime`, `PrintLabel`, etc. |

### Kernel binaries (src/*.cc)

| Binary | Algorithm |
|---|---|
| `bfs` | Direction-optimizing BFS (top-down + bottom-up with alpha/beta switching) |
| `sssp` | Delta-stepping SSSP |
| `pr` | PageRank - iterative pull (Gauss-Seidel and Jacobi variants) |
| `pr_spmv` | PageRank via sparse matrix-vector multiply |
| `cc` | Connected components - Afforest |
| `cc_sv` | Connected components - Shiloach-Vishkin |
| `bc` | Betweenness centrality - Brandes |
| `tc` | Triangle counting - order-invariant with optional degree relabeling |
| `converter` | Converts between graph formats / builds `.sg`/`.wsg` files |

### Type conventions

- `NodeID` = `int32_t`, `WeightT` = `int32_t` (defined in `benchmark.h`)
- Unweighted graph: `CSRGraph<NodeID>` (i.e., `Graph`)
- Weighted graph: `CSRGraph<NodeID, NodeWeight<NodeID, WeightT>>` (i.e., `WGraph`)
- `MakeInverse=true` (default) stores both out- and in-adjacency; set `false` to save memory when in-edges are unneeded.

### Adding a new kernel

1. Create `src/mykern.cc` - include `benchmark.h`, parse args via a `CLApp` (or appropriate `CL*` subclass), build the graph with `Builder`, implement the kernel, call `BenchmarkKernel()`.
2. Add `mykern` to `KERNELS` in `Makefile` - the pattern rule `% : src/%.cc src/*.h` handles compilation automatically.
3. The kernel's verification function should print `"Verification:           PASS"` / `"FAIL"` so the test framework can detect it.

## Testing

Tests live in `test/test.mk` (included by root `Makefile`). Small reference graphs are in `test/graphs/` and expected output strings in `test/reference/`. Test output files are written to `test/out/` (created on demand).

`TEST_GRAPH ?= g10` controls which graph is used for verification tests; override to use a different graph:

```bash
make test-verify TEST_GRAPH=u10
```
