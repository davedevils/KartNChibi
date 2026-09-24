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
// xyz the world travel direction of a NiDirectionalLight of the model own tree w is 1 when set
uniform vec4 u_modelLightDirection[2];
uniform vec4 u_modelLightColour[2];
// xyz half the world third and second columns of a NiTextureEffect sphere map w 1 when a part has one
uniform vec4 u_environmentRowU;
uniform vec4 u_environmentRowV;
uniform vec4 u_eyePosition;

vec3 shaded_vertex_colour(vec4 vertex_colour, vec3 ambient_colour, vec3 world_normal)
{
	vec3 emissive = mix(u_shadeEmissive.rgb, vertex_colour.rgb, u_shadeEmissive.w);
	vec3 ambient = mix(u_shadeAmbient.rgb, vertex_colour.rgb, u_shadeAmbient.w);
	vec3 diffuse = mix(u_shadeDiffuse.rgb, vertex_colour.rgb, u_shadeDiffuse.w);
	// Zero normal from a mesh without exported normals would normalise to NaN and paint it black
	float reach = length(world_normal);
	float lambert = reach > 0.0 ? max(dot(world_normal / reach, -u_sunDirection.xyz), 0.0) : 0.0;
	vec3 sun = diffuse * u_sunColour.rgb * lambert * u_sunDirection.w;
	vec3 unit = reach > 0.0 ? world_normal / reach : vec3_splat(0.0);
	sun += diffuse * u_modelLightColour[0].rgb * max(dot(unit, -u_modelLightDirection[0].xyz), 0.0) * u_modelLightDirection[0].w;
	sun += diffuse * u_modelLightColour[1].rgb * max(dot(unit, -u_modelLightDirection[1].xyz), 0.0) * u_modelLightDirection[1].w;
	// The device clamps the lit vertex colour before it reaches the stage
	return min(emissive + ambient * ambient_colour + sun, vec3_splat(1.0));
}

// D3D camera space reflection vector 2 N dot E N minus E read through the sphere map projection
vec2 sphere_environment_coord(vec3 world_position, vec3 world_normal)
{
	float reach = length(world_normal);
	if (u_environmentRowU.w < 0.5 || reach <= 0.0) return vec2_splat(0.5);
	vec3 unit = world_normal / reach;
	vec3 to_eye = normalize(u_eyePosition.xyz - world_position);
	vec3 reflected = 2.0 * dot(to_eye, unit) * unit - to_eye;
	return vec2(dot(u_environmentRowU.xyz, reflected), dot(u_environmentRowV.xyz, reflected)) + vec2_splat(0.5);
}
