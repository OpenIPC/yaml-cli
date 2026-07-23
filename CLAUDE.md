# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`yaml-cli` is a tiny C command-line tool for getting/setting/deleting a single value in a YAML
file addressed by a dotted path (e.g. `.video0.codec`). It ships in OpenIPC camera firmware to edit
`majestic`'s config (default input file: `/etc/majestic.yaml`).

`libyaml/` is a vendored copy of the upstream library, built as a CMake subproject. Project code
lives entirely in `src/`.

## Build & run

```bash
cmake -B build
cmake --build build -j
# binary: build/yaml-cli

# Debug build adds -ggdb -O0 and AddressSanitizer (see CMakeLists.txt):
cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dbg -j
```

The top-level `CMakeLists.txt` sets `CMAKE_POLICY_VERSION_MINIMUM` to 3.5 so the vendored
`libyaml` (which declares `cmake_minimum_required(VERSION 3.0)`) still configures under CMake >= 4.
Without that shim, configure fails with "Compatibility with CMake < 3.5 has been removed".

```bash
./build/yaml-cli -i file.yaml -g .video0.codec        # get -> prints value, exit 0 if found
./build/yaml-cli -i file.yaml -s .video0.codec h265   # set (in-place: -o defaults to input path)
./build/yaml-cli -i file.yaml -d .video0.codec        # delete key
# -o <file> writes elsewhere; -c canonical, -u unicode; -h help
```

Paths are leading-dot dotted: `.block.subblock.key`. A numeric component indexes
a **sequence** element when the node at that point is a list — `-g .outgoing.0`
reads the first item, `-g .outgoing` prints the whole list one item per line,
`-s .outgoing.N` replaces item N (or appends when N equals the length), and
`-d .outgoing.N` removes it. Against a mapping a numeric component is an ordinary
key, so a key literally named `0` still works.

## Tests

Black-box tests drive the built binary — there are no unit tests (the `libyaml/tests/` belong to the
upstream library and are excluded from our `ctest` via `BUILD_TESTING OFF`).

```bash
cmake -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
```

`tests/run.sh` is a dependency-free POSIX-sh runner covering get/set/del, output-file redirection,
key insertion, empty-parent pruning (incl. recursive), sequence element get/set/append/delete, and
the error paths (missing key/file, unknown option, no mode). Each case is registered as its own CTest test in `CMakeLists.txt` (keep the
`YAML_CLI_TEST_CASES` list in sync with the `case_*` functions in the script). Run one case directly
with `sh tests/run.sh ./build/yaml-cli set_new_key`.

Build in **Debug** to run the suite under AddressSanitizer/LeakSanitizer — do this after any change
to the document handling in `main.c` or `yaml_doc.c`:

```bash
cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug && cmake --build build-dbg -j
ctest --test-dir build-dbg --output-on-failure
```

CI (`.github/workflows/ci.yml`) runs both Release and Debug matrices on every push and PR.

## Formatting

Two configs, both 4-space indent: `.clang-format` (`clang-format`) and `yaml-cli.astylerc`
(`astyle`, Google style, 80-col). Match existing style; don't reformat unrelated lines.

## Architecture: load → operate on the DOM → dump

The tool uses libyaml's high-level **document (DOM) API**, not the low-level event stream. `main.c`
loads the whole file into a `yaml_document_t` tree, `yaml_doc.c` performs the get/set/del on that
tree by walking the dotted path, and (for set/del) the modified tree is dumped back out. Flow:

```
input file -> yaml_parser_load -> yaml_document_t
           -> yaml_doc_get / yaml_doc_set / yaml_doc_del   (src/yaml_doc.c)
           -> yaml_emitter_dump -> output file             (set/del only; -o defaults to input path)
```

Modules (`src/`):
- **input_data.c** — manual argv parsing into a global `prog_args_t` (`get_args()`), input-file open,
  and `open_output_file()`. The input is closed right after loading so the output may reuse its path
  for an in-place edit. Output path defaults to the input path.
- **yaml_doc.c** — all document manipulation, addressed by a leading-dot dotted path. `get` prints
  the scalar leaf; `set` walks/creates intermediate mappings and replaces or appends the final
  scalar; `del` removes the target pair and then prunes any parent mapping the removal left empty
  (recursively, never the root).
- **main.c** — parser/emitter lifecycle, error reporting, and mode dispatch.

### Things that will bite you in `yaml_doc.c`

- **Nodes are referenced by 1-based integer id, and the node array reallocates.** Any
  `yaml_document_add_*` call can invalidate every `yaml_node_t *` you hold. Navigate and mutate by
  node id, and *re-fetch* a node pointer with `yaml_document_get_node()` after every add. The root is
  always id `1` (`ROOT_NODE_ID`) when the document is non-empty.
- **Deletion detaches, it does not free.** Removing a pair from a mapping just shifts the pairs array;
  the orphaned key/value nodes stay in the document's node array and are freed together with the
  whole document (by `yaml_emitter_dump`, or `yaml_document_delete` on the GET path). Don't free them
  by hand — that would double-free.
- **`yaml_emitter_dump` destroys the document** (even on failure). `emit_document()` therefore owns
  the document; the GET path, which never dumps, deletes it itself. Delete exactly once.
- **Format fidelity depends on the str tag.** libyaml's loader tags untagged plain scalars with the
  default str tag, and the dumper emits a str-tagged scalar plainly/unquoted — so a DOM round-trip
  keeps values like `80` and `true` unquoted. New scalars are added with a `NULL` tag (which defaults
  to str) to inherit the same behaviour.
