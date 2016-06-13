#ifndef RENDERER_H
#define RENDERER_H
#include "Math3D.h"

struct MeshGeometryData {
	Vec3* vData;
	float* uvData;
	Vec3* normalData;
	#define MAXBONESPERVERT 4
	float* boneWeightData;
	uint32* boneIndexData;
	uint32* iData;
	uint32 dataCount;
};

struct TextureData {
	uint8* data;
	uint16 width;
	uint16 height;
	uint8 channelsPerPixel;
};

typedef uint32 TextureBindingID;

#define MAXBONES 32
struct Bone {
	Mat4 bindPose, invBindPose;
	Mat4* currentTransform;
	Bone* parent;
	Bone* children[4];
	char name[32];
	uint8 childCount;
	uint8 boneIndex;
};

struct Armature {
	Bone bones[ MAXBONES ];
	Mat4 boneTransforms[ MAXBONES ];
	Bone* rootBone;
	uint8 boneCount;
};

struct BoneKeyFrame {
	Mat4 combinedMatrix;
    Quat rotation;
	Vec3 translation, scale;
};

struct ArmatureKeyFrame {
	BoneKeyFrame targetBoneTransforms[ MAXBONES ];
};

struct MeshGPUBinding {
	uint32 dataCount;
	uint32 vertexDataPtr;
    uint32 nrmlDataPtr;
	uint32 uvDataPtr;
    uint32 indexDataPtr;

	bool hasBoneData;
	uint32 boneWeightDataPtr;
	uint32 boneIndexDataPtr;
};

#define MAX_SUPPORTED_VERT_INPUTS 16
#define MAX_SUPPORTED_UNIFORMS 16
#define MAX_SUPPORTED_TEX_SAMPLERS 4
struct ShaderProgram {
	uint32 programID;
    char nameBuffer [512];
    char* vertexInputNames[ MAX_SUPPORTED_VERT_INPUTS ];
    char* uniformNames[ MAX_SUPPORTED_UNIFORMS ];
    char* samplerNames[ MAX_SUPPORTED_TEX_SAMPLERS ];

    int32 vertexInputPtrs[ MAX_SUPPORTED_VERT_INPUTS ];
    int32 uniformPtrs[ MAX_SUPPORTED_UNIFORMS ];
    int32 samplerPtrs[ MAX_SUPPORTED_TEX_SAMPLERS ];

    int32 vertexInputTypes[ MAX_SUPPORTED_VERT_INPUTS ];
    int32 uniformTypes[ MAX_SUPPORTED_UNIFORMS ];
    uint8 vertInputCount, uniformCount, samplerCount;
};

int32 GetShaderProgramInputPtr( ShaderProgram* shader, char* inputName ) {
    for( int i = 0; i < shader->vertInputCount; ++i ) {
        if( strcmp( shader->vertexInputNames[i] , inputName ) == 0 ) {
            return shader->vertexInputPtrs[i];
        }
    }

    for( int i = 0; i < shader->uniformCount; ++i ) {
        if( strcmp( shader->uniformNames[i] , inputName ) == 0 ) {
            return shader->uniformPtrs[i];
        }
    }

    for( int i = 0; i < shader->samplerCount; ++i ) {
        if( strcmp( shader->samplerNames[i] , inputName ) == 0 ) {
            return shader->samplerPtrs[i];
        }
    }
}

struct ShaderProgramParams {
    ShaderProgram* baseProgram;

    uint32 indexDataPtr;
    uint32 indiciesToDraw;

    uint32 vertexInputData [ MAX_SUPPORTED_VERT_INPUTS ];
    void* uniformData[ MAX_SUPPORTED_UNIFORMS ];
    TextureBindingID samplerData[ MAX_SUPPORTED_TEX_SAMPLERS ];
};

ShaderProgramParams CreateShaderParamSet( ShaderProgram* baseProgram ) {
    ShaderProgramParams returnParams;
    returnParams.baseProgram = baseProgram;
    returnParams.indexDataPtr = 0;
    returnParams.indiciesToDraw = 0;
    memset( &returnParams.vertexInputData[0], 0, sizeof( uint32 ) * MAX_SUPPORTED_VERT_INPUTS );
    memset( &returnParams.uniformData[0], 0, sizeof( void* ) * MAX_SUPPORTED_UNIFORMS );
    memset( &returnParams.samplerData[0], 0, sizeof( TextureBindingID ) * MAX_SUPPORTED_TEX_SAMPLERS );
    return returnParams;
}

void SetUniform( ShaderProgramParams* params, const char* uniformName, void* newData ) {
    for( int uniformNamesIndex = 0; uniformNamesIndex < params->baseProgram->uniformCount; ++uniformNamesIndex ) {
        if( strcmp( params->baseProgram->uniformNames[ uniformNamesIndex ], uniformName ) == 0 ) {
            params->uniformData[ uniformNamesIndex ] = newData;
            return;
        }
    }

    printf( "Cannot set uniform named: %s because it couldn't be found\n", uniformName );
}

