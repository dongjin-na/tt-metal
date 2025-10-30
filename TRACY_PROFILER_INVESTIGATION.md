# Tracy와 Device Profiler 관련 TG 및 Grayskull 이슈 조사 보고서

## 요약 (Executive Summary)

본 문서는 Tenstorrent tt-metal 프로젝트에서 Tracy 및 Device Profiler가 TG (Tenstorrent Galaxy) 및 Grayskull 아키텍처에서 겪고 있는 문제들을 정리한 것입니다.

## 발견된 주요 이슈

### 1. Grayskull의 Soft Reset 문제 (Critical)

**위치**: `docs/source/tt-metalium/tools/device_program_profiler.rst:131-132`

**문제 설명**:
- Grayskull에서 `tensix_reset` 및 `tt-smi` soft reset을 사용하면 core clock 간 skew가 크게 악화됨
- 이로 인해 코어 간 타이밍 비교가 부정확하고 잘못된 결과를 초래함
- **해결 방법**: Soft reset 사용 후에는 전체 호스트 재부팅 필요

**영향도**: High - 프로파일링 정확도에 직접적인 영향

**재현 조건**:
- Grayskull 디바이스 사용
- `tensix_reset` 또는 `tt-smi` soft reset 실행 후 프로파일링 수행

**권장 사항**:
```python
# Grayskull에서 프로파일링 전 체크
if arch == "grayskull" and recent_soft_reset():
    raise Warning("Full host reboot required after soft reset for accurate profiling on Grayskull")
```

### 2. Grayskull에서 test_multi_op 테스트 스킵

**위치**: `tests/tt_metal/tools/profiler/test_device_profiler.py:63`

**문제 설명**:
- `test_multi_op()` 함수가 `@skip_for_grayskull()` 데코레이터로 스킵됨
- 하지만 코드에는 Grayskull 레퍼런스 카운트가 정의되어 있음:
  ```python
  REF_COUNT_DICT = {
      "grayskull": [108 * OP_COUNT * RUN_COUNT, 88 * OP_COUNT * RUN_COUNT],
      "wormhole_b0": [72 * OP_COUNT * RUN_COUNT, 64 * OP_COUNT * RUN_COUNT, 56 * OP_COUNT * RUN_COUNT],
  }
  ```

**불일치**: 
- 테스트가 스킵되지만 Grayskull 지원 코드는 존재함
- `test_device_profiler_gs_no_reset.py`에서 `test_multi_op()`를 다시 호출함 (no reset 버전)

**분석**:
- Grayskull은 reset 이슈로 인해 일반 테스트에서는 제외되었으나, no-reset 전용 테스트는 지원
- 이는 위의 soft reset 이슈와 연관됨

### 3. 프린트 지원 제한 (Grayskull만 지원)

**위치**: `tt_metal/tools/profiler/process_device_log.py:81-82`

**문제 설명**:
```python
def is_print_supported(devicesData):
    return devicesData["deviceInfo"]["arch"] == "grayskull"
```

- 상세한 통계 프린트 출력이 Grayskull에서만 지원됨
- 다른 아키텍처 (Wormhole, Galaxy 등)에서는 상세 출력 비활성화

**영향**:
- 코어별 리스크별 통계를 시각적 그리드로 표시하는 기능이 Grayskull에서만 작동
- 다른 플랫폼에서는 요약 통계만 출력

**이유 추정**:
- Grayskull 그리드 레이아웃 (12x12)이 코드에 하드코딩됨
- 다른 아키텍처는 다른 코어 레이아웃을 가질 수 있음

### 4. 아키텍처별 RISC 카운트 차이

**위치**: `tests/tt_metal/tools/profiler/test_device_profiler.py`

**문제 설명**:
- Grayskull과 Wormhole_b0가 서로 다른 코어 카운트를 가짐:
  - `test_multi_op`: Grayskull [108, 88], Wormhole_b0 [72, 64, 56]
  - `test_full_buffer`: 동일한 패턴

**분석**:
- 아키텍처마다 다른 워커 코어 수를 가짐
- 프로파일러가 이를 고려하여 레퍼런스 값을 다르게 설정

### 5. Galaxy/TG 특정 지원 부족

**문제 설명**:
- Galaxy 클러스터에 대한 명시적 프로파일러 지원이 코드에서 발견되지 않음
- `tt_metal/llrt/tt_cluster.hpp`에서 Galaxy 클러스터 타입은 정의되어 있으나, 프로파일러 코드에서는 참조되지 않음

