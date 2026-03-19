#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>

// constructor - just saves the block number we want to work with
BlockBuffer::BlockBuffer(int blockNum)
{
    this->blockNum = blockNum;
}

// RecBuffer constructor - calls the parent BlockBuffer constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

// reads the 32-byte header of this block into the HeadInfo struct
// now uses the buffer instead of reading from disk directly every time
int BlockBuffer::getHeader(struct HeadInfo *head)
{
    unsigned char *bufferPtr;

    // load the block into buffer and get a pointer to it
    // if block is already in buffer, this just returns the pointer
    // if not, it reads from disk first, then returns the pointer
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
    {
        return ret;
    }

    // copy each field from the correct byte offset in the buffer
    // (same byte positions as stage 2, but now reading from buffer not disk)
    memcpy(&head->numSlots, bufferPtr + 24, 4);   // bytes 24-27
    memcpy(&head->numEntries, bufferPtr + 16, 4); // bytes 16-19
    memcpy(&head->numAttrs, bufferPtr + 20, 4);   // bytes 20-23
    memcpy(&head->rblock, bufferPtr + 12, 4);     // bytes 12-15
    memcpy(&head->lblock, bufferPtr + 8, 4);      // bytes 8-11

    return SUCCESS;
}

// reads the record at slotNum from this block into the rec array
// now uses the buffer instead of reading from disk directly every time
int RecBuffer::getRecord(union Attribute *rec, int slotNum)
{
    struct HeadInfo head;

    // first get the header to know numAttrs and numSlots
    this->getHeader(&head);

    int attrCount = head.numAttrs; // how many attributes per record
    int slotCount = head.numSlots; // how many slots in this block

    unsigned char *bufferPtr;

    // load the block into buffer and get a pointer to it
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
    {
        return ret;
    }

    // calculate where the record at slotNum starts in the buffer:
    // skip header (32 bytes) + skip slotmap (slotCount bytes) + skip previous records
    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

    // copy the record into rec
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}

// loads the block into the buffer if not already there
// sets *buffPtr to point to the buffer slot holding this block
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr)
{
    // check if this block is already in the buffer
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER)
    {
        // block is not in buffer, so we need to load it from disk

        // get a free buffer slot for this block
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if (bufferNum == E_OUTOFBOUND)
        {
            return E_OUTOFBOUND;
        }

        // read the block from disk into the free buffer slot
        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }

    // set the pointer to this buffer slot so the caller can read from it
    *buffPtr = StaticBuffer::blocks[bufferNum];

    return SUCCESS;
}