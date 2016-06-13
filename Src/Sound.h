struct SoundSystemStorage;

struct LoadedSound {
	int32 sampleCount;
	int32 channelCount;
	int16* samples [2];
};

LoadedSound LoadWaveFile( char* filePath );

struct SoundRenderBuffer {
	int32 samplesPerSecond;
	int32 samplesToWrite;
	int16* samples;
};

#define MAXSOUNDSATONCE 16
struct PlayingSound {
	LoadedSound* baseSound;
	uint32 lastPlayLocation;
};

PlayingSound* QueueLoadedSound( LoadedSound* sound, PlayingSound* activeSoundList ) {
	for( uint8 soundIndex = 0; soundIndex < MAXSOUNDSATONCE; ++soundIndex ) {
		if( activeSoundList[ soundIndex ].baseSound == NULL ) {
			activeSoundList[ soundIndex ].baseSound = sound;
			activeSoundList[ soundIndex ].lastPlayLocation = 0;
			return &activeSoundList[ soundIndex ];
		}
	}

	return NULL;
}

void OutputTestTone( SoundRenderBuffer* srb, int hz = 440, int volume = 3000 ) {
	const int WavePeriod = srb->samplesPerSecond / hz;
	static float tSine = 0.0f;
	const float tSineStep = 2.0f * PI / (float)WavePeriod;

	int32 samplesToWrite = srb->samplesToWrite / 2;

	for( int32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex ) {
		tSine += tSineStep;
		if( tSine > ( 2.0f * PI ) ) {
			tSine -= ( 2.0f * PI );
		}

		int16 sampleValue = volume * sinf( tSine );
		int32 i = sampleIndex * 2;
		srb->samples[ i ] += sampleValue; //Left Channel
		srb->samples[ i + 1 ] += sampleValue; //Right Channel
	}
}

void MixSound( SoundRenderBuffer* srb, PlayingSound* activeSoundList ) {
	for( uint8 soundIndex = 0; soundIndex < MAXSOUNDSATONCE; ++soundIndex ) {
		if( activeSoundList[ soundIndex ].baseSound != NULL ) {
			PlayingSound* activeSound = &activeSoundList[ soundIndex ];

			uint32 samplesToWrite = srb->samplesToWrite / 2;
			uint32 samplesLeftInSound = activeSound->baseSound->sampleCount - activeSound->lastPlayLocation;
			if( samplesLeftInSound < samplesToWrite ) {
				samplesToWrite = samplesLeftInSound;
			}
			for( int32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex ) {
				int16 value = activeSound->baseSound->samples[0][ ( sampleIndex + activeSound->lastPlayLocation ) ];

				int32 i = sampleIndex * 2;

		        srb->samples[ i ] += value;     //Left Channel
		        srb->samples[ i + 1 ] += value; //Right Channel
		    }
		    activeSound->lastPlayLocation += samplesToWrite;

			if( activeSound->lastPlayLocation >= activeSound->baseSound->sampleCount ) {
				activeSound->baseSound = NULL;
				activeSound->lastPlayLocation = 0;
			}
		}
	}
}

#ifdef WIN32_ENTRY
#include <mmsystem.h>
#include <dsound.h>
#include <comdef.h>

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name( LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter )
typedef DIRECT_SOUND_CREATE( direct_sound_create );

struct SoundSystemStorage {
	int32 writeBufferSize;
	int32 safetySampleBytes;
	int32 expectedBytesPerFrame;
	uint8 bytesPerSample;
	LPDIRECTSOUNDBUFFER writeBuffer;

	uint64 runningSampleIndex;
	DWORD bytesToWrite, byteToLock;

	PlayingSound* activeSounds;
	SoundRenderBuffer srb;
};