**관련 코드**:
```cpp
// tt_metal/llrt/tt_cluster.hpp
// For TG Galaxy systems, mmio chips are gateway chips that are only used for dispatch
```

**영향**:
- Galaxy 시스템의 멀티 디바이스 프로파일링이 제대로 지원되지 않을 가능성
- 디바이스 간 타임스탬프 동기화 이슈 (문서에 언급됨)

### 6. 디바이스 간 타임스탬프 동기화 문제

**위치**: `docs/source/tt-metalium/tools/device_program_profiler.rst:134`

**문제 설명**:
> "The cycle counts from cores on different devices are usually not synced. Comparing times across devices requires this consideration."

**영향**:
- TG/Galaxy와 같은 멀티 디바이스 시스템에서 크로스 디바이스 프로파일링이 부정확
- 디바이스 간 성능 비교가 어려움

### 7. L1 버퍼 제한

**위치**: `docs/source/tt-metalium/tools/device_program_profiler.rst:126`

**문제 설명**:
- 각 코어는 최대 125개 스코프만 기록 가능
- L1 버퍼 크기 제한

**영향**:
- 복잡한 연산이나 긴 프로그램의 프로파일링 제한
- 버퍼 오버플로우 시 데이터 손실 가능

## 아키텍처별 상태 요약

| 기능 | Grayskull | Wormhole_b0 | Galaxy/TG |
|------|-----------|-------------|-----------|
| Device Profiler | ✅ (reset 이슈 있음) | ✅ | ❓ 미확인 |
| Tracy 통합 | ✅ | ✅ | ❓ 미확인 |
| 상세 통계 출력 | ✅ | ❌ | ❌ |
| Multi-op 테스트 | ⚠️ (no-reset만) | ✅ | ❓ 미확인 |
| 멀티 디바이스 동기화 | N/A | N/A | ❌ |

## 권장 개선 사항

### 단기 (Immediate)
1. **Grayskull soft reset 경고 추가**: 프로파일러 시작 시 soft reset 후 재부팅 필요성 경고
2. **문서 업데이트**: Galaxy/TG 관련 알려진 제약사항 명시
3. **테스트 커버리지**: Galaxy 시스템에서 프로파일러 테스트 추가

### 중기 (Medium-term)
1. **상세 출력 확장**: Wormhole 및 Galaxy를 위한 그리드 시각화 지원
2. **디바이스 간 동기화**: 멀티 디바이스 타임스탬프 정렬 메커니즘 구현
3. **L1 버퍼 최적화**: 버퍼 크기 증가 또는 스트리밍 방식 도입

### 장기 (Long-term)
1. **아키텍처 추상화**: 플랫폼별 하드코딩 제거, 설정 기반 접근
2. **자동 리셋 감지**: Grayskull에서 soft reset 감지 및 자동 경고
3. **Galaxy 최적화**: TG 클러스터를 위한 전용 프로파일링 모드

## 테스트 필요 영역

1. **Galaxy/TG 시스템**:
   - 기본 device profiler 작동 확인
   - Tracy 통합 테스트
   - 멀티 디바이스 시나리오

2. **Grayskull**:
   - Soft reset 후 프로파일링 정확도 측정
   - No-reset 환경에서 장기 실행 테스트

3. **크로스 플랫폼**:
   - 동일 테스트의 결과 일관성 검증
   - 아키텍처별 성능 기준선 설정

## 참고 파일

- `tt_metal/tools/profiler/process_device_log.py` - 주요 프로파일러 처리 로직
- `tests/tt_metal/tools/profiler/test_device_profiler.py` - 프로파일러 테스트
- `docs/source/tt-metalium/tools/device_program_profiler.rst` - 문서
- `docs/source/tt-metalium/tools/tracy_profiler.rst` - Tracy 문서

## 결론

현재 프로파일링 인프라는 Grayskull과 Wormhole에서 잘 작동하지만, 다음과 같은 주요 이슈가 있습니다:

1. **Grayskull의 soft reset 후 부정확한 타이밍** - 가장 심각한 이슈
2. **Galaxy/TG 지원 불명확** - 테스트 및 문서 부족
3. **플랫폼별 하드코딩** - 유지보수 어려움
4. **멀티 디바이스 동기화 미지원** - 대규모 시스템에서 제약

이러한 이슈들을 단계적으로 해결하면 모든 플랫폼에서 안정적이고 정확한 프로파일링이 가능할 것입니다.
