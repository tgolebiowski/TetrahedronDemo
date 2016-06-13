#include "ColorLogic.h"

struct TetraRenderParams {
    Mat4 transform;
    Color* colors [6];
    float widthsAndTypes [4];
};

struct GameMemory {
    TetraRenderParams renderParams[5];
    MeshGeometryData tetraData;
    MeshGPUBinding binding;
    ShaderProgram tetraShader;
    ShaderProgramParams tetraRenderParams;
    SlabSubsection_Stack lasResidentStorage;
    Pallette pallette;
    TextureData spaceData;
    TextureBindingID spaceTexBinding;
    float screenSize[2];
    Vec3 lightDirection;
    Framebuffer myColorBuffer;
    Framebuffer myDepthBuffer;
    LoadedSound backgroundSound;
};

void CreateTetrahedron( MeshGeometryData* storage, SlabSubsection_Stack* savedSpace ) {
    float myRad = 3.0f;
    Vec3 v1, v2, v3, v4;
    v1 = { 0.0f, 1.0f * myRad, 0.0f };
    v2 = { 0.943f * myRad, -0.333f * myRad , 0.0f };
    v3 = { -0.471f * myRad, -0.333f * myRad, 0.816f * myRad };
    v4 = { -0.471f * myRad, -0.333f * myRad, -0.816f * myRad };

    Vec3 face1Normal, face2Normal, face3Normal, face4Normal;
    face1Normal = Cross( DiffVec( v2, v1 ), DiffVec( v2, v3 ) );
    face2Normal = Cross( DiffVec( v3, v4 ), DiffVec( v3, v2 ) );
    face3Normal = Cross( DiffVec( v4, v1 ), DiffVec( v4, v2 ) );
    face4Normal = Cross( DiffVec( v3, v1 ), DiffVec( v3, v4 ) );
    Normalize( &face1Normal ); Normalize( &face2Normal );
    Normalize( &face3Normal ); Normalize( &face4Normal );

    struct uvCoord {
        float x,y;
    };
    uvCoord uv1, uv2, uv3;
    uv1 = { 0.0f, 1.0f };
    uv2 = { 1.0f, 1.0f };
    uv3 = { 0.0f, 0.0f };

    storage->vData = (Vec3*)AllocOnSubStack_Aligned( savedSpace, sizeof( Vec3 ) * 3 * 4, 4 );
    storage->uvData = (float*)AllocOnSubStack_Aligned( savedSpace, sizeof( uvCoord ) * 3 * 4, 4 );
    storage->iData = (uint32*)AllocOnSubStack_Aligned( savedSpace, sizeof( uint32 ) * 3 * 4, 4 );
    storage->normalData = (Vec3*)AllocOnSubStack_Aligned( savedSpace, sizeof( Vec3 ) * 3 * 4, 4 );

    storage->vData[0] = v1; storage->vData[1] = v2; storage->vData[2] = v3;
    storage->vData[3] = v4; storage->vData[4] = v3; storage->vData[5] = v2;
    storage->vData[6] = v1; storage->vData[7] = v4; storage->vData[8] = v2;
    storage->vData[9] = v1; storage->vData[10] = v3; storage->vData[11] = v4;
    storage->iData[0] = 0; storage->iData[1] = 1; storage->iData[2] = 2;
    storage->iData[3] = 3; storage->iData[4] = 4; storage->iData[5] = 5;
    storage->iData[6] = 6; storage->iData[7] = 7; storage->iData[8] = 8;
    storage->iData[9] = 9; storage->iData[10] = 10; storage->iData[11] = 11;
    storage->uvData[0] = uv1.x; storage->uvData[1] = uv1.y; storage->uvData[2] = uv2.x; storage->uvData[3] = uv2.y; storage->uvData[4] = uv3.x; storage->uvData[5] = uv3.y;
    storage->uvData[6] = uv1.x; storage->uvData[7] = uv1.y; storage->uvData[8] = uv2.x; storage->uvData[9] = uv2.y; storage->uvData[10] = uv3.x; storage->uvData[11] = uv3.y;
    storage->uvData[12] = uv1.x; storage->uvData[13] = uv1.y; storage->uvData[14] = uv2.x; storage->uvData[15] = uv2.y; storage->uvData[16] = uv3.x; storage->uvData[17] = uv3.y;
    storage->uvData[18] = uv1.x; storage->uvData[19] = uv1.y; storage->uvData[20] = uv2.x; storage->uvData[21] = uv2.y; storage->uvData[22] = uv3.x; storage->uvData[23] = uv3.y;
    storage->normalData[0] = face1Normal; storage->normalData[1] = face1Normal; storage->normalData[2] = face1Normal;
    storage->normalData[3] = face2Normal; storage->normalData[4] = face2Normal; storage->normalData[5] = face2Normal;
    storage->normalData[6] = face3Normal; storage->normalData[7] = face3Normal; storage->normalData[8] = face3Normal;
    storage->normalData[9] = face4Normal; storage->normalData[10] = face4Normal; storage->normalData[11] = face4Normal;

    storage->dataCount = 12;
}