SoundSystemStorage* Win32InitSound( HWND hwnd, int targetGameHZ, SlabSubsection_Stack* systemStorage ) {
	const int32 SamplesPerSecond = 48000;
	const int32 BufferSize = SamplesPerSecond * sizeof( int16 ) * 2;
	HMODULE DirectSoundDLL = LoadLibraryA( "dsound.dll" );

	SoundSystemStorage* soundSystemStorage = (SoundSystemStorage*)AllocOnSubStack_Aligned( systemStorage, sizeof( SoundSystemStorage ), 8 );

	//TODO: logging on all potential failure points
	if( DirectSoundDLL ) {
		direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress( DirectSoundDLL, "DirectSoundCreate" );

		LPDIRECTSOUND directSound;
		if( DirectSoundCreate && SUCCEEDED( DirectSoundCreate( 0, &directSound, 0 ) ) ) {

			WAVEFORMATEX waveFormat = {};
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nChannels = 2;
			waveFormat.nSamplesPerSec = SamplesPerSecond;
			waveFormat.wBitsPerSample = 16;
			waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
			waveFormat.cbSize = 0;

			if( SUCCEEDED( directSound->SetCooperativeLevel( hwnd, DSSCL_PRIORITY ) ) ) {

				DSBUFFERDESC bufferDescription = {};
				bufferDescription.dwSize = sizeof( bufferDescription );
				bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				LPDIRECTSOUNDBUFFER primaryBuffer;
				if( SUCCEEDED( directSound->CreateSoundBuffer( &bufferDescription, &primaryBuffer, 0 ) ) ) {
					if( !SUCCEEDED( primaryBuffer->SetFormat( &waveFormat ) ) ) {
						printf("Couldn't set format primary buffer\n");
					}
				} else {
					printf("Couldn't secure primary buffer\n");
				}
			} else {
				printf("couldn't set CooperativeLevel\n");
			}

			//For the secondary buffer (why do we need that again?)
			DSBUFFERDESC bufferDescription = {};
			bufferDescription.dwSize = sizeof( bufferDescription );
			bufferDescription.dwFlags = 0; //DSBCAPS_PRIMARYBUFFER;
			bufferDescription.dwBufferBytes = BufferSize;
			bufferDescription.lpwfxFormat = &waveFormat;
			HRESULT result = directSound->CreateSoundBuffer( &bufferDescription, &soundSystemStorage->writeBuffer, 0 );
			if( !SUCCEEDED( result ) ) {
				printf("Couldn't secure a writable buffer\n");
			}

			soundSystemStorage->byteToLock = 0;
			soundSystemStorage->bytesToWrite = 0;
			soundSystemStorage->writeBufferSize = BufferSize;
			soundSystemStorage->bytesPerSample = sizeof( int16 );
 			soundSystemStorage->safetySampleBytes = ( ( SamplesPerSecond / targetGameHZ ) / 2 ) * soundSystemStorage->bytesPerSample;
 			soundSystemStorage->expectedBytesPerFrame = ( 48000 * soundSystemStorage->bytesPerSample * 2 ) / targetGameHZ;
			soundSystemStorage->runningSampleIndex = 0;

			soundSystemStorage->srb.samplesPerSecond = SamplesPerSecond;
			soundSystemStorage->srb.samplesToWrite = BufferSize / sizeof( int16 );
			soundSystemStorage->srb.samples = (int16*)malloc( BufferSize );
			memset( soundSystemStorage->srb.samples, 0, BufferSize );

			size_t playingInfoBufferSize = sizeof( PlayingSound ) * MAXSOUNDSATONCE;
			soundSystemStorage->activeSounds = (PlayingSound*)malloc( playingInfoBufferSize );
			memset( soundSystemStorage->activeSounds, 0, playingInfoBufferSize );

			HRESULT playResult = soundSystemStorage->writeBuffer->Play( 0, 0, DSBPLAY_LOOPING );
			if( !SUCCEEDED( playResult ) ) {
				printf("failed to play\n");
			}

		} else {
			printf("couldn't create direct sound object\n");
		}
	} else {
		printf("couldn't create direct sound object\n");
	}

	return soundSystemStorage;
}

