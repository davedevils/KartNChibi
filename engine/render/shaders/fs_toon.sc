$input v_color0, v_texcoord0, v_fogdepth, v_toonlight

#include <bgfx_shader.sh>
#include "shading.sh"

SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_toonRamp, 1);

void main()
{
	vec4 diffuse = texture2D(s_diffuse, mapped_texcoord(v_texcoord0));
	// cutout sign encodes NiAlphaProperty test function
	float cutout = u_splatParams.z;
	if (cutout > 0.0 && diffuse.a < cutout) discard;
	if (cutout < 0.0 && diffuse.a >= -cutout) discard;
	// oT0 held lighting and c0y at 50% ToonRamp bmp is 256x1 regardless
	float ramp = texture2D(s_toonRamp, vec2(v_toonlight, 0.5)).r;
	// Straight alpha like the plain scene pass the NiAlphaProperty blend pair decides it
	vec4 lit = vec4(diffuse.rgb * v_color0.rgb * ramp, diffuse.a * surface_alpha(v_color0.a));
	float fogged = fog_amount(v_fogdepth);
	gl_FragColor = vec4(mix(lit.rgb, u_fogColour.rgb, fogged), lit.a);
}
