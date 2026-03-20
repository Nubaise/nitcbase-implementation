#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer()
{
    // initialise all buffer slots
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }
}

// on system exit, write back all dirty blocks to disk
StaticBuffer::~StaticBuffer()
{
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        // if slot is occupied and dirty, write it back to disk
        if (metainfo[bufferIndex].free == false &&
            metainfo[bufferIndex].dirty == true)
        {
            Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
        }
    }
}

// finds a free buffer slot using LRU replacement if needed
int StaticBuffer::getFreeBuffer(int blockNum)
{
    if (blockNum < 0 || blockNum >= DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    // increment timestamp of all occupied buffer slots
    // this tracks how long since each block was last used
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if (metainfo[bufferIndex].free == false)
        {
            metainfo[bufferIndex].timeStamp++;
        }
    }

    int allocatedBuffer = -1;

    // first try to find a free slot
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if (metainfo[bufferIndex].free)
        {
            allocatedBuffer = bufferIndex;
            break;
        }
    }

    // if no free slot found, use LRU — find the slot with highest timestamp
    if (allocatedBuffer == -1)
    {
        int maxTimestamp = -1;

        for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
        {
            if (metainfo[bufferIndex].timeStamp > maxTimestamp)
            {
                maxTimestamp = metainfo[bufferIndex].timeStamp;
                allocatedBuffer = bufferIndex;
            }
        }

        // if the LRU block is dirty, write it back to disk before replacing
        if (metainfo[allocatedBuffer].dirty == true)
        {
            Disk::writeBlock(blocks[allocatedBuffer], metainfo[allocatedBuffer].blockNum);
        }
    }

    // set up the metadata for the newly allocated buffer slot
    metainfo[allocatedBuffer].free = false;
    metainfo[allocatedBuffer].dirty = false;
    metainfo[allocatedBuffer].blockNum = blockNum;
    metainfo[allocatedBuffer].timeStamp = 0;

    return allocatedBuffer;
}

// returns the buffer index of the slot holding blockNum
// or E_BLOCKNOTINBUFFER if not in buffer
int StaticBuffer::getBufferNum(int blockNum)
{
    if (blockNum < 0 || blockNum >= DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if (metainfo[bufferIndex].free == false &&
            metainfo[bufferIndex].blockNum == blockNum)
        {
            return bufferIndex;
        }
    }

    return E_BLOCKNOTINBUFFER;
}

// marks the buffer slot holding blockNum as dirty
// so it gets written back to disk when replaced or on exit
int StaticBuffer::setDirtyBit(int blockNum)
{
    // find the buffer slot holding this block
    int bufferNum = getBufferNum(blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER)
    {
        return E_BLOCKNOTINBUFFER;
    }

    if (bufferNum == E_OUTOFBOUND)
    {
        return E_OUTOFBOUND;
    }

    // mark as dirty
    metainfo[bufferNum].dirty = true;

    return SUCCESS;
}