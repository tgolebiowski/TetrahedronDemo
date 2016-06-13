#define KILOBYTES(value) value * 1024
#define MEGABYTES(value) KILOBYTES(value) * 1024
#define GIGABYTES(value) MEGABYTES(value) * 1024

struct MemorySlab {
	void* slabStart;
	void* current;
	uint64 slabSize;
};

struct SlabSubsection_Stack {
	void* start;
	void* end;
	void* current;
};

SlabSubsection_Stack CarveNewSubsection( MemorySlab* slab, uint64 bytes ) {
	SlabSubsection_Stack slabSub = { 0, 0, 0 };
	ptrdiff_t bytesLeft = slab->slabSize - ((uintptr)slab->current - (uintptr)slab->slabStart);
	if( bytesLeft > bytes ) {
		slabSub.start = slab->current;
		slabSub.current = slab->current;
		slab->current = (void*)( (intptr)slab->current + bytes );
		slabSub.end = slab->current;
	}
	return slabSub;
}

void* AllocOnSubStack( SlabSubsection_Stack* subStack, uint64 sizeInBytes ) {
	//Check how much space is left in the sub stack
	ptrdiff_t bytesLeft = (intptr)subStack->end - (intptr)subStack->current;
	//Allocate if there's enough space left
	if( bytesLeft > sizeInBytes ) {
		void* returnValue = subStack->current;
		subStack->current = (void*)( (intptr)subStack->current + sizeInBytes );
		return returnValue;
	} else {
		return NULL;
	}
}

//Naive implementation based on examples in "Game Engine Architechture"
void* AllocOnSubStack_Aligned( SlabSubsection_Stack* subStack, uint64 size, uint8 alignment = 8 ) {
	//Add some padding so we have enough room to align
	uint64 expandedSize = size + alignment;

	//Determine how "off" the basic allocated pointer is
	uintptr rawAddress = (uintptr)AllocOnSubStack( subStack, expandedSize );
	uintptr alignmentMask = ( alignment - 1 );
	uintptr misalignment = ( rawAddress & alignmentMask );

	//Adjust the pointer to be aligned and return that
	uintptr adjustment = alignment - misalignment;
	uintptr alignedAddress = rawAddress + adjustment;
	return (void*)alignedAddress;
}

void ClearSubStack( SlabSubsection_Stack* subStack ) {
	subStack->current = subStack->start;
}

void FreeStub( void* ptr ) {

}

void ReallocStub( void* ptr ) {
	
}