void SetSampler( ShaderProgramParams* params, const char* targetSamplerName, TextureBindingID texBinding ) {
    for( int samplerIndex = 0; samplerIndex < params->baseProgram->samplerCount; ++samplerIndex ) {
        if( strcmp( params->baseProgram->samplerNames[ samplerIndex ], targetSamplerName ) == 0 ) {
            params->samplerData[ samplerIndex ] = texBinding;
            return;
        }
    }

    printf( "Cannot set sampler named: %s because it couldn't be found\n", targetSamplerName );
}

void SetVertexInput( ShaderProgramParams* params, const char* targetInputName, uint32 gpuDataPtr ) {
    for( int vertexInputIndex = 0; vertexInputIndex < params->baseProgram->vertInputCount; ++vertexInputIndex ) {
        if( strcmp( params->baseProgram->vertexInputNames[ vertexInputIndex ], targetInputName ) == 0 ) {
            params->vertexInputData[ vertexInputIndex ] = gpuDataPtr;
            return;
        }
    }

    printf( "Cannot set vertex input named: %s because it couldn't be found\n", targetInputName );
}

struct Framebuffer {
    enum FramebufferType {
        DEPTH, COLOR
    };
    FramebufferType type;
    uint32 framebufferPtr;
    TextureData framebufferTexture;
    TextureBindingID textureBindingID;
};

struct RendererStorage{
	Mat4 baseProjectionMatrix;
	Mat4 cameraTransform;

	ShaderProgram texturedQuadShader;
	uint32 quadVDataPtr;
	uint32 quadUVDataPtr;
	uint32 quadIDataPtr;
    int32 quadPosAttribPtr;
    int32 quadUVAttribPtr;
    int32 quadMat4UniformPtr;

	ShaderProgram pShader;
	//Data for rendering lines as a debugging tool
	uint32 lineDataPtr;
	uint32 lineIDataPtr;
	//Data for rendering circles/dots as a debugging tool
	uint32 circleDataPtr;
	uint32 circleIDataPtr;
};

/*----------------------------------------------------------------------------------------------------------------
                                      PLATFORM INDEPENDENT FUNCTIONS
-----------------------------------------------------------------------------------------------------------------*/

void CreateEmptyTexture( TextureData* texData, uint16 width, uint16 height ) {
	texData->data = ( uint8* )malloc( sizeof( uint8 ) * 4 * width * height );
    texData->width = width;
    texData->height = height;
    texData->channelsPerPixel = 4;
}

void SetRendererCameraProjection( float width, float height, float nearPlane, float farPlane, Mat4* m ) {
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    float depth = nearPlane - farPlane;

    m->m[0][0] = 1.0f / halfWidth; m->m[0][1] = 0.0f; m->m[0][2] = 0.0f; m->m[0][3] = 0.0f;
    m->m[1][0] = 0.0f; m->m[1][1] = 1.0f / halfHeight; m->m[1][2] = 0.0f; m->m[1][3] = 0.0f;
    m->m[2][0] = 0.0f; m->m[2][1] = 0.0f; m->m[2][2] = 2.0f / depth; m->m[2][3] = 0.0f;
    m->m[3][0] = 0.0f; m->m[3][1] = 0.0f; m->m[3][2] = -(farPlane + nearPlane) / depth; m->m[3][3] = 1.0f;
}

void SetRendererCameraTransform( RendererStorage* rStorage, Vec3 position, Vec3 lookAtTarget ) {
	Mat4 cameraTransform = LookAtMatrix( position, lookAtTarget, { 0.0f, 1.0f, 0.0f } );
    SetTranslation( &cameraTransform, position.x, position.y, position.z );
    rStorage->cameraTransform = MultMatrix( cameraTransform, rStorage->baseProjectionMatrix );
}

void ApplyKeyFrameToArmature( ArmatureKeyFrame* pose, Armature* armature ) {
    for( uint8 boneIndex = 0; boneIndex < armature->boneCount; ++boneIndex ) {
        Bone* bone = &armature->bones[ boneIndex ];
        *bone->currentTransform = MultMatrix( bone->invBindPose, pose->targetBoneTransforms[ boneIndex ].combinedMatrix );
    }
}

