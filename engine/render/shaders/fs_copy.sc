$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_source, 0);

// CombineBuffer has no pixel shader stage 0 is SELECTARG1 on the texture blend state decides replace or add
void main()
{
	gl_FragColor = texture2D(s_source, v_texcoord0);
}
