$input v_color0, v_texcoord0, v_fogdepth

#include <bgfx_shader.sh>
#include "shading.sh"

SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_coverage, 1);
SAMPLER2D(s_detail, 2);

void main()
{
	vec4 diffuse = texture2D(s_diffuse, mapped_texcoord(v_texcoord0));
	// The detail map of the NiTexturingProperty the fixed function stage modulates it twice
	if (u_detailParams.x > 0.5)
		diffuse.rgb = min(diffuse.rgb * texture2D(s_detail, mapped_detail_texcoord(v_texcoord0)).rgb * 2.0, vec3_splat(1.0));
	// The alpha test of the NiAlphaProperty the sign of the cutout carries its function
	float cutout = u_splatParams.z;
	if (cutout > 0.0 && diffuse.a < cutout) discard;
	if (cutout < 0.0 && diffuse.a >= -cutout) discard;
	// straight alpha reaches device as authored blend pair takes it so additive surface adds its own colour only
	float coverage = texture2D(s_coverage, v_texcoord0).a;
	vec4 lit = vec4(diffuse.rgb * v_color0.rgb, diffuse.a * coverage * surface_alpha(v_color0.a));
	float fogged = fog_amount(v_fogdepth);
	gl_FragColor = vec4(mix(lit.rgb, u_fogColour.rgb, fogged), lit.a);
}