ArmatureKeyFrame BlendKeyFrames( ArmatureKeyFrame* keyframeA, ArmatureKeyFrame* keyframeB, float weight, uint8 boneCount ) {
    float keyAWeight, keyBWeight;
    ArmatureKeyFrame out;
    keyAWeight = weight;
    keyBWeight = 1.0f - keyAWeight;

    for( uint8 boneIndex = 0; boneIndex < boneCount; ++boneIndex ) {
        BoneKeyFrame* netBoneKey = &out.targetBoneTransforms[ boneIndex ];
        BoneKeyFrame* bonekeyA = &keyframeA->targetBoneTransforms[ boneIndex ];
        BoneKeyFrame* bonekeyB = &keyframeB->targetBoneTransforms[ boneIndex ];

        netBoneKey->translation = { 
            bonekeyA->translation.x * keyAWeight + bonekeyB->translation.x * keyBWeight,
            bonekeyA->translation.y * keyAWeight + bonekeyB->translation.y * keyBWeight,
            bonekeyA->translation.z * keyAWeight + bonekeyB->translation.z * keyBWeight
        };
        netBoneKey->scale = {
            bonekeyA->scale.x * keyAWeight + bonekeyB->scale.x * keyBWeight,
            bonekeyA->scale.y * keyAWeight + bonekeyB->scale.y * keyBWeight,
            bonekeyA->scale.z * keyAWeight + bonekeyB->scale.z * keyBWeight
        };
        //TODO: Slerp Option
        netBoneKey->rotation = {
            bonekeyA->rotation.w * keyAWeight + bonekeyB->rotation.w * keyBWeight,
            bonekeyA->rotation.x * keyAWeight + bonekeyB->rotation.x * keyBWeight,
            bonekeyA->rotation.y * keyAWeight + bonekeyB->rotation.y * keyBWeight,
            bonekeyA->rotation.z * keyAWeight + bonekeyB->rotation.z * keyBWeight
        };

        netBoneKey->combinedMatrix = Mat4FromComponents( netBoneKey->scale, netBoneKey->rotation, netBoneKey->translation );
    }

    return out;
}

/*-----------------------------------------------------------------------------------------------------------------
                                    THINGS FOR THE RENDERER TO IMPLEMENT
------------------------------------------------------------------------------------------------------------------*/

void CreateTextureBinding( TextureData* textureData, TextureBindingID* texBindID );
void CreateShaderProgram( const char* vertProgramFilePath, const char* fragProgramFilePath, SlabSubsection_Stack* allocater, ShaderProgram* bindData );
void CreateRenderBinding( MeshGeometryData* geometryStorage, MeshGPUBinding* bindData );

RendererStorage* InitRenderer( uint16 screen_w, uint16 screen_h, SlabSubsection_Stack* systemsMemory );

void RenderBoundData( ShaderProgram* program, ShaderProgramParams params );
void RenderTexturedQuad( RendererStorage* rendererStorage, TextureBindingID texture, float width, float height, float x, float y );

void RenderDebugCircle( Vec3 position, float radius = 1.0f , Vec3 color = { 1.0f, 1.0f, 1.0f} );
void RenderDebugLines( float* vertexData, uint8 vertexCount, Mat4 transform, Vec3 color = { 1.0f, 1.0f, 1.0f } );
void RenderArmatureAsLines(  Armature* armature, Mat4 transform, Vec3 color = { 1.0f, 1.0f, 1.0f } );

//TODO: figure out if this is useful or not
//Hypothetical ease-of-use idea, you always want renderbinding but only sometimes care to store the mesh data anywhere other
//than on the GPU, then this function header below would be easiest
//LoadRenderBinding( const char fileName, MeshRenderBinding** bindDataStorage, MeshDataStorage** meshDataStorage = NULL );

/*------------------------------------------------------------------------------------------------------------------
                                     THINGS FOR THE OS LAYER TO IMPLEMENT
--------------------------------------------------------------------------------------------------------------------*/

///Return 0 on success, required buffer length if buffer is too small, or -1 on other OS failure
char* ReadShaderSrcFileFromDisk( const char* fileName );
void LoadMeshDataFromDisk( const char* fileName, SlabSubsection_Stack* allocater, MeshGeometryData* storage, Armature* armature = NULL );
void LoadAnimationDataFromCollada( const char* fileName, ArmatureKeyFrame* pose, Armature* armature );
void LoadTextureDataFromDisk( const char* fileName, TextureData* texDataStorage );

#endif //RENDERER_H

/*------------------------------------------------------------------------------------------------------------------
                                       PLATFORM SPECIFIC IMPLEMENTATION
-------------------------------------------------------------------------------------------------------------------*/
#ifdef OPENGL_RENDERER_IMPLEMENTATION

Framebuffer CreateFramebuffer( uint32 pixelWidth, uint32 pixelHeight, Framebuffer::FramebufferType type ) {
    Framebuffer newFramebuffer = { };
    newFramebuffer.type = type;
    CreateEmptyTexture( &newFramebuffer.framebufferTexture, pixelWidth, pixelHeight );
    CreateTextureBinding( &newFramebuffer.framebufferTexture, &newFramebuffer.textureBindingID );

    glGenFramebuffers( 1, &newFramebuffer.framebufferPtr );

    return newFramebuffer;
}

void SetCurrentFramebuffer( Framebuffer* framebuffer ) { 
    if( framebuffer != NULL ) {
        if( framebuffer->type == Framebuffer::FramebufferType::COLOR ) {
            glBindFramebuffer( GL_FRAMEBUFFER, framebuffer->framebufferPtr );
            glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer->textureBindingID, 0 );
        } else {
            //lFramebufferRenderBuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer->textureBindingID, 0 );
        }
    } else {
        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    }
}

