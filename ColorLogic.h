struct Color {
    float r,g,b,a;
};

struct Color_HSV {
	float h, s, v;
};

struct Pallette {
	char* nameBuffer;
	char** names;
	Color* colors;
	uint16 nameBufferSize, nextNameSpot;
};

void CreatePallette( Pallette* pallette, SlabSubsection_Stack* allocater, uint8 maxNumColors, uint16 nameBufferSize ) {
	pallette->nameBuffer = (char*)AllocOnSubStack( allocater, nameBufferSize );
	pallette->names = (char**)AllocOnSubStack_Aligned( allocater, maxNumColors * sizeof( char* ), sizeof( char* ) );
	pallette->colors = (Color*)AllocOnSubStack_Aligned( allocater, maxNumColors * sizeof( Color ), sizeof( float ) );
	pallette->nameBufferSize = nameBufferSize;
	pallette->nextNameSpot = 0;
}

void InsertColorIntoPallette( const char* name, Color color, Pallette* pallette ) {
	size_t newNameLen = strlen( name );
	char* namePtr = &pallette->nameBuffer[ pallette->nextNameSpot ];
	memcpy( namePtr, name, newNameLen );
	pallette->nameBuffer[ pallette->nextNameSpot + newNameLen + 1 ] = 0;
	pallette->nextNameSpot += newNameLen + 2;

	uint8 colorIndex = 0;
	for(;;) {
		if( pallette->names[ colorIndex ] == NULL ) {
			pallette->names[ colorIndex ] = namePtr;
			pallette->colors[ colorIndex ] = color;
			return;
		}
		++colorIndex;
	}
}

Color* GetColorByName( const char* name, Pallette* pallette ) {
	uint8 colorIndex = 0;
	char* current = pallette->names[0];
	while( current != NULL ) {
		if( strcmp( current, name ) == 0 ) {
			return &pallette->colors[ colorIndex ];
		}
		colorIndex++;
		current = pallette->names[ colorIndex ];
	}

	return NULL;
}

Color_HSV ColorModelConversion( Color c ) {
	Color_HSV hsv;

	float alpha = 0.5f * ( 2.0f * c.r - c.g - c.b );
	float beta = ( sqrt( 3.0f ) * 0.5f ) * ( c.g - c.b );
	hsv.h = atan2( beta, alpha );
	float chroma = sqrtf( alpha * alpha + beta * beta );
	hsv.v = 0.33333f * ( c.r + c.g + c.b );
	hsv.s = chroma / hsv.v;

	return hsv;
}

Color ColorModelConversion( Color_HSV c ) {
	Color rgb;

	float chroma = c.v * c.s;
	float hPrime = c.h / ( ( 2.0f * PI ) / 6.0f );
	float x = chroma * ( 1.0f - std::abs( fmod( hPrime, 2 ) - 1 ) );

	if( hPrime >= 0.0f && hPrime < 1.0f ) {
		rgb.r = chroma; rgb.g = x; rgb.b = 0.0f;
	} else if( hPrime >= 1.0f && hPrime < 2.0f ) {
		rgb.r = x; rgb.g = chroma; rgb.b = 0.0f;
	} else if( hPrime >= 2.0f && hPrime < 3.0f ) {
		rgb.r = 0.0f; rgb.g = chroma; rgb.b = x;
	} else if( hPrime >= 3.0f && hPrime < 4.0f ) {
		rgb.r = 0.0f; rgb.g = x; rgb.b = chroma;
	} else if( hPrime >= 4.0f && hPrime < 5.0f ) {
		rgb.r = x; rgb.g = 0.0f; rgb.b = chroma;
	} else {
		rgb.r = chroma; rgb.g = 0.0f; rgb.b = x;
	}

	float min = c.v - chroma;
	rgb.r += min;
	rgb.g += min;
	rgb.b += min;

	return rgb;
}

Color CalculateComplement( Color c ) {
	Color_HSV complement = ColorModelConversion( c );

	complement.h += PI;
	if( complement.h > ( 2.0f * PI ) ) complement.h -= ( 2.0f * PI );

	return ColorModelConversion( complement );
}