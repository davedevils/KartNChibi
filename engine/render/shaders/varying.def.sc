vec4 v_color0    : COLOR0    = vec4(1.0, 1.0, 1.0, 1.0);
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
float v_fogdepth : TEXCOORD1 = 0.0;
float v_toonlight : TEXCOORD2 = 1.0;

vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec4 a_color0    : COLOR0;
vec2 a_texcoord0 : TEXCOORD0;
// Skinned meshes only palette slots move the vertex slots arrive as unnormalised bytes shader casts to int
vec4 a_indices   : BLENDINDICES;
vec4 a_weight    : BLENDWEIGHT;