void CreateTextureBinding( TextureData* texData, TextureBindingID* texBindID ) {
	GLenum pixelFormat;
    if( texData->channelsPerPixel == 3 ) {
        pixelFormat = GL_RGB;
    } else if( texData->channelsPerPixel == 4 ) {
        pixelFormat = GL_RGBA;
    }

    GLuint glTextureID;
    glGenTextures( 1, &glTextureID );
    glBindTexture( GL_TEXTURE_2D, glTextureID );

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

    glTexImage2D( GL_TEXTURE_2D, 0, texData->channelsPerPixel, texData->width, texData->height, 0, pixelFormat, GL_UNSIGNED_BYTE, texData->data );

    *texBindID = glTextureID;

    //stbi_image_free( data );
}

void PrintGLShaderLog( GLuint shader ) {
    //Make sure name is shader
    if( glIsShader( shader ) ) {
        //Shader log length
        int infoLogLength = 0;
        int maxLength = infoLogLength;
        
        //Get info string length
        glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &maxLength );
        
        //Allocate string
        GLchar infoLog[ 256 ];
        
        //Get info log
        glGetShaderInfoLog( shader, maxLength, &infoLogLength, infoLog );
        if( infoLogLength > 0 ) {
            //Print Log
            printf( "%s\n", infoLog );
        }
    } else {
        printf( "Name %d is not a shader\n", shader );
    }
}

void CreateShaderProgram( char* vertProgramFilePath, char* fragProgramFilePath, ShaderProgram* bindDataStorage ) {
    bindDataStorage->programID = glCreateProgram();

    GLuint vertexShader = glCreateShader( GL_VERTEX_SHADER );

    int64 bytesRead = 0;
    char* srcDataPtr = ReadShaderSrcFileFromDisk( vertProgramFilePath );
    glShaderSource( vertexShader, 1, &srcDataPtr, NULL );

    glCompileShader( vertexShader );
    GLint compiled = GL_FALSE;
    glGetShaderiv( vertexShader, GL_COMPILE_STATUS, &compiled );
    if( compiled != GL_TRUE ) {
        printf( "Could not compile Vertex Shader from file %s\n", vertProgramFilePath );
        PrintGLShaderLog( vertexShader );
    } else {
        printf( "Vertex Shader %s compiled\n", vertProgramFilePath );
        glAttachShader( bindDataStorage->programID, vertexShader );
    }
    free( srcDataPtr );

    GLuint fragShader = glCreateShader( GL_FRAGMENT_SHADER );
    bytesRead = 0;
    srcDataPtr = ReadShaderSrcFileFromDisk( fragProgramFilePath );
    glShaderSource( fragShader, 1, &srcDataPtr, NULL );

    glCompileShader( fragShader );
    //Check for errors
    compiled = GL_FALSE;
    glGetShaderiv( fragShader, GL_COMPILE_STATUS, &compiled );
    if( compiled != GL_TRUE ) {
        printf( "Unable to compile fragment shader from file %s\n", fragProgramFilePath );
        PrintGLShaderLog( fragShader );
    } else {
        printf( "Frag Shader %s compiled\n", fragProgramFilePath );
        //Actually attach it if it compiled
        glAttachShader( bindDataStorage->programID, fragShader );
    }
    free( srcDataPtr );

    glLinkProgram( bindDataStorage->programID );
    //Check for errors
    compiled = GL_TRUE;
    glGetProgramiv( bindDataStorage->programID, GL_LINK_STATUS, &compiled );
    if( compiled != GL_TRUE ) {
        printf( "Error linking program\n" );
    } else {
        printf( "Shader Program Linked Successfully\n");
    }

    glUseProgram( bindDataStorage->programID );

    uint32 nameWriteTargetOffset = 0;
    GLsizei nameLen;
    GLint attribSize;
    GLenum attribType;

    //Record All vertex inputs
    bindDataStorage->vertInputCount = 0;
    GLint activeGLAttributeCount;
    glGetProgramiv( bindDataStorage->programID, GL_ACTIVE_ATTRIBUTES, &activeGLAttributeCount );
    for( GLuint attributeIndex = 0; attributeIndex < activeGLAttributeCount; ++attributeIndex ) {
        char* nameWriteTarget = &bindDataStorage->nameBuffer[ nameWriteTargetOffset ];
        glGetActiveAttrib( bindDataStorage->programID, attributeIndex, 512 - nameWriteTargetOffset, &nameLen, &attribSize, &attribType, nameWriteTarget );
        bindDataStorage->vertexInputPtrs[ attributeIndex ] = glGetAttribLocation( bindDataStorage->programID, nameWriteTarget );
        bindDataStorage->vertexInputNames[ attributeIndex ] = nameWriteTarget;
        bindDataStorage->vertexInputTypes[ attributeIndex ] = attribType;
        nameWriteTargetOffset += nameLen + 1;
        bindDataStorage->vertInputCount++;
    }

    //Record all uniform info
    bindDataStorage->uniformCount = 0;
    bindDataStorage->samplerCount = 0;
    GLint activeGLUniformCount;
    glGetProgramiv( bindDataStorage->programID, GL_ACTIVE_UNIFORMS, &activeGLUniformCount );
    for( GLuint uniformIndex = 0; uniformIndex < activeGLUniformCount; ++uniformIndex ) {
        char* nameWriteTarget = &bindDataStorage->nameBuffer[ nameWriteTargetOffset ];
        glGetActiveUniform( bindDataStorage->programID, uniformIndex, 512 - nameWriteTargetOffset, &nameLen, &attribSize, &attribType, nameWriteTarget );
        if( attribType == GL_SAMPLER_2D ) {
            bindDataStorage->samplerPtrs[ bindDataStorage->samplerCount ] = glGetUniformLocation( bindDataStorage->programID, nameWriteTarget );
            glUniform1i( bindDataStorage->samplerPtrs[ bindDataStorage->samplerCount ], bindDataStorage->samplerCount );
            bindDataStorage->samplerNames[ bindDataStorage->samplerCount++ ] = nameWriteTarget;
        } else {
            bindDataStorage->uniformPtrs[ uniformIndex - bindDataStorage->samplerCount ] = glGetUniformLocation( bindDataStorage->programID, nameWriteTarget );
            bindDataStorage->uniformNames[ uniformIndex - bindDataStorage->samplerCount ] = nameWriteTarget;
            bindDataStorage->uniformTypes[ uniformIndex - bindDataStorage->samplerCount ] = attribType;
            ++bindDataStorage->uniformCount;
        }
        nameWriteTargetOffset += nameLen + 1;       
     }

    glUseProgram(0);
    glDeleteShader( vertexShader ); 
    glDeleteShader( fragShader );
}

