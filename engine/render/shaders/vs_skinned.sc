$input a_position, a_normal, a_color0, a_texcoord0, a_indices, a_weight
$output v_color0, v_texcoord0, v_fogdepth, v_envcoord

#include <bgfx_shader.sh>
#include "skinning.sh"
#include "shade.sh"

// rgb is the hours ambient colour the device holds as D3DRS AMBIENT
uniform vec4 u_ambientColour;
// z 1 measures the fog distance radially which is what RANGEFOGENABLE does
uniform vec4 u_fogRange;

void main()
{
	// Palette is in model space so placement arrives as the model transform one pose serves every spawn of the mob
	vec3 posed = skin_blend(a_indices, a_weight, vec4(a_position, 1.0));
	vec4 view_position = mul(u_modelView, vec4(posed, 1.0));
	gl_Position = mul(u_proj, view_position);
	// The normal rides the same palette w 0 keeps the bone translation out of it
	vec3 posed_normal = skin_blend(a_indices, a_weight, vec4(a_normal, 0.0));
	vec3 world_normal = mul(u_model[0], vec4(posed_normal, 0.0)).xyz;
	v_color0 = vec4(shaded_vertex_colour(a_color0, u_ambientColour.rgb, world_normal), a_color0.a);
	v_texcoord0 = a_texcoord0;
	v_envcoord = sphere_environment_coord(mul(u_model[0], vec4(posed, 1.0)).xyz, world_normal);
	// Table fog runs on the eye-space depth which the projection leaves in w
	v_fogdepth = mix(gl_Position.w, length(view_position.xyz), u_fogRange.z);
}
