// Shared by every scene fragment program texel sampling and fog distance

// xy is diffuse repeat count z is NiAlphaProperty alpha cutout sign picks test w is material alpha
uniform vec4 u_splatParams;
// x is vertex alpha reach y is material alpha reach a surface picks one only particles take both
uniform vec4 u_shadeAlpha;
// rgb is the fog colour distance fades to w 0 leaves it unfogged which the sky wants
uniform vec4 u_fogColour;
// x is where depth fog starts y is 1 over fog end minus fog start
uniform vec4 u_fogRange;
// 2x2 texture matrix row by row m00 m01 m10 m11
uniform vec4 u_uvRows;
// xy is matrix translation zw is the centre it turns and scales about
uniform vec4 u_uvOffset;
// The same two for the detail map of slot 2 and params x is 1 when one is bound
uniform vec4 u_detailRows;
uniform vec4 u_detailOffset;
uniform vec4 u_detailParams;

// NiTexturingProperty Map transform order is centre scale rotation translate negative centre same as Max
vec2 mapped_texcoord(vec2 texcoord)
{
	vec2 placed = texcoord * u_splatParams.xy + u_uvOffset.xy - u_uvOffset.zw;
	return vec2(dot(u_uvRows.xy, placed), dot(u_uvRows.zw, placed)) + u_uvOffset.zw;
}

vec2 mapped_detail_texcoord(vec2 texcoord)
{
	vec2 placed = texcoord + u_detailOffset.xy - u_detailOffset.zw;
	return vec2(dot(u_detailRows.xy, placed), dot(u_detailRows.zw, placed)) + u_detailOffset.zw;
}

float fog_amount(float fogdepth)
{
	return clamp((fogdepth - u_fogRange.x) * u_fogRange.y, 0.0, 1.0) * u_fogColour.w;
}

// Picks not multiplies client copies one material alpha into all D3DMATERIAL9 colours HBOnline exe 0x00662A00 diffuse decides
float surface_alpha(float vertex_alpha)
{
	return mix(1.0, vertex_alpha, u_shadeAlpha.x) * mix(1.0, u_splatParams.w, u_shadeAlpha.y);
}
