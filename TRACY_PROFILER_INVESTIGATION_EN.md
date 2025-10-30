# Tracy and Device Profiler Issues Investigation Report: TG and Grayskull

## Executive Summary

This document outlines the issues discovered with Tracy and Device Profiler on TG (Tenstorrent Galaxy) and Grayskull architectures in the Tenstorrent tt-metal project.

## Critical Issues Identified

### 1. Grayskull Soft Reset Issue (Critical)

**Location**: `docs/source/tt-metalium/tools/device_program_profiler.rst:131-132`

**Issue Description**:
- On Grayskull, using `tensix_reset` or `tt-smi` soft reset significantly worsens the skew between core clocks
- This makes core-to-core timing comparisons inaccurate and produces wrong results
- **Required Fix**: Full host reboot is necessary to sync core clocks after soft reset

**Impact**: High - Directly affects profiling accuracy

**Reproduction**:
- Use Grayskull device
- Perform profiling after executing `tensix_reset` or `tt-smi` soft reset

**Recommendation**:
```python
# Check before profiling on Grayskull
if arch == "grayskull" and recent_soft_reset():
    raise Warning("Full host reboot required after soft reset for accurate profiling on Grayskull")
```

### 2. test_multi_op Skipped on Grayskull

**Location**: `tests/tt_metal/tools/profiler/test_device_profiler.py:63`

**Issue Description**:
- `test_multi_op()` function is skipped with `@skip_for_grayskull()` decorator
- However, the code contains Grayskull reference counts:
  ```python
  REF_COUNT_DICT = {
      "grayskull": [108 * OP_COUNT * RUN_COUNT, 88 * OP_COUNT * RUN_COUNT],
      "wormhole_b0": [72 * OP_COUNT * RUN_COUNT, 64 * OP_COUNT * RUN_COUNT, 56 * OP_COUNT * RUN_COUNT],
  }
  ```

**Inconsistency**: 
- Test is skipped but Grayskull support code exists
- `test_device_profiler_gs_no_reset.py` re-invokes `test_multi_op()` (no-reset version)

**Analysis**:
- Grayskull excluded from regular tests due to reset issues
- No-reset specific test is supported, which correlates with the soft reset issue above

### 3. Print Support Limited to Grayskull Only

**Location**: `tt_metal/tools/profiler/process_device_log.py:81-82`

**Issue Description**:
```python
def is_print_supported(devicesData):
    return devicesData["deviceInfo"]["arch"] == "grayskull"
```

- Detailed statistics printing is only supported on Grayskull
- Other architectures (Wormhole, Galaxy, etc.) have detailed output disabled

**Impact**:
- Visual grid display of per-core, per-RISC statistics only works on Grayskull
- Other platforms only get summary statistics

**Suspected Reason**:
- Grayskull grid layout (12x12) is hardcoded
- Other architectures may have different core layouts

### 4. Architecture-Specific RISC Count Differences

**Location**: `tests/tt_metal/tools/profiler/test_device_profiler.py`

**Issue Description**:
- Grayskull and Wormhole_b0 have different core counts:
  - `test_multi_op`: Grayskull [108, 88], Wormhole_b0 [72, 64, 56]
  - `test_full_buffer`: Same pattern

**Analysis**:
- Different architectures have different worker core counts
- Profiler accounts for this with different reference values

### 5. Lack of Explicit Galaxy/TG Support

**Issue Description**:
- No explicit profiler support for Galaxy clusters found in the codebase
- Galaxy cluster type is defined in `tt_metal/llrt/tt_cluster.hpp`, but not referenced in profiler code

**Related Code**:
```cpp
// tt_metal/llrt/tt_cluster.hpp
// For TG Galaxy systems, mmio chips are gateway chips that are only used for dispatch
```

**Impact**:
- Multi-device profiling on Galaxy systems may not be properly supported
- Cross-device timestamp synchronization issues (mentioned in documentation)

### 6. Cross-Device Timestamp Synchronization Problem

**Location**: `docs/source/tt-metalium/tools/device_program_profiler.rst:134`

