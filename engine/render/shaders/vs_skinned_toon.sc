$input a_position, a_normal, a_color0, a_texcoord0, a_indices, a_weight
$output v_color0, v_texcoord0, v_fogdepth, v_toonlight

#include <bgfx_shader.sh>
#include "skinning.sh"

// Skinned vs toon matches SkinToonEdgeShader NSB same two-pass cartoon as static one with palette blend first
uniform vec4 u_toonLight;
uniform vec4 u_ambientColour;
uniform vec4 u_fogRange;

void main()
{
	vec3 posed = skin_blend(a_indices, a_weight, vec4(a_position, 1.0));
	vec3 posed_normal = skin_blend(a_indices, a_weight, vec4(a_normal, 0.0));
	vec4 view_position = mul(u_modelView, vec4(posed, 1.0));
	gl_Position = mul(u_proj, view_position);
	vec3 view_normal = mul(u_modelView, vec4(posed_normal, 0.0)).xyz;
	float length_squared = dot(view_normal, view_normal);
	// A zero normal marks a vertex whose colour is already shaded
	v_toonlight = length_squared > 0.0
		? max(dot(-view_normal * inversesqrt(length_squared), u_toonLight.xyz), 0.0)
		: 1.0;
	v_color0 = vec4(a_color0.rgb * u_ambientColour.rgb, a_color0.a);
	v_texcoord0 = a_texcoord0;
	// Table fog runs on the eye-space depth which the projection leaves in w
	v_fogdepth = mix(gl_Position.w, length(view_position.xyz), u_fogRange.z);
}
