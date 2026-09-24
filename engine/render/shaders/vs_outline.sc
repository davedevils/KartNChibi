$input a_position, a_normal
$output v_fogdepth

#include <bgfx_shader.sh>

// x is LineThickness StaticSilhouette vsh pushes the vertex along its normal in model space before world transform
uniform vec4 u_lineParams;
uniform vec4 u_fogRange;

void main()
{
	vec3 swollen = a_position + a_normal * u_lineParams.x;
	vec4 view_position = mul(u_modelView, vec4(swollen, 1.0));
	gl_Position = mul(u_proj, view_position);
	v_fogdepth = mix(gl_Position.w, length(view_position.xyz), u_fogRange.z);
}
