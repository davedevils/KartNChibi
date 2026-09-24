$input a_position, a_normal, a_color0, a_texcoord0
$output v_color0, v_texcoord0, v_fogdepth

#include <bgfx_shader.sh>
#include "shade.sh"

// rgb is the hours ambient colour the device holds as D3DRS AMBIENT bgfx has no vec3 uniform so w pads
uniform vec4 u_ambientColour;
// z 1 measures the fog distance radially which is what RANGEFOGENABLE does
uniform vec4 u_fogRange;

void main()
{
	vec4 view_position = mul(u_modelView, vec4(a_position, 1.0));
	gl_Position = mul(u_proj, view_position);
	// The normal feeds the sun term when the scene set one ambient only otherwise
	vec3 world_normal = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
	v_color0 = vec4(shaded_vertex_colour(a_color0, u_ambientColour.rgb, world_normal), a_color0.a);
	v_texcoord0 = a_texcoord0;
	// Table fog runs on the eye-space depth which the projection leaves in w
	v_fogdepth = mix(gl_Position.w, length(view_position.xyz), u_fogRange.z);
}