void GameInit( MemorySlab* mainSlab, void* gameMemory, RendererStorage* rendererStoragePtr ) {
    GameMemory* gMem = (GameMemory*)gameMemory;

    gMem->lasResidentStorage = CarveNewSubsection( mainSlab, KILOBYTES( 128 ) );

    float screenAspectRatio = (float)SCREEN_HEIGHT / (float)SCREEN_WIDTH;
    SetRendererCameraProjection( 10.0f, 10.0f * screenAspectRatio, 6.0f, -6.0f, &rendererStoragePtr->baseProjectionMatrix );
    SetRendererCameraTransform( rendererStoragePtr, { 0.0f, 0.0f, -2.0f }, { 0.0f, 0.0f, 0.0f } );

    gMem->backgroundSound = LoadWaveFile( "Data/Sounds/SpaceSounds.wav" );

    CreateShaderProgram( "Data/Shaders/Basic.vert", "Data/Shaders/Basic.frag", &gMem->tetraShader );
    LoadTextureDataFromDisk( "Data/Textures/Space.png", &gMem->spaceData );
    CreateTextureBinding( &gMem->spaceData, &gMem->spaceTexBinding );

    CreateTetrahedron( &gMem->tetraData, &gMem->lasResidentStorage );
    CreateRenderBinding( &gMem->tetraData, &gMem->binding );
    gMem->myColorBuffer = CreateFramebuffer( SCREEN_WIDTH, SCREEN_HEIGHT, Framebuffer::FramebufferType::COLOR );
    gMem->myDepthBuffer = CreateFramebuffer( SCREEN_WIDTH, SCREEN_HEIGHT, Framebuffer::FramebufferType::DEPTH );

    CreatePallette( &gMem->pallette, &gMem->lasResidentStorage, 16, 256 );
    InsertColorIntoPallette( "Yellow", { 248.0f / 255.0f, 233.0f / 255.0f, 53.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "offWhite", { 1.0f, 250.0f / 255.0f, 243.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "Magenta", { 230.0f / 255.0f, 22.0f / 255.0f, 120.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "Red", { 241.0f / 255.0f, 8.0f / 255.0f, 21.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "NeonBlue", { 64.0f / 255.0f, 218.0f / 255.0f, 244.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "NavyBlue", { 6.0f / 255.0f, 23.0f / 255.0f, 102.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "Teal", { 61.0f / 255.0f, 184.0f / 255.0f, 205.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "Green", { 45.0f / 255.0f, 172.0f / 255.0f, 91.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "DarkPurple", { 43.0f / 255.0f, 14.0f / 255.0f, 39.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "LightYellow", { 252.0f / 255.0f, 249.0f / 255.0f, 114.0f / 255.0f, 1.0f }, &gMem->pallette );
    InsertColorIntoPallette( "ShadowBlue", { 2.0f / 255.0f, 4.0f / 255.0f, 98.0f / 255.0f, 1.0f }, &gMem->pallette );

    gMem->screenSize[0] = SCREEN_WIDTH;
    gMem->screenSize[1] = SCREEN_HEIGHT;
    gMem->lightDirection = { -0.707f, -1.0f, 0.5f };
    Normalize( &gMem->lightDirection );

    gMem->tetraRenderParams = CreateShaderParamSet( &gMem->tetraShader );
    gMem->tetraRenderParams.indexDataPtr = gMem->binding.indexDataPtr;
    gMem->tetraRenderParams.indiciesToDraw = gMem->binding.dataCount;
    SetVertexInput( &gMem->tetraRenderParams, "position", gMem->binding.vertexDataPtr );
    SetVertexInput( &gMem->tetraRenderParams, "normal", gMem->binding.nrmlDataPtr );
    SetVertexInput( &gMem->tetraRenderParams, "texCoord", gMem->binding.uvDataPtr );
    SetUniform( &gMem->tetraRenderParams, "cameraMatrix", (void*)&rendererStoragePtr->cameraTransform );
    SetUniform( &gMem->tetraRenderParams, "screenSize", (void*)&gMem->screenSize[0] );
    SetUniform( &gMem->tetraRenderParams, "lightDirection", (void*)&gMem->lightDirection );
    SetUniform( &gMem->tetraRenderParams, "lightColor", (void*)GetColorByName( "LightYellow", &gMem->pallette ) );
    SetUniform( &gMem->tetraRenderParams, "shadowColor", (void*)GetColorByName( "ShadowBlue", &gMem->pallette ) );
    SetSampler( &gMem->tetraRenderParams, "spaceBG", gMem->spaceTexBinding );

    for( int renderParamIndex = 0; renderParamIndex < 5; ++renderParamIndex ) {
        SetToIdentity( &gMem->renderParams[ renderParamIndex ].transform );

        const float gradient = 0.5f;
        const float lines = 0.0f;
        const float space = 1.0f;

        if( renderParamIndex == 0) {
            gMem->renderParams[ renderParamIndex ].colors[0] = GetColorByName( "Teal", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[1] = GetColorByName( "Green", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[2] = GetColorByName( "DarkPurple", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[3] = GetColorByName( "DarkPurple", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[4] = GetColorByName( "NeonBlue", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[5] = GetColorByName( "NavyBlue", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[0] = 0.22f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[1] = 0.37f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[2] = gradient;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[3] = lines;
        } else if( renderParamIndex == 1 ) {
            gMem->renderParams[ renderParamIndex ].colors[0] = GetColorByName( "Teal", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[1] = GetColorByName( "Green", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[2] = GetColorByName( "DarkPurple", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[3] = GetColorByName( "DarkPurple", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[4] = GetColorByName( "Magenta", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[5] = GetColorByName( "Teal", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[0] = 0.18f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[1] = 0.35f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[2] = space;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[3] = gradient;
        } else if( renderParamIndex == 2 ) {
            gMem->renderParams[ renderParamIndex ].colors[0] = GetColorByName( "Yellow", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[1] = GetColorByName( "offWhite", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[2] = GetColorByName( "Magenta", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[3] = GetColorByName( "Red", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[4] = GetColorByName( "Red", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[5] = GetColorByName( "offWhite", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[0] = 0.18f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[1] = 0.35f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[2] = gradient;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[3] = gradient;
        } else if ( renderParamIndex == 3 ) {
            gMem->renderParams[ renderParamIndex ].colors[0] = GetColorByName( "Teal", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[1] = GetColorByName( "offWhite", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[2] = GetColorByName( "NavyBlue", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[3] = GetColorByName( "DarkPurple", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[4] = GetColorByName( "NavyBlue", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[5] = GetColorByName( "DarkPurple", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[0] = 0.22f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[1] = 0.33f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[2] = gradient;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[3] = lines;
        } else {
            gMem->renderParams[ renderParamIndex ].colors[0] = GetColorByName( "Red", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[1] = GetColorByName( "Magenta", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[2] = GetColorByName( "DarkPurple", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[3] = GetColorByName( "NavyBlue", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[4] = GetColorByName( "NeonBlue", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].colors[5] = GetColorByName( "NavyBlue", &gMem->pallette );
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[0] = 0.15f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[1] = 0.32f;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[2] = gradient;
            gMem->renderParams[ renderParamIndex ].widthsAndTypes[3] = gradient;
        }
    }
}

bool Update( void* gameMemory, float millisecondsElapsed, SoundRenderBuffer* sound, PlayingSound* activeSoundList ) {
    GameMemory* gMem = (GameMemory*)gameMemory;

    static float step = PI / 256.0f;
    const float magicY = -0.247437f;
    const float magicX = 0.834486f;
    static float magicZ = -0.846755f;

    if( IsKeyDown( 'z' ) )
        magicZ += step;
    if( IsKeyDown( 'x' ) )
        magicZ -= step;
   
    const float maxRotation = PI;
    float mousex, mousey;
    GetMousePosition( &mousex, &mousey );

    const float wiggleStep = ( ( PI / 180.0f ) * 60.0f ) / 1000.0f;
    const float angleWiggleMax = PI / 100.0f;
    static float wiggle = 0.0f;
    wiggle += ( wiggleStep * millisecondsElapsed );
    if( wiggle > ( 2.0f * PI ) ) wiggle -= ( 2.0f * PI );

    for( uint8 i = 0; i < 5; ++i ) {
        SetToIdentity( &gMem->renderParams[i].transform );

        float zExtra =  ((float)i) * ( ( 2.0f * PI ) / 5.0f );
        Mat4 y, x, z, m;
        SetToIdentity( &y ); SetToIdentity( &x ); SetToIdentity( &z ); SetToIdentity( &m );

        const float wiggleOffset = ( 2.0f * PI ) / 5.0f;
        float wiggleValue = cosf( wiggle + ( (float)i * wiggleOffset ) ) * angleWiggleMax;

        SetRotation( &y, 0.0f, 1.0f, 0.0f, magicY + ( mousey * maxRotation ) + wiggleValue );
        SetRotation( &x, 1.0f, 0.0f, 0.0f, magicX + ( mousex * maxRotation ) + wiggleValue );
        SetRotation( &z, 0.0f, 0.0f, 1.0f, magicZ + zExtra );
        gMem->renderParams[i].transform = MultMatrix( MultMatrix( y, x ), z );
    }

    static bool onlyTrueOnce = true;
    if( onlyTrueOnce ) {
        QueueLoadedSound( &gMem->backgroundSound, activeSoundList );
    }
    onlyTrueOnce = false;

    return true;
}

void Render( void* gameMemory, RendererStorage* rendererStorage ) {
    GameMemory* gMem = (GameMemory*)gameMemory;

    //SetCurrentFramebuffer( &gMem->myColorBuffer );
    //SetCurrentFramebuffer( &gMem->myDepthBuffer );
    //glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    for( int tetraRenders = 0; tetraRenders < 5; ++tetraRenders ) {
        SetUniform( &gMem->tetraRenderParams, "modelMatrix", (void*)&gMem->renderParams[ tetraRenders ].transform );
        SetUniform( &gMem->tetraRenderParams, "outerColor1", (void*)gMem->renderParams[ tetraRenders ].colors[0] );
        SetUniform( &gMem->tetraRenderParams, "outerColor2", (void*)gMem->renderParams[ tetraRenders ].colors[1] );
        SetUniform( &gMem->tetraRenderParams, "innerColor1", (void*)gMem->renderParams[ tetraRenders ].colors[2] );
        SetUniform( &gMem->tetraRenderParams, "innerColor2", (void*)gMem->renderParams[ tetraRenders ].colors[3] );
        SetUniform( &gMem->tetraRenderParams, "innerBorderColor", (void*)gMem->renderParams[ tetraRenders ].colors[4] );
        SetUniform( &gMem->tetraRenderParams, "outerBorderColor", (void*)gMem->renderParams[ tetraRenders ].colors[5] );
        SetUniform( &gMem->tetraRenderParams, "widthsAndTypes", (void*)&gMem->renderParams[ tetraRenders ].widthsAndTypes[0] );
        RenderBoundData( &gMem->tetraShader, &gMem->tetraRenderParams );
    }

    //SetCurrentFramebuffer( NULL );
    //RenderTexturedQuad( rendererStorage, gMem->myColorBuffer.textureBindingID, 1.0f, 1.0f, 0.0f, 0.0f );
}