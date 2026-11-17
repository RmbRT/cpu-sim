precision mediump float;
varying vec3 colour;

#if 0
// Converts a color from linear light gamma to sRGB gamma
// CC BY-SA 3.0 from https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
vec3 toSRGB(vec3 linearRGB)
{
	vec3 cutoff = vec3(lessThan(linearRGB, vec3(0.0031308)));
	vec3 higher = vec3(1.055)*pow(linearRGB, vec3(1.0/2.4)) - vec3(0.055);
	vec3 lower = linearRGB * vec3(12.92);

	return higher * (vec3(1.0) - cutoff) + lower * cutoff;
}

// Converts a color from sRGB gamma to linear light gamma
// CC BY-SA 3.0 from https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
vec3 toLinear(vec3 sRGB)
{
	vec3 cutoff = vec3(lessThan(sRGB, vec3(0.04045)));
	vec3 higher = pow((sRGB + vec3(0.055))/vec3(1.055), vec3(2.4));
	vec3 lower = sRGB/vec3(12.92);

	return higher * (vec3(1.0) - cutoff) + lower * cutoff;
}
#endif

void main() {
	gl_FragColor = vec4(colour, 1);

	//gl_FragColor = toLinear(gl_FragColor);
	//gl_FragColor = colour;

	//gl_FragColor = toSRGB(gl_FragColor);
}