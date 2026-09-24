$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_source, 0);

// BrightPassPS from PostProcess fx pow colour 8 over 7 5 only near white texels survive brightest is near 0 133
void main()
{
	gl_FragColor = pow(texture2D(s_source, v_texcoord0), vec4_splat(8.0)) / 7.5;
}
