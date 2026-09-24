// Bone palette binds 3 rows not 4x4 24 bones at 3 rows is 72 under GLSL ES 128 vectors
uniform vec4 u_bones[72];

vec3 bone_moved(int slot, vec4 padded)
{
	int row = slot * 3;
	return vec3(dot(u_bones[row], padded), dot(u_bones[row + 1], padded),
	            dot(u_bones[row + 2], padded));
}

// Slots arrive normalised no int attribute D3D11 won't feed UINT to float register times 255 plus half restores index
vec3 skin_blend(vec4 slots, vec4 weights, vec4 padded)
{
	vec4 index = slots * 255.0 + 0.5;
	return weights.x * bone_moved(int(index.x), padded) +
	       weights.y * bone_moved(int(index.y), padded) +
	       weights.z * bone_moved(int(index.z), padded) +
	       weights.w * bone_moved(int(index.w), padded);
}
