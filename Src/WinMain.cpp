#include <stdio.h>
#include <stdbool.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <XInput.h>
#include <assert.h>
#include <functional>

#include "tinyxml2/tinyxml2.h"
#include "tinyxml2/tinyxml2.cpp"

#define GLEW_STATIC
#include "OpenGL/glew.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb/stb_image.h"

#include "App.h"
#include "..\App.cpp"

//Win32 function prototypes, allows the entry point to be the first function
static LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
//A helper function only to be used in this file ( so far )
static void TextToNumberConversion( char* textIn, float* numbersOut );

static struct {
	HINSTANCE appInstance;
	HWND hwnd;
	WNDCLASSEX wc;
	HDC deviceContext;

	HGLRC openglRenderContext;
	LPRECT windowRect;
	LARGE_INTEGER timerResolution;

	uint16 windowPosX;
	uint16 windowPosY;
	bool running;
	bool isFullScreen;
	//I don't have a reason for this to be a generally available struct
	struct {
		float leftStick_x, leftStick_y;
		float rightStick_x, rightStick_y;
		float leftTrigger, rightTrigger;
		bool leftBumper, rightBumper;
		bool button1, button2, button3, button4;
		bool specialButtonLeft, specialButtonRight;
	} controllerState;

	int64 mSecsPerFrame;
} appInfo;

