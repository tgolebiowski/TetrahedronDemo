#version 140

uniform mat4 modelMatrix;
attribute vec3 position;
attribute vec2 texCoord;

void main() { 
	gl_Position = modelMatrix * vec4( position, 1.0f);
	gl_TexCoord[0] = vec4(texCoord.xy, 0.0f, 0.0f);
}