$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_source, 0);

// four taps of one pass from PostProcess fx BlurTransform xy is first offset zw is second
uniform vec4 u_blurTaps[2];

void main()
{
	vec4 total = texture2D(s_source, v_texcoord0 + u_blurTaps[0].xy);
	total += texture2D(s_source, v_texcoord0 + u_blurTaps[0].zw);
	total += texture2D(s_source, v_texcoord0 + u_blurTaps[1].xy);
	total += texture2D(s_source, v_texcoord0 + u_blurTaps[1].zw);
	// BlurPS divides four taps by eight the second pass adds four more
	gl_FragColor = total / 8.0;
}
