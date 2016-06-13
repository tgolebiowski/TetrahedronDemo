#version 140
uniform sampler2D tex1;

void main() {
	gl_FragColor = texture( tex1, gl_TexCoord[0].xy );
}