**Issue Description**:
> "The cycle counts from cores on different devices are usually not synced. Comparing times across devices requires this consideration."

**Impact**:
- Cross-device profiling on multi-device systems like TG/Galaxy is inaccurate
- Difficult to compare performance across devices

### 7. L1 Buffer Limitation

**Location**: `docs/source/tt-metalium/tools/device_program_profiler.rst:126`

**Issue Description**:
- Each core can only record up to 125 scopes
- L1 buffer size limitation

**Impact**:
- Limited profiling for complex operations or long programs
- Potential data loss on buffer overflow

## Architecture Support Matrix

| Feature | Grayskull | Wormhole_b0 | Galaxy/TG |
|---------|-----------|-------------|-----------|
| Device Profiler | ✅ (reset issues) | ✅ | ❓ Unverified |
| Tracy Integration | ✅ | ✅ | ❓ Unverified |
| Detailed Stats Output | ✅ | ❌ | ❌ |
| Multi-op Tests | ⚠️ (no-reset only) | ✅ | ❓ Unverified |
| Multi-Device Sync | N/A | N/A | ❌ |

## Recommended Improvements

### Immediate (Short-term)
1. **Add Grayskull soft reset warning**: Warn about reboot requirement after soft reset when profiler starts
2. **Documentation update**: Explicitly document known Galaxy/TG constraints
3. **Test coverage**: Add profiler tests on Galaxy systems

### Medium-term
1. **Extend detailed output**: Support grid visualization for Wormhole and Galaxy
2. **Cross-device synchronization**: Implement multi-device timestamp alignment mechanism
3. **L1 buffer optimization**: Increase buffer size or introduce streaming approach

### Long-term
1. **Architecture abstraction**: Remove platform-specific hardcoding, use configuration-based approach
2. **Automatic reset detection**: Detect soft reset on Grayskull and auto-warn
3. **Galaxy optimization**: Dedicated profiling mode for TG clusters

## Areas Requiring Testing

1. **Galaxy/TG Systems**:
   - Verify basic device profiler operation
   - Tracy integration testing
   - Multi-device scenarios

2. **Grayskull**:
   - Measure profiling accuracy after soft reset
   - Long-running tests in no-reset environment

3. **Cross-platform**:
   - Verify result consistency for identical tests
   - Establish architecture-specific performance baselines

## Key Files Referenced

- `tt_metal/tools/profiler/process_device_log.py` - Main profiler processing logic
- `tests/tt_metal/tools/profiler/test_device_profiler.py` - Profiler tests
- `docs/source/tt-metalium/tools/device_program_profiler.rst` - Documentation
- `docs/source/tt-metalium/tools/tracy_profiler.rst` - Tracy documentation

## Conclusions

The current profiling infrastructure works well on Grayskull and Wormhole, but has the following major issues:

1. **Inaccurate timing on Grayskull after soft reset** - Most critical issue
2. **Unclear Galaxy/TG support** - Lack of tests and documentation
3. **Platform-specific hardcoding** - Maintenance difficulties
4. **Lack of multi-device synchronization** - Constraints on large-scale systems

Addressing these issues systematically will enable stable and accurate profiling across all platforms.

## Discovered Limitations (Summary)

### Grayskull-Specific
- ❌ **Critical**: Soft reset causes clock skew, requires full host reboot
- ⚠️ Regular multi-op tests skipped (no-reset version only)
- ✅ Detailed grid statistics output supported

### Wormhole
- ✅ Full device profiler support
- ❌ No detailed grid statistics output
- ✅ All tests enabled

### Galaxy/TG
- ❓ Device profiler support unclear (needs testing)
- ❌ No explicit code for Galaxy cluster profiling
- ❌ Cross-device timestamp sync not supported
- ❌ No dedicated tests found

### Universal Issues
- ⚠️ L1 buffer limited to 125 scopes per core
- ⚠️ Cross-device timestamps not synchronized
- ⚠️ Platform-specific hardcoding throughout codebase
