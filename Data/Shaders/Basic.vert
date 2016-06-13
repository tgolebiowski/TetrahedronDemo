#version 140

uniform mat4 modelMatrix;
uniform mat4 cameraMatrix;

attribute vec3 position;
attribute vec3 normal;
attribute vec2 texCoord;

smooth out vec3 bary;
smooth out vec3 nrml;

void main() { 
	gl_TexCoord[0] = vec4( texCoord.xy, 0.0f, 0.0f );

	if( texCoord.x == 0.0f && texCoord.y == 1.0f ) {
		bary = vec3( 1.0f, 0.0f, 0.0f );
	} else if( texCoord.x == 1.0f && texCoord.y == 1.0f ) {
		bary = vec3( 0.0f, 1.0f, 0.0f );
	} else {
		bary = vec3( 0.0f, 0.0f, 1.0f );
	}

	vec3 normalisbeingused = normal;
	gl_Position = cameraMatrix * modelMatrix * vec4( position, 1.0f );
	nrml = (modelMatrix * vec4( normal, 0.0f ) ).xyz;
	normalize( nrml );
}