static int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	AllocConsole(); //create a console
	freopen( "conin$","r",stdin );
	freopen( "conout$","w",stdout );
	freopen( "conout$","w",stderr );
	printf( "Program Started, console initialized\n" );

	appInfo.appInstance = hInstance;

	const char WindowName[] = "Wet Clay";

	appInfo.running = true;
	appInfo.isFullScreen = false;
	appInfo.mSecsPerFrame = 16; //60FPS

	appInfo.wc.cbSize = sizeof(WNDCLASSEX);
	appInfo.wc.style = CS_OWNDC;
	appInfo.wc.lpfnWndProc = WndProc;
	appInfo.wc.cbClsExtra = 0;
	appInfo.wc.cbWndExtra = 0;
	appInfo.wc.hInstance = appInfo.appInstance;
	appInfo.wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	appInfo.wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	appInfo.wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	appInfo.wc.lpszMenuName = NULL;
	appInfo.wc.lpszClassName = WindowName;
	appInfo.wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if( !RegisterClassEx( &appInfo.wc ) ) {
		printf( "Failed to register class\n" );
		appInfo.running = false;
		return -1;
	}

	// center position of the window
	appInfo.windowPosX = (GetSystemMetrics(SM_CXSCREEN) / 2) - (SCREEN_WIDTH / 2);
	appInfo.windowPosY = (GetSystemMetrics(SM_CYSCREEN) / 2) - (SCREEN_HEIGHT / 2);
 
	// set up the window for a windowed application by default
	//long wndStyle = WS_OVERLAPPEDWINDOW;
 
	if( appInfo.isFullScreen ) {
		appInfo.windowPosX = 0;
		appInfo.windowPosY = 0;

		//change resolution before the window is created
		//SysSetDisplayMode(width, height, SCRDEPTH);
		//TODO: implement
	}
 
	// at this point WM_CREATE message is sent/received
	// the WM_CREATE branch inside WinProc function will execute here
	appInfo.hwnd = CreateWindowEx(0, WindowName, "Wet Clay App", WS_BORDER, appInfo.windowPosX, appInfo.windowPosY, 
		SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL, appInfo.appInstance, NULL);

	GetClientRect( appInfo.hwnd, appInfo.windowRect );

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof( PIXELFORMATDESCRIPTOR ),
		1,
	    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
		PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
		32,                       //Colordepth of the framebuffer.
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		32,                       //Number of bits for the depthbuffer
	    0,                        //Number of bits for the stencilbuffer
		0,                        //Number of Aux buffers in the framebuffer.
		0, 0, 0, 0, 0
	};

	appInfo.deviceContext = GetDC( appInfo.hwnd );

	int letWindowsChooseThisPixelFormat;
	letWindowsChooseThisPixelFormat = ChoosePixelFormat( appInfo.deviceContext, &pfd ); 
	SetPixelFormat( appInfo.deviceContext, letWindowsChooseThisPixelFormat, &pfd );

	appInfo.openglRenderContext = wglCreateContext( appInfo.deviceContext );
	if( wglMakeCurrent ( appInfo.deviceContext, appInfo.openglRenderContext ) == false ) {
		printf( "Couldn't make GL context current.\n" );
		return -1;
	}

	printf("Size of double:%d\n", sizeof( double ) );

	glewInit();

	MemorySlab gameSlab;
	gameSlab.slabSize = MEGABYTES( RESERVED_SPACE );
	gameSlab.slabStart = VirtualAlloc( NULL, gameSlab.slabSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
	assert( gameSlab.slabStart != NULL );
	gameSlab.current = gameSlab.slabStart;

	SlabSubsection_Stack systemsMemory = CarveNewSubsection( &gameSlab, KILOBYTES( 12 ) );
	SlabSubsection_Stack gameMemoryStack = CarveNewSubsection( &gameSlab, sizeof( GameMemory ) * 2 );
	void* gMemPtr = AllocOnSubStack_Aligned( &gameMemoryStack, sizeof( GameMemory ) );

	SoundSystemStorage* soundSystemStorage = Win32InitSound( appInfo.hwnd, 60, &systemsMemory );
	RendererStorage* renderSystemStorage = InitRenderer( SCREEN_WIDTH, SCREEN_HEIGHT, &systemsMemory );

	SetWindowLong( appInfo.hwnd, GWL_STYLE, 0 );
	ShowWindow ( appInfo.hwnd, SW_SHOWNORMAL );
	UpdateWindow( appInfo.hwnd );

	BOOL canSupportHiResTimer = QueryPerformanceFrequency( &appInfo.timerResolution );
	assert( canSupportHiResTimer );

	GameInit( &gameSlab, gMemPtr, renderSystemStorage );

	MSG Msg;
	do {
		while( PeekMessage( &Msg, NULL, 0, 0, PM_REMOVE ) ) {
			TranslateMessage( &Msg );
			DispatchMessage( &Msg );
		}

		static LARGE_INTEGER startTime;
		LARGE_INTEGER lastTime = startTime;
		QueryPerformanceCounter( &startTime );

		//GAME LOOP
		if(appInfo.running) {
			XINPUT_STATE state;
			DWORD queryResult;
			memset( &state, 0, sizeof( XINPUT_STATE ) ) ;
			queryResult = XInputGetState( 0, &state );
			if( queryResult == ERROR_SUCCESS ) {
				//Note: polling of the sticks results in the range not quite reaching 1.0 in the positive direction
				//it is like this to avoid branching on greater or less than 0.0
				appInfo.controllerState.leftStick_x = ((float)state.Gamepad.sThumbLX / 32768.0f );
				appInfo.controllerState.leftStick_y = ((float)state.Gamepad.sThumbLY / 32768.0f );
				appInfo.controllerState.rightStick_x = ((float)state.Gamepad.sThumbRX / 32768.0f );
				appInfo.controllerState.rightStick_y = ((float)state.Gamepad.sThumbRY / 32768.0f );
				appInfo.controllerState.leftTrigger = ((float)state.Gamepad.bLeftTrigger / 255.0f );
				appInfo.controllerState.rightTrigger = ((float)state.Gamepad.bRightTrigger / 255.0f );
				appInfo.controllerState.leftBumper = state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
				appInfo.controllerState.rightBumper = state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
				appInfo.controllerState.button1 = state.Gamepad.wButtons & XINPUT_GAMEPAD_A;
				appInfo.controllerState.button2 = state.Gamepad.wButtons & XINPUT_GAMEPAD_B;
				appInfo.controllerState.button3 = state.Gamepad.wButtons & XINPUT_GAMEPAD_X;
				appInfo.controllerState.button4 = state.Gamepad.wButtons & XINPUT_GAMEPAD_Y;
				appInfo.controllerState.specialButtonLeft = state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK;
				appInfo.controllerState.specialButtonRight = state.Gamepad.wButtons & XINPUT_GAMEPAD_START;
			} else {
				appInfo.controllerState = { };
			}

			LARGE_INTEGER elapsedTime;
			elapsedTime.QuadPart = startTime.QuadPart - lastTime.QuadPart;
			elapsedTime.QuadPart *= 1000;
			elapsedTime.QuadPart /= appInfo.timerResolution.QuadPart;

			appInfo.running = Update( gMemPtr, (float)elapsedTime.QuadPart, &soundSystemStorage->srb, soundSystemStorage->activeSounds );
			Render( gMemPtr, renderSystemStorage );

		    PushAudioToSoundCard( soundSystemStorage );

			SwapBuffers( appInfo.deviceContext );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		}

		LARGE_INTEGER endTime, computeTime;
		QueryPerformanceCounter( &endTime );
		computeTime.QuadPart = endTime.QuadPart - startTime.QuadPart;
		computeTime.QuadPart *= 1000;
		computeTime.QuadPart /= appInfo.timerResolution.QuadPart;
		if( computeTime.QuadPart <= appInfo.mSecsPerFrame ) {
			Sleep(appInfo.mSecsPerFrame - computeTime.QuadPart );
		} else {
			printf("Didn't sleep, compute was %ld\n", computeTime.QuadPart );
		}

	} while( appInfo.running );

	FreeConsole();

	return Msg.wParam;
}

/*----------------------------------------------------------------------------------------
                         App.h function prototype implementations
-----------------------------------------------------------------------------------------*/

