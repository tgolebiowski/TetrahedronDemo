#version 140

uniform mat4 modelMatrix;
uniform mat4 cameraMatrix;
attribute vec3 position;

void main() { 
	gl_Position = cameraMatrix * modelMatrix * vec4( position, 1.0f );
}