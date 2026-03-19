#include "StaticBuffer.h"

// these are the static member fields declared in StaticBuffer.h
// they need to be explicitly defined here outside the class
// blocks[][] is the actual buffer - 32 slots, each holding one full disk block (2048 bytes)
unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
// metainfo[] stores metadata about each of the 32 buffer slots
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer()
{
    // at startup, all 32 buffer slots are empty/free
    // so we mark all of them as free
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        metainfo[bufferIndex].free = true;
    }
}

// we are not implementing write-back yet (that comes in stage 6)
// so destructor is empty for now
StaticBuffer::~StaticBuffer() {}

// finds a free slot in the buffer for a given disk block
// returns the index of the free slot, or E_OUTOFBOUND if blockNum is invalid
int StaticBuffer::getFreeBuffer(int blockNum)
{
    // check if the block number is valid
    if (blockNum < 0 || blockNum > DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    int allocatedBuffer = -1;

    // scan through all 32 buffer slots to find a free one
    for (int i = 0; i < BUFFER_CAPACITY; i++)
    {
        if (metainfo[i].free)
        {
            allocatedBuffer = i;
            break; // found a free slot, stop searching
        }
    }

    // mark this slot as occupied and remember which disk block it holds
    metainfo[allocatedBuffer].free = false;
    metainfo[allocatedBuffer].blockNum = blockNum;

    return allocatedBuffer;
}

// checks if a disk block is already loaded in the buffer
// returns the buffer index if found, or E_BLOCKNOTINBUFFER if not found
int StaticBuffer::getBufferNum(int blockNum)
{
    // check if the block number is valid
    if (blockNum < 0 || blockNum > DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    // scan through all 32 buffer slots
    for (int i = 0; i < BUFFER_CAPACITY; i++)
    {
        // check if this slot is occupied AND holds the block we're looking for
        if (metainfo[i].free == false && metainfo[i].blockNum == blockNum)
        {
            return i; // found it! return the buffer index
        }
    }

    // block is not in the buffer
    return E_BLOCKNOTINBUFFER;
}