void CreateRenderBinding( MeshGeometryData* meshDataStorage, MeshGPUBinding* bindDataStorage ) {
	GLuint glVBOPtr;
	glGenBuffers( 1, &glVBOPtr );
	glBindBuffer( GL_ARRAY_BUFFER, glVBOPtr );
	glBufferData( GL_ARRAY_BUFFER, meshDataStorage->dataCount  * 3 * sizeof(float), meshDataStorage->vData, GL_STATIC_DRAW );
	bindDataStorage->vertexDataPtr = glVBOPtr;

    GLuint glNormalPtr;
    glGenBuffers( 1, &glNormalPtr );
    glBindBuffer( GL_ARRAY_BUFFER, glNormalPtr );
    glBufferData( GL_ARRAY_BUFFER, meshDataStorage->dataCount * 3 * sizeof(float), meshDataStorage->normalData, GL_STATIC_DRAW );
    bindDataStorage->nrmlDataPtr = glNormalPtr;

    GLuint glUVPtr;
    glGenBuffers( 1, &glUVPtr );
    glBindBuffer( GL_ARRAY_BUFFER, glUVPtr );
    glBufferData( GL_ARRAY_BUFFER, meshDataStorage->dataCount * 2 * sizeof(float), meshDataStorage->uvData, GL_STATIC_DRAW );
    bindDataStorage->uvDataPtr = glUVPtr;

	GLuint glIBOPtr;
	glGenBuffers( 1, &glIBOPtr );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, glIBOPtr );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, meshDataStorage->dataCount * sizeof(uint32), meshDataStorage->iData, GL_STATIC_DRAW );
	bindDataStorage->indexDataPtr = glIBOPtr;

    if( meshDataStorage->boneWeightData != NULL && meshDataStorage->boneIndexData != NULL ) {
        bindDataStorage->hasBoneData = true;

        GLuint glBoneWeightBufferPtr;
        glGenBuffers( 1, &glBoneWeightBufferPtr );
        glBindBuffer( GL_ARRAY_BUFFER, glBoneWeightBufferPtr );
        glBufferData( GL_ARRAY_BUFFER, meshDataStorage->dataCount * MAXBONESPERVERT * sizeof(float), meshDataStorage->boneWeightData, GL_STATIC_DRAW );
        bindDataStorage->boneWeightDataPtr = glBoneWeightBufferPtr;

        GLuint glBoneIndexBufferPtr;
        glGenBuffers( 1, &glBoneIndexBufferPtr );
        glBindBuffer( GL_ARRAY_BUFFER, glBoneIndexBufferPtr );
        glBufferData( GL_ARRAY_BUFFER, meshDataStorage->dataCount * MAXBONESPERVERT * sizeof(uint32), meshDataStorage->boneIndexData, GL_STATIC_DRAW );
        bindDataStorage->boneIndexDataPtr = glBoneIndexBufferPtr;
    } else {
        bindDataStorage->hasBoneData = false;
    }

	bindDataStorage->dataCount = meshDataStorage->dataCount;
}

