// Matches Renderer SetupVertexColorLighting 0x00648E90 and ApplyLights 0x00648B00 in HBOnline exe one global ambient no directional term

// rgb is emissive colour w is 1 when vertex colour overrides it
uniform vec4 u_shadeEmissive;
// rgb is ambient colour w is 1 when vertex colour overrides it
uniform vec4 u_shadeAmbient;
// rgb is diffuse colour w is 1 when vertex colour overrides it
uniform vec4 u_shadeDiffuse;
// xyz is sun direction world space w is 1 when a sun is set NiDirectionalLight comes from World Light nif
uniform vec4 u_sunDirection;
// rgb is the sun's diffuse colour
uniform vec4 u_sunColour;

vec3 shaded_vertex_colour(vec4 vertex_colour, vec3 ambient_colour, vec3 world_normal)
{
	vec3 emissive = mix(u_shadeEmissive.rgb, vertex_colour.rgb, u_shadeEmissive.w);
	vec3 ambient = mix(u_shadeAmbient.rgb, vertex_colour.rgb, u_shadeAmbient.w);
	vec3 diffuse = mix(u_shadeDiffuse.rgb, vertex_colour.rgb, u_shadeDiffuse.w);
	// Zero normal from a mesh without exported normals would normalise to NaN and paint it black
	float reach = length(world_normal);
	float lambert = reach > 0.0 ? max(dot(world_normal / reach, -u_sunDirection.xyz), 0.0) : 0.0;
	vec3 sun = diffuse * u_sunColour.rgb * lambert * u_sunDirection.w;
	// The device clamps the lit vertex colour before it reaches the stage
	return min(emissive + ambient * ambient_colour + sun, vec3_splat(1.0));
}
