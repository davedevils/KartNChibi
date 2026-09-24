# Gamebryo asset readers NIF KF KFM and the glTF export no device plain Cpp17
add_library(KnCFormats STATIC
    gltf_writer.cpp
    kfm_reader.cpp
    nif_animation_eval.cpp
    nif_components.cpp
    nif_controllers.cpp
    nif_edit.cpp
    nif_effect.cpp
    nif_encode.cpp
    nif_extra_data.cpp
    nif_geometry_data.cpp
    nif_gltf.cpp
    nif_gltf_animation.cpp
    nif_keys.cpp
    nif_kf_remainder.cpp
    nif_misc_objects.cpp
    nif_node_variants.cpp
    nif_particles.cpp
    nif_physx.cpp
    nif_properties.cpp
    nif_psys_controllers.cpp
    nif_psys_modifiers.cpp
    nif_reader.cpp
    nif_scene_graph.cpp
    nif_scene_objects.cpp
    nif_sequences.cpp
    nif_skeleton.cpp
    nif_type_hashes.cpp
    nif_writer.cpp
)
set_target_properties(KnCFormats PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_include_directories(KnCFormats PUBLIC ${CMAKE_SOURCE_DIR})