void PushAudioToSoundCard( SoundSystemStorage* soundSystemStorage ) {
	memset( soundSystemStorage->srb.samples, 0, soundSystemStorage->writeBufferSize );

	//Setup info needed for writing (where to, how much, etc.)
	DWORD playCursorPosition, writeCursorPosition;
	if( SUCCEEDED( soundSystemStorage->writeBuffer->GetCurrentPosition( &playCursorPosition, &writeCursorPosition) ) ) {
		static bool firstTime = true;
		if( firstTime ) {
			soundSystemStorage->runningSampleIndex = writeCursorPosition / soundSystemStorage->bytesPerSample;
			firstTime = false;
		}

	    //Pick up where we left off
		soundSystemStorage->byteToLock = ( soundSystemStorage->runningSampleIndex * soundSystemStorage->bytesPerSample * 2 ) % soundSystemStorage->writeBufferSize;

	    //Calculate how much to write
		DWORD ExpectedFrameBoundaryByte = playCursorPosition + soundSystemStorage->expectedBytesPerFrame;

		//DSound can be latent sometimes when reporting the current writeCursor, so this is 
		//the farthest ahead that the cursor could possibly be
		DWORD safeWriteCursor = writeCursorPosition;
		if( safeWriteCursor < playCursorPosition ) {
			safeWriteCursor += soundSystemStorage->writeBufferSize;
		}
		safeWriteCursor += soundSystemStorage->safetySampleBytes;

		bool AudioCardIsLowLatency = safeWriteCursor < ExpectedFrameBoundaryByte;

		//Determine up to which byte we should write
		DWORD targetCursor = 0;
		if( AudioCardIsLowLatency ) {
			targetCursor = ( ExpectedFrameBoundaryByte + soundSystemStorage->expectedBytesPerFrame );
		} else {
			targetCursor = ( writeCursorPosition + soundSystemStorage->expectedBytesPerFrame + soundSystemStorage->safetySampleBytes );
		}
		targetCursor = targetCursor % soundSystemStorage->writeBufferSize;

		//Wrap up on math, how many bytes do we actually write
		if( soundSystemStorage->byteToLock > targetCursor ) {
			soundSystemStorage->bytesToWrite = soundSystemStorage->writeBufferSize - soundSystemStorage->byteToLock;
			soundSystemStorage->bytesToWrite += targetCursor;
		} else {
			soundSystemStorage->bytesToWrite = targetCursor - soundSystemStorage->byteToLock;
		}

		if( soundSystemStorage->bytesToWrite == 0 ) {
			printf( "BTW is zero, BTL:%lu, TC:%lu, PC:%lu, WC:%lu \n", soundSystemStorage->byteToLock, targetCursor, playCursorPosition, writeCursorPosition );
		}

	    //Save number of samples that can be written to platform independent struct
		soundSystemStorage->srb.samplesToWrite = soundSystemStorage->bytesToWrite / soundSystemStorage->bytesPerSample;
	} else {
		printf("couldn't get cursor\n");
		return;
	}

	//Mix together currently playing sounds
	MixSound( &soundSystemStorage->srb, soundSystemStorage->activeSounds );

	//Push mixed sounds to the actual card
	VOID* region0;
	DWORD region0Size;
	VOID* region1;
	DWORD region1Size;

	HRESULT result = soundSystemStorage->writeBuffer->Lock( soundSystemStorage->byteToLock, soundSystemStorage->bytesToWrite,
		&region0, &region0Size,
		&region1, &region1Size,
		0 );
	if( !SUCCEEDED( result ) ) {
		_com_error error( result );
		LPCTSTR errMessage = error.ErrorMessage();
		//printf( "Couldn't Lock Sound Buffer\n" );
		printf( "Failed To Lock Buffer %s \n", errMessage );
		//printf( "BTL:%lu BTW:%lu r0:%lu r0s:%lu r1:%lu r1s:%lu\n", soundSystemStorage->byteToLock, 
		//	soundSystemStorage->bytesToWrite, region0, region0Size, region1, region1Size );
		//printf( "SoundRenderBuffer Info: Samples--%d\n", soundSystemStorage->srb.samplesToWrite );
		return;
	}

	int16* sampleSrc = soundSystemStorage->srb.samples;
	int16* sampleDest = (int16*)region0;
	DWORD region0SampleCount = region0Size / ( soundSystemStorage->bytesPerSample * 2 );
	for( DWORD sampleIndex = 0; sampleIndex < region0SampleCount; ++sampleIndex ) {
		*sampleDest++ = *sampleSrc++;
		*sampleDest++ = *sampleSrc++;
		++soundSystemStorage->runningSampleIndex;
	}
	sampleDest = (int16*)region1;
	DWORD region1SampleCount = region1Size / ( soundSystemStorage->bytesPerSample * 2 );
	for( DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex ) {
		*sampleDest++ = *sampleSrc++;
		*sampleDest++ = *sampleSrc++;
		++soundSystemStorage->runningSampleIndex;
	}

	//memset( soundSystemStorage->srb.samples, 0, sizeof( int16 ) * 2 * soundSystemStorage->srb.samplesToWrite );
	//soundSystemStorage->srb.samplesToWrite = 0;

	result = soundSystemStorage->writeBuffer->Unlock( region0, region0Size, region1, region1Size );
	if( !SUCCEEDED( result ) ) {
		printf("Couldn't Unlock\n");
	}
}

