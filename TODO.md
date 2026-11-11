# TODO

## Core modules

- [x] Profiler
- [x] Generational Handles System
- [x] Events System
- [x] Arena Allocator
- [x] Pool Allocator
- [x] Free-List Allocator
- [x] Job System
- [x] I/O Utils
- [x] Binary/JSON Helpers
- [x] UUID

### P0

- [ ] Result / error categories (`result.hpp`), `WCORE_TRY`, no exceptions across APIs
- [ ] Reflection (`reflection.hpp`): type registry, properties, enums, flags, attributes
- [ ] Reflection-driven serialization (`serialize.hpp`): JSON/binary archives, version headers, migration hooks
- [ ] Geometry (`geometry.hpp`): AABB, ray, plane, sphere, frustum, transform, color, rect, intersections

### P1

- [ ] Hashing (`hash.hpp`): stable FNV/xxHash/CRC, type IDs, asset hashes
- [ ] String interning (`name.hpp`): `NameID` for fast property and asset keys
- [ ] Config (`config.hpp`): JSON-backed settings with defaults and overrides
- [ ] Command line (`command_line.hpp`): portable argument parsing for apps/tools
- [ ] Random (`random.hpp`): seedable PCG RNG, deterministic distributions
- [ ] String utilities (`string.hpp`): UTF-8 helpers, trim/split/join, case conversion, no locale dependence
- [ ] Flags (`flags.hpp`): type-safe enum bit flags
- [ ] Version (`version.hpp`): semantic version type and compatibility checks

### P2

- [ ] Memory tracking (`memory_tracking.hpp`): allocation counters/tags for debug builds
- [ ] Small vector / ring buffer: hot-path containers, only if profiling justifies them
- [ ] Platform detection (`platform.hpp`): compile-time OS/compiler/architecture detection only, no OS calls

### `handle.hpp`

- [ ] Add convenience type tags/aliases
- [ ] Document generation wrap and invalid-handle behavior
- [ ] Keep handles 64-bit and POD

### `job_system.hpp`

- [ ] Explicit job dependencies/continuations for asset dependency graphs
- [ ] Main-thread completion queue

### `event.hpp`

- [ ] Deferred/queued dispatch mode for gameplay and editor events

### `io.hpp`

- [ ] Atomic write (temp file + rename)
- [ ] UTF-8 path conversion helpers
- [ ] Relative/canonical normalization
- [ ] Extension utilities

### `json.hpp`

- [ ] Stable object ordering guarantees for serialization
- [ ] Option to preserve unknown fields for forward compatibility

### `binary.hpp`

- [ ] Versioned container header and alignment helpers
- [ ] Never serialize raw structs with padding

### `logger.hpp`

- [ ] Categories/channels
- [ ] Ring-buffer sink for the editor console
- [ ] Callback sink

### `profiler.hpp`

- [ ] Counters/plots
- [ ] Correlate CPU scopes with renderer GPU timestamps

### `math.hpp`

- [ ] Keep `glm` as the vocabulary; put engine geometry and color types in separate headers (`geometry.hpp`) instead of growing the alias header

### `test.hpp`

- [ ] Temporary directories and asset fixtures for integration tests