RendererStorage* InitRenderer( uint16 screen_w, uint16 screen_h, SlabSubsection_Stack* systemsMemory ) {
    RendererStorage* rendererStorage = (RendererStorage*)AllocOnSubStack_Aligned( systemsMemory, sizeof( RendererStorage ), 4 );

	printf( "Vendor: %s\n", glGetString( GL_VENDOR ) );
    printf( "Renderer: %s\n", glGetString( GL_RENDERER ) );
    printf( "GL Version: %s\n", glGetString( GL_VERSION ) );
    printf( "GLSL Version: %s\n", glGetString( GL_SHADING_LANGUAGE_VERSION ) );

    GLint k;
    glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &k );
    printf("Max texture units: %d\n", k);

    glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &k );
    printf( "Max Vertex Attributes: %d\n", k );

    //Initialize clear color
    glClearColor( 50.0f / 255.0f, 15.0f / 255.0f, 32.0f / 255.0f, 1.0f );

    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );
    glCullFace( GL_BACK );

    glViewport( 0, 0, screen_w, screen_h );

    SetToIdentity( &rendererStorage->baseProjectionMatrix );
    SetToIdentity( &rendererStorage->cameraTransform );

    CreateShaderProgram( "Data/Shaders/TexturedQuad.vert", "Data/Shaders/TexturedQuad.frag", &rendererStorage->texturedQuadShader );
    GLfloat quadVData[12] = { -1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f };
    GLfloat quadUVData[8] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
    const GLuint quadIndexData[6] = { 0, 1, 2, 0, 2, 3 };
    GLuint quadDataPtrs [3];
    glGenBuffers( 3, (GLuint*)quadDataPtrs );

    rendererStorage->quadVDataPtr = quadDataPtrs[0];
    glBindBuffer( GL_ARRAY_BUFFER, quadDataPtrs[0] );
    glBufferData( GL_ARRAY_BUFFER, 12 * sizeof( GLfloat ), &quadVData[0], GL_STATIC_DRAW );
    rendererStorage->quadUVDataPtr = quadDataPtrs[1];
    glBindBuffer( GL_ARRAY_BUFFER, quadDataPtrs[1] );
    glBufferData( GL_ARRAY_BUFFER, 8 * sizeof( GLfloat ), &quadUVData[0], GL_STATIC_DRAW );
    rendererStorage->quadIDataPtr = quadDataPtrs[2];
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, quadDataPtrs[2] );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof( GLuint ), &quadIndexData[0], GL_STATIC_DRAW );
    rendererStorage->quadPosAttribPtr = GetShaderProgramInputPtr( &rendererStorage->texturedQuadShader, "position" );
    rendererStorage->quadUVAttribPtr = GetShaderProgramInputPtr( &rendererStorage->texturedQuadShader, "texCoord" );
    rendererStorage->quadMat4UniformPtr = GetShaderProgramInputPtr( &rendererStorage->texturedQuadShader, "modelMatrix" );

    //Check for error
    GLenum error = glGetError();
    if( error != GL_NO_ERROR ) {
        printf( "Error initializing OpenGL! %s\n", gluErrorString( error ) );
        return false;
    } else {
        printf( "Initialized OpenGL\n" );
    }

    CreateShaderProgram( "Data/Shaders/Primitive.vert", "Data/Shaders/Primitive.frag", &rendererStorage->pShader );

    //Initialization of data for line primitives
    GLuint glLineDataPtr, glLineIndexPtr;
    glGenBuffers( 1, &glLineDataPtr );
    glGenBuffers( 1, &glLineIndexPtr );
    rendererStorage->lineDataPtr = glLineDataPtr;
    rendererStorage->lineIDataPtr = glLineIndexPtr;

    GLuint sequentialIndexBuffer[64];
    for( uint8 i = 0; i < 64; i++ ) {
        sequentialIndexBuffer[i] = i;
    }
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, glLineIndexPtr );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, 64 * sizeof(GLuint), &sequentialIndexBuffer[0], GL_STATIC_DRAW );

    //Initialization of data for circle primitives
    GLuint glCircleDataPtr, glCircleIndexPtr;
    glGenBuffers( 1, &glCircleDataPtr );
    glGenBuffers( 1, &glCircleIndexPtr );
    rendererStorage->circleDataPtr = glCircleDataPtr;
    rendererStorage->circleIDataPtr = glCircleIndexPtr;

    GLfloat circleVertexData[ 18 * 3 ];
    circleVertexData[0] = 0.0f;
    circleVertexData[1] = 0.0f;
    circleVertexData[2] = 0.0f;
    for( uint8 i = 0; i < 17; i++ ) {
        GLfloat x, y;
        float angle = (2.0f * PI / 16.0f) * (float)i;
        x = cosf( angle );
        y = sinf( angle );
        circleVertexData[ (i + 1) * 3 ] = x;
        circleVertexData[ (i + 1) * 3 + 1 ] = y;
        circleVertexData[ (i + 1) * 3 + 2 ] = 0.0f;
    }
    glBindBuffer( GL_ARRAY_BUFFER, glCircleDataPtr );
    glBufferData( GL_ARRAY_BUFFER, 18 * 3 * sizeof(GLfloat), &circleVertexData[0], GL_STATIC_DRAW );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, glCircleIndexPtr );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, 18 * sizeof(GLuint), &sequentialIndexBuffer[0], GL_STATIC_DRAW );

    return rendererStorage;
}

