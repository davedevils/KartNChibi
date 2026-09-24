$input a_position, a_normal, a_indices, a_weight
$output v_fogdepth

#include <bgfx_shader.sh>
#include "skinning.sh"

// Skinned vs outline shell swells along the normal in the mesh's own space before the palette moves the vertex
uniform vec4 u_lineParams;
uniform vec4 u_fogRange;

void main()
{
	vec3 swollen = a_position + a_normal * u_lineParams.x;
	vec3 posed = skin_blend(a_indices, a_weight, vec4(swollen, 1.0));
	vec4 view_position = mul(u_modelView, vec4(posed, 1.0));
	gl_Position = mul(u_proj, view_position);
	v_fogdepth = mix(gl_Position.w, length(view_position.xyz), u_fogRange.z);
}
