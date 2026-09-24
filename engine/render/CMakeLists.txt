# bgfx scene renderer needs CXX20 designated initializers shaderc compiles shaders once via scripts build shaderc ps1

find_program(KNC_SHADERC
    NAMES shaderc
    HINTS ${CMAKE_SOURCE_DIR}/build-bgfx-tools/cmake/bgfx/Release
          ${CMAKE_SOURCE_DIR}/build-bgfx-tools/cmake/bgfx
    DOC "bgfx shader compiler, one run per backend profile"
)
if(NOT KNC_SHADERC)
    message(WARNING "shaderc not found, KnCRender is skipped. Run scripts/build/build_shaderc.ps1 then configure again")
    return()
endif()

set(knc_bgfx_root ${CMAKE_SOURCE_DIR}/thirdparty/bgfx)

# profile suffix for embedded shader header then shaderc platform every BGFX EMBEDDED SHADER suffix on this host must appear here
set(knc_shader_profiles
    "120|glsl|windows"
    "100_es|essl|windows"
    "spirv|spv|linux"
    "metal|mtl|osx"
)
if(WIN32)
    list(APPEND knc_shader_profiles "s_5_0|dxbc|windows" "s_6_0|dxil|windows" "wgsl|wgsl|windows")
endif()

# Compiles one shader to every profile and writes the header that gathers them
function(knc_embed_shader stem shader_type out_blobs)
    set(shader_dir ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
    set(source ${shader_dir}/${stem}.sc)
    set(varying ${shader_dir}/varying.def.sc)
    set(shared ${shader_dir}/shade.sh ${shader_dir}/shading.sh ${shader_dir}/skinning.sh)
    set(root ${CMAKE_CURRENT_BINARY_DIR}/generated/engine/render/shaders)
    set(blobs "")
    set(includes "")
    foreach(entry ${knc_shader_profiles})
        string(REPLACE "|" ";" fields ${entry})
        list(GET fields 0 profile)
        list(GET fields 1 suffix)
        list(GET fields 2 platform)
        set(blob ${root}/${suffix}/${stem}.bin.h)
        add_custom_command(
            OUTPUT ${blob}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${root}/${suffix}
            COMMAND ${KNC_SHADERC} -f ${source} -o ${blob} --type ${shader_type}
                    --platform ${platform} --profile ${profile} --varyingdef ${varying}
                    -i ${knc_bgfx_root}/bgfx/src -i ${shader_dir} --bin2c ${stem}_${suffix}
            MAIN_DEPENDENCY ${source}
            DEPENDS ${varying} ${shared}
            COMMENT "shaderc ${stem} ${profile}"
        )
        list(APPEND blobs ${blob})
        string(APPEND includes "#include \"${suffix}/${stem}.bin.h\"\n")
    endforeach()
    file(WRITE ${root}/${stem}.bin.h "${includes}")
    set(${out_blobs} ${blobs} PARENT_SCOPE)
endfunction()

set(knc_shader_blobs "")
foreach(stem vs_scene vs_skinned vs_toon vs_outline vs_skinned_toon vs_skinned_outline vs_fullscreen)
    knc_embed_shader(${stem} vertex blobs)
    list(APPEND knc_shader_blobs ${blobs})
endforeach()
foreach(stem fs_scene fs_toon fs_outline fs_brightpass fs_blur fs_copy)
    knc_embed_shader(${stem} fragment blobs)
    list(APPEND knc_shader_blobs ${blobs})
endforeach()

add_library(KnCRender STATIC
    day_night.cpp
    glow_pass.cpp
    light_rig.cpp
    model_animation.cpp
    nif_character_model.cpp
    nif_prop_model.cpp
    particle_simulation.cpp
    png_screenshot.cpp
    scene_particles.cpp
    scene_renderer.cpp
    skinned_model.cpp
    texture_cache.cpp
    toon_program.cpp
    ${knc_shader_blobs}
)
set_target_properties(KnCRender PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(KnCRender PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(KnCRender PUBLIC KnCFormats bgfx bimg bimg_decode bx)
if(NOT WIN32)
    # Nothing off Windows compiles DXBC DXIL or WGSL the embedded table must not name them
    target_compile_definitions(KnCRender PUBLIC
        BGFX_PLATFORM_SUPPORTS_DXBC=0
        BGFX_PLATFORM_SUPPORTS_DXIL=0
        BGFX_PLATFORM_SUPPORTS_WGSL=0
    )
endif()