LoadedSound LoadWaveFile( char* filePath ) {
	#pragma pack( push, 1 )
	struct WaveHeader{
		uint32 RIFFID;
		uint32 size;
		uint32 WAVEID;
	};

    #define RIFF_CODE( a, b, c, d ) ( ( (uint32)(a) << 0 ) | ( (uint32)(b) << 8 ) | ( (uint32)(c) << 16 ) | ( (uint32)(d) << 24 ) )
	enum {
		WAVE_ChunkID_fmt = RIFF_CODE( 'f', 'm', 't', ' ' ),
		WAVE_ChunkID_data = RIFF_CODE( 'd', 'a', 't', 'a' ),
		WAVE_ChunkID_RIFF = RIFF_CODE( 'R', 'I', 'F', 'F' ),
		WAVE_ChunkID_WAVE = RIFF_CODE( 'W', 'A', 'V', 'E' )
	};

	struct WaveChunk {
		uint32 ID;
		uint32 size;
	};

	struct Wave_fmt {
		uint16 wFormatTag;
		uint16 nChannels;
		uint32 nSamplesPerSec;
		uint32 nAvgBytesPerSec;
		uint16 nBlockAlign;
		uint16 wBitsPerSample;
		uint16 cbSize;
		uint16 wValidBitsPerSample;
		uint32 dwChannelMask;
		uint8 SubFormat [8];
	};
    #pragma pack( pop )

	LoadedSound result = { };

	int64 bytesRead = 0;
	void* fileData = ReadWholeFile( filePath, &bytesRead );

	if( bytesRead > 0 ) {
		struct RiffIterator {
			uint8* currentByte;
			uint8* stop;
		};

		auto ParseChunkAt = []( WaveHeader* header, void* stop ) -> RiffIterator {
			return { (uint8*)header, (uint8*)stop };
		};
		auto IsValid = []( RiffIterator iter ) -> bool  {
			return iter.currentByte < iter.stop;
		};
		auto NextChunk = []( RiffIterator iter ) -> RiffIterator {
			WaveChunk* chunk = (WaveChunk*)iter.currentByte;
			//This is for alignment: ( ptr + ( targetalignment - 1 ) & ~( targetalignment - 1)) aligns ptr with the correct bit padding
			uint32 size = ( chunk->size + 1 ) & ~1;
			iter.currentByte += sizeof( WaveChunk ) + size;
			return iter;
		};
		auto GetChunkData = []( RiffIterator iter ) -> void* {
			void* result = ( iter.currentByte  + sizeof( WaveChunk ) );
			return result;
		};
		auto GetType = []( RiffIterator iter ) -> uint32 {
			WaveChunk* chunk = (WaveChunk*)iter.currentByte;
			uint32 result = chunk->ID;
			return result;
		};
		auto GetChunkSize = []( RiffIterator iter) -> uint32 {
			WaveChunk* chunk = (WaveChunk*)iter.currentByte;
			uint32 result = chunk->size;
			return result;
		};

		WaveHeader* header = (WaveHeader*)fileData;
		assert( header->RIFFID == WAVE_ChunkID_RIFF );
		assert( header->WAVEID == WAVE_ChunkID_WAVE );

		uint32 channelCount = 0;
		uint32 sampleDataSize = 0;
		void* sampleData = 0;
		for( RiffIterator iter = ParseChunkAt( header + 1, (uint8*)( header + 1 ) + header->size - 4 ); 
			IsValid( iter ); iter = NextChunk( iter ) ) {
			switch( GetType( iter ) ) {
				case WAVE_ChunkID_fmt: {
					Wave_fmt* fmt = (Wave_fmt*)GetChunkData( iter );
					assert( fmt->wFormatTag == 1 ); //NOTE: only supporting PCM
					assert( fmt->nSamplesPerSec == 48000 );
					assert( fmt->wBitsPerSample == 16 );
					channelCount = fmt->nChannels;
				}break;
				case WAVE_ChunkID_data: {
					sampleData = GetChunkData( iter );
					sampleDataSize = GetChunkSize( iter );
				}break;
			}
		}

		assert( sampleData != 0 );

		result.sampleCount = sampleDataSize / ( channelCount * sizeof( uint16 ) );
		result.channelCount = channelCount;

		if( channelCount == 1 ) {
			result.samples[0] = (int16*)sampleData;
			result.samples[1] = 0;
		} else if( channelCount == 2 ) {

		} else {
			assert(false);
		}
	}
	
	return result;
}
#endif //WIN32 specific implementation