#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

RUN_GPU=false
for arg in "$@"; do
  case "$arg" in
    --gpu) RUN_GPU=true ;;
    *) echo "Unknown flag: $arg"; exit 1 ;;
  esac
done

# headless tests that need no GPU
HEADLESS_TESTS=(
  test_pal_filesystem
  test_pal_haptics
  test_pal_init
  test_pal_input
  test_pal_integration
  test_pal_network
  test_pal_platform
  test_pal_time
  test_physics
  test_math
  test_network
  test_shared_types
  test_octree
  test_frustum
  test_render_settings
  test_skeletal_math
  test_scene_node
  test_ecs
  test_message_bus
  test_binary_stream
  test_task_system
  test_allocators
  test_service_manager
  test_camera
  test_particles_cpu
  test_terrain_cpu
  test_resource_manager
  test_animation_cpu
  test_shader_registry
  test_framework
  test_core
  test_integration_ecs_scene
  test_integration_physics_scene
  test_integration_services
  test_integration_scene_render
  test_shader_compile_all
)

# GPU tests that need a display and GPU
GPU_TESTS=(
  test_renderer
  test_rhi_core
  test_rhi_buffers
  test_rhi_textures
  test_rhi_shaders
  test_rhi_pipeline
  test_rhi_framebuffer
  test_material
  test_mesh_model
  test_animation
  test_nif_bridge
  test_visual_regression
  test_postfx
  test_modern_rendering
)

passed=0
failed=0
skipped=0
fail_list=()

run_test_list() {
  local label="$1"
  shift
  local tests=("$@")

  echo -e "${CYAN}=== $label ===${NC}"
  for test in "${tests[@]}"; do
    # look in release tests first then release
    exe=""
    if [[ -f "$ROOT_DIR/release/tests/$test.exe" ]]; then
      exe="$ROOT_DIR/release/tests/$test.exe"
    elif [[ -f "$ROOT_DIR/release/$test.exe" ]]; then
      exe="$ROOT_DIR/release/$test.exe"
    elif [[ -f "$ROOT_DIR/release/tests/$test" ]]; then
      exe="$ROOT_DIR/release/tests/$test"
    elif [[ -f "$ROOT_DIR/release/$test" ]]; then
      exe="$ROOT_DIR/release/$test"
    fi

    if [[ -z "$exe" ]]; then
      echo -e "  ${YELLOW}SKIP${NC} $test (not found)"
      ((skipped++))
      continue
    fi

    if "$exe" --auto > /dev/null 2>&1; then
      echo -e "  ${GREEN}PASS${NC} $test"
      ((passed++))
    else
      echo -e "  ${RED}FAIL${NC} $test (exit code: $?)"
      ((failed++))
      fail_list+=("$test")
    fi
  done
  echo ""
}

run_test_list "Headless Tests" "${HEADLESS_TESTS[@]}"

if $RUN_GPU; then
  run_test_list "GPU Tests" "${GPU_TESTS[@]}"
else
  echo -e "${YELLOW}Skipping GPU tests (use --gpu to include them)${NC}"
  echo ""
fi

echo "========================================"
echo -e "Results: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}, ${YELLOW}$skipped skipped${NC}"
echo "========================================"

if [[ $failed -gt 0 ]]; then
  echo ""
  echo -e "${RED}Failed tests:${NC}"
  for t in "${fail_list[@]}"; do
    echo "  - $t"
  done
  exit 1
fi
