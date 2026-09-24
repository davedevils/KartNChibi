$input a_position, a_normal, a_color0, a_texcoord0
$output v_color0, v_texcoord0, v_fogdepth, v_toonlight

#include <bgfx_shader.sh>

// xyz is const LightDir the direction StaticToonEdgeShader NSB bakes into c1 a view-space vector so light turns with camera
uniform vec4 u_toonLight;
// rgb is the hours ambient colour which reaches the shader as AmbientLight
uniform vec4 u_ambientColour;
// z 1 measures the fog distance radially which is what RANGEFOGENABLE does
uniform vec4 u_fogRange;

void main()
{
	vec4 view_position = mul(u_modelView, vec4(a_position, 1.0));
	gl_Position = mul(u_proj, view_position);
	// StaticToonShader vsh skips normalising r1 like the original ours also detects the zero sentinel
	vec3 view_normal = mul(u_modelView, vec4(a_normal, 0.0)).xyz;
	float length_squared = dot(view_normal, view_normal);
	// A zero normal marks a vertex whose colour is already shaded
	v_toonlight = length_squared > 0.0
		? max(dot(-view_normal * inversesqrt(length_squared), u_toonLight.xyz), 0.0)
		: 1.0;
	// oD0 is EnvironmentAmbientColor times AmbientLight times MaterialDiffuse and NSB ships EnvironmentAmbientColor white
	v_color0 = vec4(a_color0.rgb * u_ambientColour.rgb, a_color0.a);
	v_texcoord0 = a_texcoord0;
	// Table fog runs on the eye-space depth which the projection leaves in w
	v_fogdepth = mix(gl_Position.w, length(view_position.xyz), u_fogRange.z);
}