void GetMousePosition( float* x, float* y ) {
	POINT mousePosition;
	GetCursorPos( &mousePosition );
	*x = ((float)(mousePosition.x - appInfo.windowPosX)) / ( (float)SCREEN_WIDTH / 2.0f ) - 1.0f;
	*y = (((float)(mousePosition.y - appInfo.windowPosY)) / ( (float)SCREEN_HEIGHT / 2.0f ) - 1.0f) * -1.0f;
}

bool IsKeyDown( uint8 keyChar ) {
	if( keyChar >= 'a' && keyChar <= 'z' ) {
		keyChar -= ( 'a' - 'A');
	}
	short keystate = GetAsyncKeyState( keyChar );

	return ( 1 << 16 ) & keystate;
}

bool IsControllerButtonDown( uint8 buttonIndex ) {
	if( buttonIndex == 0 )
	    return appInfo.controllerState.button1;
	else if( buttonIndex == 1 )
	    return appInfo.controllerState.button2;
	else if( buttonIndex == 2 )
	    return appInfo.controllerState.button3;
	else if( buttonIndex == 3 )
	    return appInfo.controllerState.button4;
	else if( buttonIndex == 4 )
		return appInfo.controllerState.leftBumper;
	else if( buttonIndex == 5 )
		return appInfo.controllerState.rightBumper;
	else if( buttonIndex == 6)
	    return appInfo.controllerState.specialButtonLeft;
	else if( buttonIndex == 7 )
	    return appInfo.controllerState.specialButtonRight;
}

void GetControllerStickState( uint8 stickIndex, float* x, float* y ) {
	if( stickIndex == 0 ) {
		*x = appInfo.controllerState.leftStick_x;
		*y = appInfo.controllerState.leftStick_y;
	} else if( stickIndex == 1 ) {
		*x = appInfo.controllerState.rightStick_x;
		*y = appInfo.controllerState.rightStick_y;
	}
}

float GetTriggerState( uint8 triggerIndex ) {

	if( triggerIndex == 0 ) {
		return appInfo.controllerState.leftTrigger;
	} else if( triggerIndex == 1) {
		return appInfo.controllerState.rightTrigger;
	}

	return 0.0f;
}

