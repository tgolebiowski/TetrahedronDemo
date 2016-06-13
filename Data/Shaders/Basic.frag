#version 140
uniform vec4 outerColor1;
uniform vec4 outerColor2;
uniform vec4 innerColor1;
uniform vec4 innerColor2;
uniform vec4 innerBorderColor;
uniform vec4 outerBorderColor;
uniform vec4 widthsAndTypes;
uniform vec4 lightColor;
uniform vec4 shadowColor;
uniform vec3 lightDirection;
uniform vec2 screenSize;

uniform sampler2D spaceBG;

smooth in vec3 bary;
smooth in vec3 nrml;

void ColorFragAsSpace() {
	float adjustedX = ( gl_FragCoord.x - ( ( screenSize.x - screenSize.y ) * 0.5 ) ) / screenSize.y;
	float adjustedY = gl_FragCoord.y / screenSize.y;
	gl_FragColor = texture2D( spaceBG, vec2( adjustedX, adjustedY ) );

	//Magic number, found through iteration that this produces most pleasing visual results of
	//Not writing over front parts of triangles but hiding the stuf in the back
	gl_FragDepth = 0.37125f;
}

void Shade() {
	float shadowDot = dot( lightDirection, nrml );
	float sign = sign( shadowDot );
	shadowDot = -1.0f * sign * sqrt( abs( shadowDot ) );

	if( shadowDot > 0.0f ) {
		gl_FragColor = mix( gl_FragColor, lightColor, shadowDot * 0.5f );
	} else {
		gl_FragColor = mix( gl_FragColor, shadowColor, shadowDot * -0.45f );
	}
}

void ColorAsLines( float subRange, vec4 c1, vec4 c2 ) {
	const float lineWidths = 0.1f;
	float dividedRanges = subRange / lineWidths;
	float sectionEnum = ( 1.0 + sign( mod( dividedRanges, 2.0 ) - 1.0 ) ) / 2.0;
	gl_FragColor = mix( c1, c2, sectionEnum );
}

void main() {
	float minComponent = min( min( bary.x, bary.y ), bary.z );
	float outerRange = widthsAndTypes.x * 0.33333f;
	float innerRange = widthsAndTypes.y * 0.33333f;

	float innerBorderWidth = 0.9f;
	float outerBorderWidth = 0.9f;
	gl_FragDepth = gl_FragCoord.z;

	if( minComponent > innerRange ) {
		ColorFragAsSpace();
	} else {
		if( minComponent > outerRange ) {
			float subRegionRange = ( minComponent - outerRange ) / ( innerRange - outerRange );
			if( subRegionRange < innerBorderWidth ) {
				if( widthsAndTypes.z == 0.5f ) {
					gl_FragColor = mix( innerColor1, innerColor2, subRegionRange );
					Shade();
				} else if( widthsAndTypes.z == 1.0f ) {
					ColorFragAsSpace();
				} else {
					ColorAsLines( subRegionRange, innerColor1, innerColor2 );
					Shade();
				}
			} else {
				gl_FragColor = innerBorderColor;
			}
		} else {
			float subRegionRange = minComponent / outerRange;
			if( subRegionRange < outerBorderWidth ) {
				if( widthsAndTypes.w == 0.5f ) {
					gl_FragColor = mix( outerColor1, outerColor2, subRegionRange );
					Shade();
				} else if( widthsAndTypes.w == 1.0f ) {
					ColorFragAsSpace();
				} else {
					ColorAsLines( subRegionRange, outerColor1, outerColor2 );
					Shade();
				}
			} else {
				gl_FragColor = outerBorderColor;
			}
		}
	}
}