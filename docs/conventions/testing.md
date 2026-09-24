# Testing Conventions

## Structure

```
tests/
    unit/           CPU-only, fast, no GPU
    integration/    May need GPU, cross-module
    visual/         GPU required, screenshot comparison
    golden/         Reference images (versioned)
    framework/      KnCTest.h, fixtures, helpers
    stress/         Performance, memory, load
    output/         Results XML + screenshots (gitignored)
```

## Categories

| Type | GPU? | Frequency | Max time |
|------|------|-----------|----------|
| Unit | No | Every commit | < 30 sec |
| Integration | Maybe | Every PR | < 2 min |
| Shader compile | No | PR (if shaders touched) | < 1 min |
| Visual | Yes | PR (if renderer/shaders touched) | < 5 min |
| Backend parity | Yes (3x) | Nightly | < 15 min |
| Stress | Yes | Weekly | < 30 min |

## Coverage Target

- Global: >= 98%
- Per modified module: >= 95%
- Measure: OpenCppCoverage (Windows) or llvm-cov

## Merge Gates

| Gate | Blocks merge? |
|------|--------------|
| Build 0 errors | YES |
| Unit tests 100% pass | YES |
| Integration tests 100% pass | YES |
| Shader compilation (3 targets) | YES |
| Coverage >= 98% | YES |
| Visual golden match (< 1% RMSE) | YES (if renderer touched) |
| Backend parity | YES (if RHI touched) |

## Golden Images

- Stored in `tests/golden/`
- Per backend: `dx12/`, `vulkan/`, `webgpu/`
- RMSE tolerance: < 1.0/255 (DX12<->Vulkan), < 3.0/255 (<->WebGPU)
- Pixel mismatch: < 0.5%

## Naming

- Test file: `test_<module>.cpp`
- Test function: `test_<what>()` or `KNC_TEST(name, backends)`
- Golden image: `tests/golden/<scene>/<backend>/capture.png`