#include <strsafe.h>
void* ReadWholeFile( char* filename, int64* bytesRead ) {
	//Open
	HANDLE fileHandle;
	//TODO: look into other options, such as: Overlapped, No_Bufffering, and Random_Access
	fileHandle = CreateFile( filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	if( fileHandle == INVALID_HANDLE_VALUE ) {
		assert( false );
	}

	//Determine amount of data
	LARGE_INTEGER fileSize;
	GetFileSizeEx( fileHandle, &fileSize );
	if( fileSize.QuadPart == 0 ) {
		assert( false );
	}
	assert( fileSize.QuadPart <= 0xFFFFFFFF );

	//Reserve Space
	void* data = 0;
	data = malloc( fileSize.QuadPart );
	memset( data, 0, fileSize.QuadPart );
	assert( data != 0 );

	//Read data
	DWORD dataRead = 0;
	//TODO: adjust for overlapped or async stuff?
	BOOL readSuccess = ReadFile( fileHandle, data, fileSize.QuadPart, &dataRead, 0 );
	if( !readSuccess || dataRead != fileSize.QuadPart ) {

	}

	//Close
	BOOL closeReturnValue = CloseHandle( fileHandle );
	assert( closeReturnValue );

	*bytesRead = fileSize.QuadPart;
	return data;
}

/*----------------------------------------------------------------------------------------
                       Renderer.h function prototype implementations
-----------------------------------------------------------------------------------------*/

char* ReadShaderSrcFileFromDisk( const char* fileName ) {
	FILE* file;
    errno_t e = fopen_s(&file, fileName, "r" );
    if( file == NULL || e != 0) {
        printf( "Could not open file %s\n", fileName );
        return 0;
    }
    fseek( file, 0, SEEK_END );
    size_t fileSize = ftell( file );
    char* buffer = (char*)malloc( fileSize + 1 );
    memset( buffer, 0, fileSize + 1 );

    fseek( file, 0, SEEK_SET );
    fread( buffer , 1, fileSize, file );
    fclose( file );
    return buffer;
}

void LoadMeshDataFromDisk( const char* fileName,  SlabSubsection_Stack* allocater, MeshGeometryData* storage, Armature* armature ) {
	tinyxml2::XMLDocument colladaDoc;
	colladaDoc.LoadFile( fileName );
	//if( colladaDoc == NULL ) {
		//printf( "Could not load mesh: %s\n", fileName );
		//return;
	//}

	tinyxml2::XMLElement* meshNode = colladaDoc.FirstChildElement( "COLLADA" )->FirstChildElement( "library_geometries" )
	->FirstChildElement( "geometry" )->FirstChildElement( "mesh" );

	char* colladaTextBuffer = NULL;
	size_t textBufferLen = 0;

	uint16 vCount = 0;
	uint16 nCount = 0;
	uint16 uvCount = 0;
	uint16 indexCount = 0;
	float* rawColladaVertexData;
	float* rawColladaNormalData;
	float* rawColladaUVData;
	float* rawIndexData;
	///Basic Mesh Geometry Data
	{
		tinyxml2::XMLNode* colladaVertexArray = meshNode->FirstChildElement( "source" );
		tinyxml2::XMLElement* vertexFloatArray = colladaVertexArray->FirstChildElement( "float_array" );
		tinyxml2::XMLNode* colladaNormalArray = colladaVertexArray->NextSibling();
		tinyxml2::XMLElement* normalFloatArray = colladaNormalArray->FirstChildElement( "float_array" );
		tinyxml2::XMLNode* colladaUVMapArray = colladaNormalArray->NextSibling();
		tinyxml2::XMLElement* uvMapFloatArray = colladaUVMapArray->FirstChildElement( "float_array" );
		tinyxml2::XMLElement* meshSrc = meshNode->FirstChildElement( "polylist" );
		tinyxml2::XMLElement* colladaIndexArray = meshSrc->FirstChildElement( "p" );

		int count;
		const char* colladaVertArrayVal = vertexFloatArray->FirstChild()->Value();
		vertexFloatArray->QueryAttribute( "count", &count );
		vCount = count;
		const char* colladaNormArrayVal = normalFloatArray->FirstChild()->Value();
		normalFloatArray->QueryAttribute( "count", &count );
		nCount = count;
		const char* colladaUVMapArrayVal = uvMapFloatArray->FirstChild()->Value();
		uvMapFloatArray->QueryAttribute( "count", &count );
		uvCount = count;
		const char* colladaIndexArrayVal = colladaIndexArray->FirstChild()->Value();
		meshSrc->QueryAttribute( "count", &count );
		//Assume this is already triangulated
		indexCount = count * 3 * 3;

		///TODO: replace this with fmaxf?
		std::function< size_t (size_t, size_t) > sizeComparison = []( size_t size1, size_t size2 ) -> size_t {
			if( size1 >= size2 ) return size1;
			return size2;
		};

		textBufferLen = strlen( colladaVertArrayVal );
		textBufferLen = sizeComparison( strlen( colladaNormArrayVal ), textBufferLen );
		textBufferLen = sizeComparison( strlen( colladaUVMapArrayVal ), textBufferLen );
		textBufferLen = sizeComparison( strlen( colladaIndexArrayVal ), textBufferLen );
		colladaTextBuffer = (char*)alloca( textBufferLen );
		memset( colladaTextBuffer, 0, textBufferLen );
		rawColladaVertexData = (float*)alloca( sizeof(float) * vCount );
		rawColladaNormalData = (float*)alloca( sizeof(float) * nCount );
		rawColladaUVData = (float*)alloca( sizeof(float) * uvCount );
		rawIndexData = (float*)alloca( sizeof(float) * indexCount );

		memset( rawColladaVertexData, 0, sizeof(float) * vCount );
		memset( rawColladaNormalData, 0, sizeof(float) * nCount );
		memset( rawColladaUVData, 0, sizeof(float) * uvCount );
		memset( rawIndexData, 0, sizeof(float) * indexCount );

		//Reading Vertex position data
		strcpy( colladaTextBuffer, colladaVertArrayVal );
		TextToNumberConversion( colladaTextBuffer, rawColladaVertexData );

	    //Reading Normals data
		memset( colladaTextBuffer, 0, textBufferLen );
		strcpy( colladaTextBuffer, colladaNormArrayVal );
		TextToNumberConversion( colladaTextBuffer, rawColladaNormalData );

	    //Reading UV map data
		memset( colladaTextBuffer, 0, textBufferLen );
		strcpy( colladaTextBuffer, colladaUVMapArrayVal );
		TextToNumberConversion( colladaTextBuffer, rawColladaUVData );

	    //Reading index data
		memset( colladaTextBuffer, 0, textBufferLen );
		strcpy( colladaTextBuffer, colladaIndexArrayVal );
		TextToNumberConversion( colladaTextBuffer, rawIndexData );
	}

	float* rawBoneWeightData = NULL;
	float* rawBoneIndexData = NULL;
	//Skinning Data
	{
		tinyxml2::XMLElement* libControllers = colladaDoc.FirstChildElement( "COLLADA" )->FirstChildElement( "library_controllers" );
		if( libControllers == NULL ) goto skinningExit;
		tinyxml2::XMLElement* controllerElement = libControllers->FirstChildElement( "controller" );
		if( controllerElement == NULL ) goto skinningExit;

		tinyxml2::XMLNode* vertexWeightDataArray = controllerElement->FirstChild()->FirstChild()->NextSibling()->NextSibling()->NextSibling();
		tinyxml2::XMLNode* vertexBoneIndexDataArray = vertexWeightDataArray->NextSibling()->NextSibling();
		tinyxml2::XMLNode* vCountArray = vertexBoneIndexDataArray->FirstChildElement( "vcount" );
		tinyxml2::XMLNode* vArray = vertexBoneIndexDataArray->FirstChildElement( "v" );
		const char* boneWeightsData = vertexWeightDataArray->FirstChild()->FirstChild()->Value();
		const char* vCountArrayData = vCountArray->FirstChild()->Value();
		const char* vArrayData = vArray->FirstChild()->Value();

		float* colladaBoneWeightData = NULL;
		float* colladaBoneIndexData = NULL;
		float* colladaBoneInfluenceCounts = NULL;
		///This is overkill, Collada stores ways less data usually, plus this still doesn't account for very complex models 
		///(e.g, lots of verts with more than MAXBONESPERVERT influencing position )
		colladaBoneWeightData = (float*)alloca( sizeof(float) * MAXBONESPERVERT * vCount );
		colladaBoneIndexData = (float*)alloca( sizeof(float) * MAXBONESPERVERT * vCount );
		colladaBoneInfluenceCounts = (float*)alloca( sizeof(float) * MAXBONESPERVERT * vCount );

		//Read bone weights data
		memset( colladaTextBuffer, 0, textBufferLen );
		strcpy( colladaTextBuffer, boneWeightsData );
		TextToNumberConversion( colladaTextBuffer, colladaBoneWeightData );

		//Read bone index data
		memset( colladaTextBuffer, 0, textBufferLen );
		strcpy( colladaTextBuffer, vArrayData );
		TextToNumberConversion( colladaTextBuffer, colladaBoneIndexData );

		//Read bone influence counts
		memset( colladaTextBuffer, 0, textBufferLen );
		strcpy( colladaTextBuffer, vCountArrayData );
		TextToNumberConversion( colladaTextBuffer, colladaBoneInfluenceCounts );

		rawBoneWeightData = (float*)alloca( sizeof(float) * MAXBONESPERVERT * vCount );
		rawBoneIndexData = (float*)alloca( sizeof(float) * MAXBONESPERVERT * vCount );
		memset( rawBoneWeightData, 0, sizeof(float) * MAXBONESPERVERT * vCount );
		memset( rawBoneIndexData, 0, sizeof(float) * MAXBONESPERVERT * vCount );

		int colladaIndexIndirection = 0;
		int verticiesInfluenced = 0;
		vCountArray->Parent()->ToElement()->QueryAttribute( "count", &verticiesInfluenced );
		for( uint16 i = 0; i < verticiesInfluenced; i++ ) {
			uint8 influenceCount = colladaBoneInfluenceCounts[i];
			for( uint16 j = 0; j < influenceCount; j++ ) {
				uint16 boneIndex = colladaBoneIndexData[ colladaIndexIndirection++ ];
				uint16 weightIndex = colladaBoneIndexData[ colladaIndexIndirection++ ];
				rawBoneWeightData[ i * MAXBONESPERVERT + j ] = colladaBoneWeightData[ weightIndex ];
				rawBoneIndexData[ i * MAXBONESPERVERT + j ] = boneIndex;
			}
		}
	}
	skinningExit:

	//Armature
	if( armature != NULL ) {
		tinyxml2::XMLElement* visualScenesNode = colladaDoc.FirstChildElement( "COLLADA" )->FirstChildElement( "library_visual_scenes" )
		->FirstChildElement( "visual_scene" )->FirstChildElement( "node" );
		tinyxml2::XMLElement* armatureNode = NULL;

		//Step through scene heirarchy until start of armature is found
		while( visualScenesNode != NULL ) {
			if( visualScenesNode->FirstChildElement( "node" ) != NULL && 
				visualScenesNode->FirstChildElement( "node" )->Attribute( "type", "JOINT" ) != NULL ) {
				armatureNode = visualScenesNode;
			    break;
			} else {
				visualScenesNode = visualScenesNode->NextSibling()->ToElement();
			}
		}
		if( armatureNode == NULL ) return;

		//Parsing basic bone data from XML
		std::function< Bone* ( tinyxml2::XMLElement*, Armature*, Bone*  ) > ParseColladaBoneData = 
		[&]( tinyxml2::XMLElement* boneElement, Armature* armature, Bone* parentBone ) -> Bone* {
			Bone* bone = &armature->bones[ armature->boneCount ];
			bone->parent = parentBone;
			bone->currentTransform = &armature->boneTransforms[ armature->boneCount ];
			SetToIdentity( bone->currentTransform );
			bone->boneIndex = armature->boneCount;
			armature->boneCount++;

			strcpy( &bone->name[0], boneElement->Attribute( "sid" ) );

			float matrixData[16];
			char matrixTextData [512];
			tinyxml2::XMLNode* matrixElement = boneElement->FirstChildElement("matrix");
			strcpy( &matrixTextData[0], matrixElement->FirstChild()->ToText()->Value() );
			TextToNumberConversion( matrixTextData, matrixData );
			//Note: this is only local transform data, but its being saved in bind matrix for now
			Mat4 m;
			memcpy( &m.m[0][0], &matrixData[0], sizeof(float) * 16 );
			bone->bindPose = TransposeMatrix( m );

			if( parentBone == NULL ) {
				armature->rootBone = bone;
			} else {
				bone->bindPose = MultMatrix( parentBone->bindPose, bone->bindPose );
			}

			bone->childCount = 0;
			tinyxml2::XMLElement* childBoneElement = boneElement->FirstChildElement( "node" );
			while( childBoneElement != NULL ) {
				Bone* childBone = ParseColladaBoneData( childBoneElement, armature, bone );
				bone->children[ bone->childCount++ ] = childBone;
				tinyxml2::XMLNode* siblingNode = childBoneElement->NextSibling();
				if( siblingNode != NULL ) {
					childBoneElement = siblingNode->ToElement();
				} else {
					childBoneElement = NULL;
				}
			};

			return bone;
		};
		armature->boneCount = 0;
		tinyxml2::XMLElement* boneElement = armatureNode->FirstChildElement( "node" );
		ParseColladaBoneData( boneElement, armature, NULL );

		//Parse inverse bind pose data from skinning section of XML
		{
			tinyxml2::XMLElement* boneNamesSource = colladaDoc.FirstChildElement( "COLLADA" )->FirstChildElement( "library_controllers" )
			->FirstChildElement( "controller" )->FirstChildElement( "skin" )->FirstChildElement( "source" );
			tinyxml2::XMLElement* boneBindPoseSource = boneNamesSource->NextSibling()->ToElement();

			char* boneNamesLocalCopy = NULL;
			float* boneMatriciesData = (float*)alloca( sizeof(float) * 16 * armature->boneCount );
			const char* boneNameArrayData = boneNamesSource->FirstChild()->FirstChild()->Value();
			const char* boneMatrixTextData = boneBindPoseSource->FirstChild()->FirstChild()->Value();
			size_t nameDataLen = strlen( boneNameArrayData );
			size_t matrixDataLen = strlen( boneMatrixTextData );
			boneNamesLocalCopy = (char*)alloca( nameDataLen + 1 );
			memset( boneNamesLocalCopy, 0, nameDataLen + 1 );
			assert( textBufferLen > matrixDataLen );
			memcpy( boneNamesLocalCopy, boneNameArrayData, nameDataLen );
			memcpy( colladaTextBuffer, boneMatrixTextData, matrixDataLen );
			TextToNumberConversion( colladaTextBuffer, boneMatriciesData );
			char* nextBoneName = &boneNamesLocalCopy[0];
			for( uint8 matrixIndex = 0; matrixIndex < armature->boneCount; matrixIndex++ ) {
				Mat4 matrix;
				memcpy( &matrix.m[0], &boneMatriciesData[matrixIndex * 16], sizeof(float) * 16 );

				char boneName [32];
				char* boneNameEnd = nextBoneName;
				do {
					boneNameEnd++;
				} while( *boneNameEnd != ' ' && *boneNameEnd != 0 );
				size_t charCount = boneNameEnd - nextBoneName;
				memset( boneName, 0, sizeof( char ) * 32 );
				memcpy( boneName, nextBoneName, charCount );
				nextBoneName = boneNameEnd + 1;

				Bone* targetBone = NULL;
				for( uint8 boneIndex = 0; boneIndex < armature->boneCount; boneIndex++ ) {
					Bone* bone = &armature->bones[ boneIndex ];
					if( strcmp( bone->name, boneName ) == 0 ) {
						targetBone = bone;
						break;
					}
				}

				Mat4 correction;
				correction.m[0][0] = 1.0f; correction.m[0][1] = 0.0f; correction.m[0][2] = 0.0f; correction.m[0][3] = 0.0f;
				correction.m[1][0] = 0.0f; correction.m[1][1] = 0.0f; correction.m[1][2] = 1.0f; correction.m[1][3] = 0.0f;
				correction.m[2][0] = 0.0f; correction.m[2][1] = -1.0f; correction.m[2][2] = 0.0f; correction.m[2][3] = 0.0f;
				correction.m[3][0] = 0.0f; correction.m[3][1] = 0.0f; correction.m[3][2] = 0.0f; correction.m[3][3] = 1.0f;
				targetBone->invBindPose = TransposeMatrix( matrix );
				targetBone->invBindPose = MultMatrix( correction, targetBone->invBindPose );
			}
		}
	}

	//output to my version of storage
	storage->dataCount = 0;
	uint16 counter = 0;

	const uint32 vertCount = indexCount / 3;
	storage->vData = (Vec3*)AllocOnSubStack_Aligned( allocater, vertCount * sizeof( Vec3 ), 16 );
	storage->uvData = (float*)AllocOnSubStack_Aligned( allocater, vertCount * 2 * sizeof( float ), 4 );
	storage->normalData = (Vec3*)AllocOnSubStack_Aligned( allocater, vertCount * sizeof( Vec3 ), 4 );
	storage->iData = (uint32*)AllocOnSubStack_Aligned( allocater, vertCount * sizeof( uint32 ), 4 );
	if( rawBoneWeightData != NULL ) {
		storage->boneWeightData = (float*)AllocOnSubStack_Aligned( allocater, sizeof(float) * vertCount * MAXBONESPERVERT, 4 );
		storage->boneIndexData = (uint32*)AllocOnSubStack_Aligned( allocater, sizeof(uint32) * vertCount * MAXBONESPERVERT, 4 );
	} else {
		storage->boneWeightData = NULL;
		storage->boneIndexData = NULL;
	}

	while( counter < indexCount ) {
		Vec3 v, n;
		float uv_x, uv_y;

		uint16 vertIndex = rawIndexData[ counter++ ];
		uint16 normalIndex = rawIndexData[ counter++ ];
		uint16 uvIndex = rawIndexData[ counter++ ];

		v.x = rawColladaVertexData[ vertIndex * 3 + 0 ];
		v.z = -rawColladaVertexData[ vertIndex * 3 + 1 ];
		v.y = rawColladaVertexData[ vertIndex * 3 + 2 ];

		n.x = rawColladaNormalData[ normalIndex * 3 + 0 ];
		n.z = -rawColladaNormalData[ normalIndex * 3 + 1 ];
		n.y = rawColladaNormalData[ normalIndex * 3 + 2 ];

		uv_x = rawColladaUVData[ uvIndex * 2 ];
		uv_y = rawColladaUVData[ uvIndex * 2 + 1 ];

		///TODO: check for exact copies of data, use to index to first instance instead
		uint32 storageIndex = storage->dataCount;
		storage->vData[ storageIndex ] = v;
		storage->normalData[ storageIndex ] = n;
		storage->uvData[ storageIndex * 2 ] = uv_x;
		storage->uvData[ storageIndex * 2 + 1 ] = uv_y;
		if( rawBoneWeightData != NULL ) {
			uint16 boneDataIndex = storage->dataCount * MAXBONESPERVERT;
			uint16 boneVertexIndex = vertIndex * MAXBONESPERVERT;
			for( uint8 i = 0; i < MAXBONESPERVERT; i++ ) {
				storage->boneWeightData[ boneDataIndex + i ] = rawBoneWeightData[ boneVertexIndex + i ];
				storage->boneIndexData[ boneDataIndex + i ] = rawBoneIndexData[ boneVertexIndex + i ];
			}
		}
		storage->iData[ storageIndex ] = storageIndex;
		storage->dataCount++;
	};
}

void LoadAnimationDataFromCollada( const char* fileName, ArmatureKeyFrame* keyframe, Armature* armature ) {
	tinyxml2::XMLDocument colladaDoc;
	colladaDoc.LoadFile( fileName );

	tinyxml2::XMLNode* animationNode = colladaDoc.FirstChildElement( "COLLADA" )->FirstChildElement( "library_animations" )->FirstChild();
	while( animationNode != NULL ) {
		//Desired data: what bone, and what local transform to it occurs
		Mat4 boneLocalTransform;
		Bone* targetBone = NULL;

		//Parse the target attribute from the XMLElement for channel, and get the bone it corresponds to
		const char* transformName = animationNode->FirstChildElement( "channel" )->Attribute( "target" );
		size_t nameLen = strlen( transformName );
		char* transformNameCopy = (char*)alloca( nameLen );
		strcpy( transformNameCopy, transformName );
		char* nameEnd = transformNameCopy;
		while( *nameEnd != '/' && *nameEnd != 0 ) {
			nameEnd++;
		}
		memset( transformNameCopy, 0, nameLen );
		nameLen = nameEnd - transformNameCopy;
		memcpy( transformNameCopy, transformName, nameLen );
		for( uint8 boneIndex = 0; boneIndex < armature->boneCount; boneIndex++ ) {
			if( strcmp( armature->bones[ boneIndex ].name, transformNameCopy ) == 0 ) {
				targetBone = &armature->bones[ boneIndex ];
				break;
			}
		}

		//Parse matrix data, and extract first keyframe data
		tinyxml2::XMLNode* transformMatrixElement = animationNode->FirstChild()->NextSibling();
		const char* matrixTransformData = transformMatrixElement->FirstChild()->FirstChild()->Value();
		size_t transformDataLen = strlen( matrixTransformData ) + 1;
		char* transformDataCopy = (char*)alloca( transformDataLen * sizeof( char ) );
		memset( transformDataCopy, 0, transformDataLen );
		memcpy( transformDataCopy, matrixTransformData, transformDataLen );
		int count = 0; 
		transformMatrixElement->FirstChildElement()->QueryAttribute( "count", &count );
		float* rawTransformData = (float*)alloca(  count * sizeof(float) );
		memset( rawTransformData, 0, count * sizeof(float) );
		TextToNumberConversion( transformDataCopy, rawTransformData );
		memcpy( &boneLocalTransform.m[0][0], &rawTransformData[0], 16 * sizeof(float) );

		//Save data in BoneKeyFrame struct
		boneLocalTransform = TransposeMatrix( boneLocalTransform );
		if( targetBone == armature->rootBone ) {
			Mat4 correction;
			correction.m[0][0] = 1.0f; correction.m[0][1] = 0.0f; correction.m[0][2] = 0.0f; correction.m[0][3] = 0.0f;
			correction.m[1][0] = 0.0f; correction.m[1][1] = 0.0f; correction.m[1][2] = -1.0f; correction.m[1][3] = 0.0f;
			correction.m[2][0] = 0.0f; correction.m[2][1] = 1.0f; correction.m[2][2] = 0.0f; correction.m[2][3] = 0.0f;
			correction.m[3][0] = 0.0f; correction.m[3][1] = 0.0f; correction.m[3][2] = 0.0f; correction.m[3][3] = 1.0f;
			boneLocalTransform = MultMatrix( boneLocalTransform, correction );
		}
		BoneKeyFrame* key = &keyframe->targetBoneTransforms[ targetBone->boneIndex ];
		key->combinedMatrix = boneLocalTransform;
		DecomposeMat4( boneLocalTransform, &key->scale, &key->rotation, &key->translation );
		Mat4 m = Mat4FromComponents( key->scale, key->rotation, key->translation );

		animationNode = animationNode->NextSibling();
	}

	//Pre multiply bones with parents to save doing it during runtime
	struct {
		ArmatureKeyFrame* keyframe;
		void PremultiplyKeyFrame( Bone* target, Mat4 parentTransform ) {
			BoneKeyFrame* boneKey = &keyframe->targetBoneTransforms[ target->boneIndex ];
			Mat4 netMatrix = MultMatrix( boneKey->combinedMatrix, parentTransform );

			for( uint8 boneIndex = 0; boneIndex < target->childCount; boneIndex++ ) {
				PremultiplyKeyFrame( target->children[ boneIndex ], netMatrix );
			}
			boneKey->combinedMatrix = netMatrix;
			DecomposeMat4( boneKey->combinedMatrix, &boneKey->scale, &boneKey->rotation, &boneKey->translation );
		}
	}LocalRecursiveScope;
	LocalRecursiveScope.keyframe = keyframe;
	Mat4 i; SetToIdentity( &i );
	LocalRecursiveScope.PremultiplyKeyFrame( armature->rootBone, i );
}

void LoadTextureDataFromDisk( const char* fileName, TextureData* storage ) {
    storage->data = (uint8*)stbi_load( fileName, (int*)&storage->width, (int*)&storage->height, (int*)&storage->channelsPerPixel, 0 );
    if( storage->data == NULL ) {
        printf( "Could not load file: %s\n", fileName );
    }
    printf( "Loaded file: %s\n", fileName );
    printf( "Width: %d, Height: %d, Channel count: %d\n", storage->width, storage->height, storage->channelsPerPixel );
}

/*----------------------------------------------------------------------------------------
                       Local Functions only to be used in this file
------------------------------------------------------------------------------------------*/

static void TextToNumberConversion( char* textIn, float* numbersOut ) {
	char* start;
	char* end; 
	start = textIn; 
	end = start;
	uint16 index = 0;
	do {
		do {
			end++;
		} while( *end != ' ' && *end != 0 );
		assert( ( end - start) < 16 );
		char buffer [16];
		memcpy( &buffer[0], start, end - start);
		buffer[end - start] = 0;
		start = end;

		numbersOut[index] = atof( buffer );
		index++;
	} while( *end != 0 );
}

static LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	switch (uMsg) {
		case WM_CLOSE: {
			appInfo.running = false;
			wglMakeCurrent( appInfo.deviceContext, NULL );
			wglDeleteContext( appInfo.openglRenderContext );
			ReleaseDC( appInfo.hwnd, appInfo.deviceContext );
			DestroyWindow( appInfo.hwnd );
			break;
		}

		case WM_DESTROY: {
			appInfo.running = false;
			wglMakeCurrent( appInfo.deviceContext, NULL );
			wglDeleteContext( appInfo.openglRenderContext );
			ReleaseDC( appInfo.hwnd, appInfo.deviceContext );
			PostQuitMessage( 0 );
			break;
		}
	}

	// Pass All Unhandled Messages To DefWindowProc
	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}