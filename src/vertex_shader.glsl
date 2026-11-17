precision highp float;
attribute vec2 vert_position; // screen coordinate
attribute vec3 vert_colour; // colour

varying vec3 colour;
uniform vec2 screen_size;

void main()
{
	vec2 screen_pos = vec2(2) * (vert_position.xy / screen_size) - vec2(1);

	gl_Position = vec4(screen_pos, 0, 1);

	colour = vert_colour;
}