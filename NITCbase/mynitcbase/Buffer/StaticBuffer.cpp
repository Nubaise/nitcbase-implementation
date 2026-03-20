#include "StaticBuffer.h"
#include "StaticBuffer.h"
#include <cstring>

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer()
{
    // initialise all buffer slots FIRST
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }

    // read BAM from disk into blockAllocMap using a temporary buffer
    // do NOT use blocks[][] for this — blocks[][] is for user data only
    unsigned char tmpBlock[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_ALLOCATION_MAP_SIZE; i++)
    {
        Disk::readBlock(tmpBlock, i);
        // copy this BAM block into the correct section of blockAllocMap
        memcpy(blockAllocMap + (i * BLOCK_SIZE), tmpBlock, BLOCK_SIZE);
    }
}

StaticBuffer::~StaticBuffer()
{
    // write blockAllocMap back to disk blocks 0-3
    unsigned char tmpBlock[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_ALLOCATION_MAP_SIZE; i++)
    {
        memcpy(tmpBlock, blockAllocMap + (i * BLOCK_SIZE), BLOCK_SIZE);
        Disk::writeBlock(tmpBlock, i);
    }

    // write back all dirty blocks to disk
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if (metainfo[bufferIndex].free == false &&
            metainfo[bufferIndex].dirty == true)
        {
            Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
        }
    }
}

int StaticBuffer::getFreeBuffer(int blockNum)
{
    if (blockNum < 0 || blockNum >= DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if (metainfo[bufferIndex].free == false)
        {
            metainfo[bufferIndex].timeStamp++;
        }
    }

    int allocatedBuffer = -1;

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if (metainfo[bufferIndex].free)
        {
            allocatedBuffer = bufferIndex;
            break;
        }
    }

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
        if (metainfo[allocatedBuffer].dirty == true)
        {
            Disk::writeBlock(blocks[allocatedBuffer], metainfo[allocatedBuffer].blockNum);
        }
    }

    metainfo[allocatedBuffer].free = false;
    metainfo[allocatedBuffer].dirty = false;
    metainfo[allocatedBuffer].blockNum = blockNum;
    metainfo[allocatedBuffer].timeStamp = 0;

    return allocatedBuffer;
}

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

int StaticBuffer::setDirtyBit(int blockNum)
{
    int bufferNum = getBufferNum(blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER)
    {
        return E_BLOCKNOTINBUFFER;
    }
    if (bufferNum == E_OUTOFBOUND)
    {
        return E_OUTOFBOUND;
    }

    metainfo[bufferNum].dirty = true;
    return SUCCESS;
}

int StaticBuffer::getStaticBlockType(int blockNum)
{
    if (blockNum < 0 || blockNum >= DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }
    return (int)blockAllocMap[blockNum];
}