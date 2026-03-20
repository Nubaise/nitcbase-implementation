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
// updated to handle timestamps for LRU
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr)
{
    // check if block is already in buffer
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER)
    {
        // not in buffer — get a free slot (LRU if needed)
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if (bufferNum == E_OUTOFBOUND)
        {
            return E_OUTOFBOUND;
        }

        // read the block from disk into the free buffer slot
        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }
    else
    {
        // block is already in buffer
        // reset its timestamp to 0 (most recently used)
        // and increment all other occupied slots' timestamps
        for (int i = 0; i < BUFFER_CAPACITY; i++)
        {
            if (StaticBuffer::metainfo[i].free == false)
            {
                StaticBuffer::metainfo[i].timeStamp++;
            }
        }
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;
    }

    *buffPtr = StaticBuffer::blocks[bufferNum];

    return SUCCESS;
}

/* writes the record at slotNum in this block from the rec array
   marks the buffer as dirty so changes get written back to disk
*/
int RecBuffer::setRecord(union Attribute *rec, int slotNum)
{
    unsigned char *bufferPtr;

    // load block into buffer and get pointer
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
    {
        return ret;
    }

    struct HeadInfo head;
    this->getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    // check if slotNum is valid
    if (slotNum < 0 || slotNum >= slotCount)
    {
        return E_OUTOFBOUND;
    }

    // calculate where this slot is in the buffer
    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

    // copy the record into the buffer
    memcpy(slotPointer, rec, recordSize);

    // mark the buffer as dirty so it gets written back to disk
    StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}

/* reads the slotmap of this record block into the slotMap array
   slotMap[i] = SLOT_OCCUPIED or SLOT_UNOCCUPIED for each slot i
*/
int RecBuffer::getSlotMap(unsigned char *slotMap)
{
    unsigned char *bufferPtr;

    // load the block into buffer and get pointer to it
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
    {
        return ret;
    }

    struct HeadInfo head;
    // get the header to find out how many slots there are
    this->getHeader(&head);

    int slotCount = head.numSlots;

    // slotmap starts right after the header (at offset HEADER_SIZE)
    unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

    // copy slotCount bytes from buffer into slotMap
    memcpy(slotMap, slotMapInBuffer, slotCount);

    return SUCCESS;
}

/* compares two attribute values of the given type
   returns:
     negative value if attr1 < attr2
     0              if attr1 == attr2
     positive value if attr1 > attr2
*/
int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType)
{
    if (attrType == NUMBER)
    {
        // for numbers, just subtract
        // negative if attr1 < attr2, 0 if equal, positive if attr1 > attr2
        if (attr1.nVal < attr2.nVal)
            return -1;
        if (attr1.nVal == attr2.nVal)
            return 0;
        return 1;
    }
    else
    {
        // for strings, use strcmp
        // strcmp returns negative, 0, or positive just like we need
        return strcmp(attr1.sVal, attr2.sVal);
    }
}