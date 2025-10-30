# Tracy and Device Profiler Issue Investigation

This directory contains investigation reports for Tracy and Device Profiler issues on various Tenstorrent hardware platforms.

## Reports Available

### [Korean Report (TRACY_PROFILER_INVESTIGATION.md)](./TRACY_PROFILER_INVESTIGATION.md)
Tracy와 Device Profiler의 TG 및 Grayskull 아키텍처 관련 이슈를 조사한 한국어 보고서입니다.

### [English Report (TRACY_PROFILER_INVESTIGATION_EN.md)](./TRACY_PROFILER_INVESTIGATION_EN.md)
Comprehensive English investigation report covering Tracy and Device Profiler issues on TG and Grayskull architectures.

## Key Findings Summary

### Critical Issues Discovered

1. **Grayskull Soft Reset Problem** (🔴 Critical)
   - Soft resets cause significant clock skew between cores
   - Requires full host reboot for accurate profiling
   - Location: Device Program Profiler documentation

2. **Limited Galaxy/TG Support** (⚠️ High Priority)
   - No explicit profiler code for Galaxy clusters
   - Cross-device timestamp synchronization not implemented
   - Needs testing and verification

3. **Platform-Specific Limitations**
   - Detailed statistics output only on Grayskull
   - Different core counts per architecture
   - Hardcoded platform assumptions

4. **L1 Buffer Constraints**
   - 125 scope limit per core
   - May cause data loss in complex profiling scenarios

### Architecture Support Matrix

| Platform | Status | Notes |
|----------|--------|-------|
| Grayskull | ⚠️ Partial | Reset issues, detailed stats supported |
| Wormhole_b0 | ✅ Good | Full support, no detailed grid stats |
| Galaxy/TG | ❓ Unknown | Needs testing and verification |

## Recommended Next Steps

1. **Immediate**: Add warnings for Grayskull soft reset scenarios
2. **Short-term**: Test profiler on Galaxy/TG systems
3. **Medium-term**: Implement cross-device synchronization
4. **Long-term**: Refactor platform-specific hardcoding

## Related Documentation

- [Device Program Profiler](docs/source/tt-metalium/tools/device_program_profiler.rst)
- [Tracy Profiler](docs/source/tt-metalium/tools/tracy_profiler.rst)
- [Profiler Tests](tests/tt_metal/tools/profiler/)

## For Developers

If you're working on profiler features, please:
- Review these investigation reports before making changes
- Test on multiple architectures when possible
- Update documentation for architecture-specific behaviors
- Add tests for new platforms

## Questions or Updates?

If you discover new issues or have updates to these findings, please:
1. Update the relevant investigation report
2. Add test cases to verify the behavior
3. Document workarounds or fixes in the main documentation
