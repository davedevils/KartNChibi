// tinyobj_loader.h - v2.0.0 RC10 - Tiny but powerful single file OBJ loader
// NOTE: This is a STUB for KnC Engine development.
// For production, download the full tinyobjloader from:
// https://github.com/tinyobjloader/tinyobjloader
//
// MIT License - Copyright (c) 2012-2021 Syoyo Fujita
//
// This stub provides basic types and declarations for compilation.

#ifndef TINY_OBJ_LOADER_H_
#define TINY_OBJ_LOADER_H_

#include <string>
#include <vector>
#include <map>

namespace tinyobj {

typedef struct {
    std::string name;
    std::vector<float> ambient;
    std::vector<float> diffuse;
    std::vector<float> specular;
    std::string ambient_texname;
    std::string diffuse_texname;
    std::string specular_texname;
} material_t;

typedef struct {
    std::string name;
    std::vector<int> intValues;
    std::vector<float> floatValues;
    std::vector<std::string> stringValues;
} tag_t;

typedef struct {
    std::vector<int> indices;
    std::vector<unsigned char> num_face_vertices;
    std::vector<int> material_ids;
    std::vector<unsigned int> smoothing_group_ids;
    std::vector<tag_t> tags;
} mesh_t;

typedef struct {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<float> colors;
} attrib_t;

typedef struct {
    std::string name;
    mesh_t mesh;
} shape_t;

// Load OBJ file
bool LoadObj(attrib_t *attrib, std::vector<shape_t> *shapes,
    std::vector<material_t> *materials, std::string *warn, std::string *err,
    const char *filename, const char *mtl_basedir = nullptr,
    bool triangulate = true);

} // namespace tinyobj

#endif // TINY_OBJ_LOADER_H_

