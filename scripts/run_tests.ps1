
param(
    [switch]$Gpu
)

$ErrorActionPreference = "Continue"
$rootDir = Split-Path -Parent $PSScriptRoot

# headless tests need no gpu
$headlessTests = @(
    "test_pal_filesystem",
    "test_pal_haptics",
    "test_pal_init",
    "test_pal_input",
    "test_pal_integration",
    "test_pal_network",
    "test_pal_platform",
    "test_pal_time",
    "test_physics",
    "test_math",
    "test_network",
    "test_shared_types",
    "test_octree",
    "test_frustum",
    "test_render_settings",
    "test_skeletal_math",
    "test_scene_node",
    "test_ecs",
    "test_message_bus",
    "test_binary_stream",
    "test_task_system",
    "test_allocators",
    "test_service_manager",
    "test_camera",
    "test_particles_cpu",
    "test_terrain_cpu",
    "test_resource_manager",
    "test_animation_cpu",
    "test_shader_registry",
    "test_framework",
    "test_core",
    "test_integration_ecs_scene",
    "test_integration_physics_scene",
    "test_integration_services",
    "test_integration_scene_render",
    "test_shader_compile_all"
)

# gpu tests need a display and gpu
$gpuTests = @(
    "test_renderer",
    "test_rhi_core",
    "test_rhi_buffers",
    "test_rhi_textures",
    "test_rhi_shaders",
    "test_rhi_pipeline",
    "test_rhi_framebuffer",
    "test_material",
    "test_mesh_model",
    "test_animation",
    "test_nif_bridge",
    "test_visual_regression",
    "test_postfx",
    "test_modern_rendering"
)

$passed = 0
$failed = 0
$skipped = 0
$failList = @()

function Run-TestList {
    param(
        [string]$Label,
        [string[]]$Tests
    )

    Write-Host "=== $Label ===" -ForegroundColor Cyan
    foreach ($test in $Tests) {
        # looks in release tests first then release
        $exe = $null
        if (Test-Path (Join-Path $rootDir "release/tests/$test.exe")) {
            $exe = Join-Path $rootDir "release/tests/$test.exe"
        } elseif (Test-Path (Join-Path $rootDir "release/$test.exe")) {
            $exe = Join-Path $rootDir "release/$test.exe"
        }

        if (-not $exe) {
            Write-Host "  SKIP $test (not found)" -ForegroundColor Yellow
            $script:skipped++
            continue
        }

        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $proc = Start-Process -FilePath $exe -ArgumentList "--auto" -NoNewWindow -Wait -PassThru -ErrorAction SilentlyContinue
        $sw.Stop()
        $elapsed = [math]::Round($sw.Elapsed.TotalSeconds, 2)

        if ($proc.ExitCode -eq 0) {
            Write-Host "  PASS $test (${elapsed}s)" -ForegroundColor Green
            $script:passed++
        } else {
            Write-Host "  FAIL $test (exit code: $($proc.ExitCode), ${elapsed}s)" -ForegroundColor Red
            $script:failed++
            $script:failList += $test
        }
    }
    Write-Host ""
}

Run-TestList -Label "Headless Tests" -Tests $headlessTests

if ($Gpu) {
    Run-TestList -Label "GPU Tests" -Tests $gpuTests
} else {
    Write-Host "Skipping GPU tests (use -Gpu to include them)" -ForegroundColor Yellow
    Write-Host ""
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Results: $passed passed, $failed failed, $skipped skipped" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if ($failed -gt 0) {
    Write-Host ""
    Write-Host "Failed tests:" -ForegroundColor Red
    foreach ($t in $failList) {
        Write-Host "  - $t" -ForegroundColor Red
    }
    exit 1
}
