$input v_fogdepth

#include <bgfx_shader.sh>
#include "shading.sh"

// rgb is LineColor StaticSilhouette vsh writes it into oD0 no texture sampled so shell is flat colour behind surface
uniform vec4 u_lineColour;

void main()
{
	gl_FragColor = vec4(mix(u_lineColour.rgb, u_fogColour.rgb, fog_amount(v_fogdepth)), 1.0);
}