void RenderBoundData( ShaderProgram* programBinding, ShaderProgramParams* params ) {
	//Flush errors
    //while( glGetError() != GL_NO_ERROR ){};

    //Bind Shader
    glUseProgram( programBinding->programID );

    for( int attributeIndex = 0; attributeIndex < programBinding->vertInputCount; ++attributeIndex ) {
        GLuint attribPtr = programBinding->vertexInputPtrs[ attributeIndex ];
        GLenum type = programBinding->vertexInputTypes[ attributeIndex ];
        uint32 attribBufferPtr = params->vertexInputData[ attributeIndex ];

        if( attribBufferPtr == 0 ) {
            continue;
        }

        int count;
        uint32 attributeDataPtr = params->vertexInputData[ attributeIndex ];
        if( type == GL_FLOAT_VEC3 ) {
            count = 3;
        } else if( type == GL_FLOAT_VEC2 ) {
            count = 2;
        }
        glBindBuffer( GL_ARRAY_BUFFER, attributeDataPtr );
        glEnableVertexAttribArray( attribPtr );
        glVertexAttribPointer( attribPtr, count, GL_FLOAT, GL_FALSE, 0, 0 );
    }

    for( int uniformIndex = 0; uniformIndex < programBinding->uniformCount; ++uniformIndex ) {
        GLuint uniformPtr = programBinding->uniformPtrs[ uniformIndex ];
        GLenum type = programBinding->uniformTypes[ uniformIndex ];
        void* uniformData = params->uniformData[ uniformIndex ];

        if( uniformData == 0 ) {
            continue;
        }

        if( type == GL_FLOAT_VEC4 ) {
            glUniform4fv( uniformPtr, 1, (float*)uniformData );
        } else if( type == GL_FLOAT_MAT4 ) {
            glUniformMatrix4fv( uniformPtr, 1, GL_FALSE, (float*)uniformData );
        } else if( type == GL_FLOAT_VEC2 ) {
            glUniform2fv( uniformPtr, 1, (float*)uniformData );
        } else if( type == GL_FLOAT_VEC3 ) {
            glUniform3fv( uniformPtr, 1, (float*)uniformData );
        }
    }

    for( int samplerIndex = 0; samplerIndex < programBinding->samplerCount; ++samplerIndex ){
        if( params->samplerData == 0 ) {
            continue;
        }
        glActiveTexture( GL_TEXTURE0 + samplerIndex );
        glBindTexture( GL_TEXTURE_2D, params->samplerData[ samplerIndex ] );
    }

    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, params->indexDataPtr );
    glDrawElements( GL_TRIANGLES, params->indiciesToDraw, GL_UNSIGNED_INT, NULL );

    for( int samplerIndex = 0; samplerIndex < programBinding->samplerCount; ++samplerIndex ) {
        glActiveTexture( GL_TEXTURE0 + samplerIndex );
        glBindTexture( GL_TEXTURE_2D, 0 );
    }
    glUseProgram( 0 );
}

void RenderTexturedQuad( RendererStorage* rendererStorage, TextureBindingID texture, float width, float height, float x, float y ) {
    Mat4 transform, translation, scale; SetToIdentity( &translation ); SetToIdentity( &scale );
    SetScale( &scale, width, height, 1.0f  ); SetTranslation( &translation, x, y, 0.0f );
    transform = MultMatrix( scale, translation );

    glUseProgram( rendererStorage->texturedQuadShader.programID );
    glUniformMatrix4fv( rendererStorage->quadMat4UniformPtr, 1, false, (float*)&transform );

    //Set vertex data
    glBindBuffer( GL_ARRAY_BUFFER, rendererStorage->quadVDataPtr );
    glEnableVertexAttribArray( rendererStorage->quadPosAttribPtr );
    glVertexAttribPointer( rendererStorage->quadPosAttribPtr, 3, GL_FLOAT, GL_FALSE, 0, 0);

    //Set UV data
    glBindBuffer( GL_ARRAY_BUFFER, rendererStorage->quadUVDataPtr );
    glEnableVertexAttribArray( rendererStorage->quadUVAttribPtr );
    glVertexAttribPointer( rendererStorage->quadUVAttribPtr, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, (GLuint)texture );

    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, rendererStorage->quadIDataPtr );
    glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL );
}

void RenderDebugCircle( RendererStorage* rendererStorage, Vec3 position, float radius, Vec3 color ) {
    glUseProgram( rendererStorage->pShader.programID );

    Mat4 p; SetToIdentity( &p );
    SetTranslation( &p, position.x, position.y, position.z ); SetScale( &p, radius, radius, radius );
    assert( false );
    // glUniformMatrix4fv( rendererStorage->pShader.modelMatrixUniformPtr, 1, false, (float*)&p.m[0] );
    // glUniformMatrix4fv( rendererStorage->pShader.cameraMatrixUniformPtr, 1, false, (float*)&rendererStorage->cameraTransform.m[0] );
    // glUniform4f( glGetUniformLocation( rendererStorage->pShader.programID, "primitiveColor" ), color.x, color.y, color.z, 1.0f );

    // glEnableVertexAttribArray( rendererStorage->pShader.positionAttribute );
    // glBindBuffer( GL_ARRAY_BUFFER, rendererStorage->circleDataPtr );
    // glVertexAttribPointer( rendererStorage->pShader.positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, rendererStorage->circleIDataPtr );
    // glDrawElements( GL_TRIANGLE_FAN, 18, GL_UNSIGNED_INT, NULL );
}

