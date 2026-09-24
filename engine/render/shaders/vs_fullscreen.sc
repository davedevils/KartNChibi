$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

// Triangle already in clip space PostProcess fx Transform mul pos WorldViewProj against identity has nothing to do
void main()
{
	gl_Position = vec4(a_position, 1.0);
	v_texcoord0 = a_texcoord0;
}