void RenderDebugLines( RendererStorage* rendererStorage, float* vertexData, uint8 dataCount, Mat4 transform, Vec3 color ) {
    glUseProgram( rendererStorage->pShader.programID );

    assert(false);
    // glUniformMatrix4fv( rendererStorage->pShader.modelMatrixUniformPtr, 1, false, (float*)&transform.m[0] );
    // glUniformMatrix4fv( rendererStorage->pShader.cameraMatrixUniformPtr, 1, false, (float*)&rendererStorage->cameraTransform.m[0] );
    // glUniform4f( glGetUniformLocation( rendererStorage->pShader.programID, "primitiveColor" ), color.x, color.y, color.z, 1.0f );

    // glEnableVertexAttribArray( rendererStorage->pShader.positionAttribute );
    // glBindBuffer( GL_ARRAY_BUFFER, rendererStorage->lineDataPtr );
    // glBufferData( GL_ARRAY_BUFFER, dataCount * 3 * sizeof(float), (GLfloat*)vertexData, GL_DYNAMIC_DRAW );
    // glVertexAttribPointer( rendererStorage->pShader.positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, rendererStorage->lineIDataPtr );
    // glDrawElements( GL_LINES, dataCount, GL_UNSIGNED_INT, NULL );
}

void RenderArmatureAsLines( RendererStorage* rStorage, Armature* armature, Mat4 transform, Vec3 color ) {
    bool isDepthTesting;
    glGetBooleanv( GL_DEPTH_TEST, ( GLboolean* )&isDepthTesting );
    if( isDepthTesting ) {
        glDisable( GL_DEPTH_TEST );
    }

    uint8 dataCount = 0;
    uint8 jointData = 0;
    Vec3 boneLines [64];
    Vec3 jointXAxes [64];
    Vec3 jointYAxes [64];
    Vec3 jointZAxes [64];

    for( uint8 boneIndex = 0; boneIndex < armature->boneCount; ++boneIndex ) {
        Bone* bone = &armature->bones[ boneIndex ];

        Vec3 p1 = { 0.0f, 0.0f, 0.0f };
        p1 = MultVec( InverseMatrix( bone->invBindPose ), p1 );
        p1 = MultVec( *bone->currentTransform, p1 );
        RenderDebugCircle( rStorage, MultVec( transform, p1 ), 0.05f, { 1.0f, 1.0f, 1.0f } );

        if( bone->childCount > 0 ) {
            for( uint8 childIndex = 0; childIndex < bone->childCount; ++childIndex ) {
                Bone* child = bone->children[ childIndex ];

                Vec3 p2 = { 0.0f, 0.0f, 0.0f };
                p2 = MultVec( InverseMatrix( child->invBindPose ), p2 );
                p2 = MultVec( *child->currentTransform, p2 );

                boneLines[ dataCount++ ] = p1;
                boneLines[ dataCount++ ] = p2;
            }
        }

        Vec3 px = { 1.0f, 0.0f, 0.0f };
        Vec3 py = { 0.0f, 1.0f, 0.0f };
        Vec3 pz = { 0.0f, 0.0f, 1.0f };
        px = MultVec( InverseMatrix( bone->invBindPose ), px );
        px = MultVec( *bone->currentTransform, px );
        py = MultVec( InverseMatrix( bone->invBindPose ), py );
        py = MultVec( *bone->currentTransform, py );
        pz = MultVec( InverseMatrix( bone->invBindPose ), pz );
        pz = MultVec( *bone->currentTransform, pz );
        jointXAxes[ jointData ] = p1; 
        jointYAxes[ jointData ] = p1; 
        jointZAxes[ jointData ] = p1;
        jointData++;
        jointXAxes[ jointData ] = px;
        jointYAxes[ jointData ] = py;
        jointZAxes[ jointData ] = pz;
        jointData++;
    }

    RenderDebugLines( rStorage, (float*)&boneLines[0], dataCount, transform, color );
    RenderDebugLines( rStorage, (float*)&jointXAxes[0], jointData, transform, { 0.09, 0.85, 0.15 } );
    RenderDebugLines( rStorage, (float*)&jointYAxes[0], jointData, transform, { 0.85, 0.85, 0.14 } );
    RenderDebugLines( rStorage, (float*)&jointZAxes[0], jointData, transform, { 0.09, 0.11, 0.85 } );

    if( isDepthTesting ) {
        glEnable( GL_DEPTH_TEST );
    }
}
#endif