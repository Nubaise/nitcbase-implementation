#include "BlockAccess.h"
#include <cstring>

/* searches for the next record in relId that satisfies the condition
   attr op attrVal (e.g. Marks >= 90)
   returns the rec-id {block, slot} of the matching record
   or {-1, -1} if no more matching records

   uses searchIndex in the relation cache to remember where we left off
   so repeated calls return successive matching records
*/
RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE],
                                union Attribute attrVal, int op)
{
    RecId prevRecId;

    // get the previous search index (where we left off last time)
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    int block, slot;

    // if searchIndex is {-1,-1}, start from the first record of the relation
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // get the relation catalog entry to find the first block
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        // start from the first block of the relation
        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else
    {
        // resume from the next slot after the previous hit
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // get the attribute catalog entry for the attribute we're searching on
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    // traverse the linked list of blocks
    while (block != -1)
    {
        // create a RecBuffer for the current block
        RecBuffer blockBuffer(block);

        // get the header to know numSlots and rblock
        HeadInfo head;
        blockBuffer.getHeader(&head);

        // get the slotmap to know which slots are occupied
        unsigned char slotMap[head.numSlots];
        blockBuffer.getSlotMap(slotMap);

        // if slot has gone past the end of this block, move to next block
        if (slot >= head.numSlots)
        {
            // follow the linked list to the next block
            block = head.rblock;
            slot = 0;
            continue;
        }

        // check each slot from current slot onwards in this block
        if (slotMap[slot] == SLOT_UNOCCUPIED)
        {
            // slot is empty, skip it
            slot++;
            continue;
        }

        // slot is occupied — read the record
        Attribute record[head.numAttrs];
        blockBuffer.getRecord(record, slot);

        // compare the attribute value in the record with the search value
        // using the offset from attrCatEntry to find the right attribute
        int cmpResult = compareAttrs(record[attrCatEntry.offset], attrVal, attrCatEntry.attrType);

        // check if the comparison satisfies the operator
        bool matchFound = false;
        switch (op)
        {
        case EQ:
            matchFound = (cmpResult == 0);
            break; // =
        case NE:
            matchFound = (cmpResult != 0);
            break; // !=
        case LT:
            matchFound = (cmpResult < 0);
            break; //
        case LE:
            matchFound = (cmpResult <= 0);
            break; // <=
        case GT:
            matchFound = (cmpResult > 0);
            break; // >
        case GE:
            matchFound = (cmpResult >= 0);
            break; // >=
        }

        if (matchFound)
        {
            // update the searchIndex so next call resumes from here
            RecId foundRecId = {block, slot};
            RelCacheTable::setSearchIndex(relId, &foundRecId);

            return foundRecId;
        }

        // no match at this slot, move to next slot
        slot++;
    }

    // no more matching records found
    // reset searchIndex to {-1,-1} so next call starts fresh
    RelCacheTable::resetSearchIndex(relId);

    return RecId{-1, -1};
}

/* renames a relation from currRelName to newRelName
   updates both the relation catalog entry and all attribute catalog entries
*/
int BlockAccess::renameRelation(char currRelName[ATTR_SIZE], char newRelName[ATTR_SIZE])
{

    /*** update the relation catalog entry ***/

    // reset search on RELATIONCAT
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    // search for currRelName in RELATIONCAT
    Attribute attrVal;
    strcpy(attrVal.sVal, currRelName);

    RecId relCatRecId = linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, attrVal, EQ);

    // if not found, relation does not exist
    if (relCatRecId.block == -1 && relCatRecId.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    // check if newRelName already exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute newAttrVal;
    strcpy(newAttrVal.sVal, newRelName);

    RecId checkId = linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, newAttrVal, EQ);
    if (checkId.block != -1 && checkId.slot != -1)
    {
        return E_RELEXIST;
    }

    // read the relation catalog record
    RecBuffer relCatBlock(relCatRecId.block);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, relCatRecId.slot);

    // update the RelName field with the new name
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newRelName);

    // write the updated record back
    relCatBlock.setRecord(relCatRecord, relCatRecId.slot);

    /*** update all attribute catalog entries for this relation ***/

    // reset search on ATTRIBUTECAT
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    // find and update all entries where RelName = currRelName
    while (true)
    {
        RecId attrCatRecId = linearSearch(ATTRCAT_RELID, (char *)ATTRCAT_ATTR_RELNAME, attrVal, EQ);

        // no more entries for this relation
        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1)
        {
            break;
        }

        // read the attribute catalog record
        RecBuffer attrCatBlock(attrCatRecId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);

        // update the RelName field
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newRelName);

        // write the updated record back
        attrCatBlock.setRecord(attrCatRecord, attrCatRecId.slot);
    }

    return SUCCESS;
}

/* renames an attribute of a relation from currAttrName to newAttrName
   updates the attribute catalog entry
*/
int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char currAttrName[ATTR_SIZE],
                                 char newAttrName[ATTR_SIZE])
{

    // reset search on ATTRIBUTECAT
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    // search for the relation in ATTRIBUTECAT
    Attribute relAttrVal;
    strcpy(relAttrVal.sVal, relName);

    // find the attribute entry where RelName = relName AND AttrName = currAttrName
    while (true)
    {
        RecId attrCatRecId = linearSearch(ATTRCAT_RELID, (char *)ATTRCAT_ATTR_RELNAME, relAttrVal, EQ);

        // no more entries for this relation — attribute not found
        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1)
        {
            return E_ATTRNOTEXIST;
        }

        // read the attribute catalog record
        RecBuffer attrCatBlock(attrCatRecId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);

        // check if this is the attribute we want to rename
        if (strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, currAttrName) == 0)
        {

            // check if newAttrName already exists for this relation
            // (we'll do a simple check here)
            strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName);

            // write the updated record back
            attrCatBlock.setRecord(attrCatRecord, attrCatRecId.slot);

            return SUCCESS;
        }
    }
}

int BlockAccess::insert(int relId, Attribute *record)
{
    // get the relation catalog entry
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    int blockNum = relCatEntry.firstBlk;
    RecId rec_id = {-1, -1};
    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttrs = relCatEntry.numAttrs;
    int prevBlockNum = -1;

    // traverse existing blocks to find a free slot
    while (blockNum != -1)
    {
        RecBuffer blockBuffer(blockNum);

        HeadInfo head;
        blockBuffer.getHeader(&head);

        unsigned char slotMap[numOfSlots];
        blockBuffer.getSlotMap(slotMap);

        // search for a free slot in this block
        for (int i = 0; i < numOfSlots; i++)
        {
            if (slotMap[i] == SLOT_UNOCCUPIED)
            {
                rec_id.block = blockNum;
                rec_id.slot = i;
                break;
            }
        }

        if (rec_id.block != -1)
            break;

        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }

    // no free slot found — allocate a new block
    if (rec_id.block == -1 && rec_id.slot == -1)
    {
        // cannot allocate more blocks for relation catalog
        if (relId == RELCAT_RELID)
        {
            return E_MAXRELATIONS;
        }

        // allocate a new record block using Constructor1
        RecBuffer newBlock;
        int ret = newBlock.getBlockNum();

        if (ret == E_DISKFULL)
        {
            return E_DISKFULL;
        }

        rec_id.block = ret;
        rec_id.slot = 0;

        // set up header for the new block
        HeadInfo head;
        head.blockType = REC;
        head.pblock = -1;
        head.lblock = prevBlockNum;
        head.rblock = -1;
        head.numEntries = 0;
        head.numAttrs = numOfAttrs;
        head.numSlots = numOfSlots;
        newBlock.setHeader(&head);

        // initialise slotmap — all slots free
        unsigned char slotMap[numOfSlots];
        for (int i = 0; i < numOfSlots; i++)
        {
            slotMap[i] = SLOT_UNOCCUPIED;
        }
        newBlock.setSlotMap(slotMap);

        if (prevBlockNum != -1)
        {
            // link previous block to new block
            RecBuffer prevBlock(prevBlockNum);
            HeadInfo prevHead;
            prevBlock.getHeader(&prevHead);
            prevHead.rblock = rec_id.block;
            prevBlock.setHeader(&prevHead);
        }
        else
        {
            // first block of the relation
            relCatEntry.firstBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatEntry);
        }

        // update lastBlk
        relCatEntry.lastBlk = rec_id.block;
        RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    }

    // insert the record
    RecBuffer recBuffer(rec_id.block);
    recBuffer.setRecord(record, rec_id.slot);

    // update slotmap
    unsigned char slotMap[numOfSlots];
    recBuffer.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    recBuffer.setSlotMap(slotMap);

    // update numEntries in header
    HeadInfo head;
    recBuffer.getHeader(&head);
    head.numEntries++;
    recBuffer.setHeader(&head);

    // update numRecs in relation cache
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    relCatEntry.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);

    return SUCCESS;
}

// searches for a record satisfying the condition and copies it to record[]
int BlockAccess::search(int relId, Attribute *record,
                        char attrName[ATTR_SIZE], Attribute attrVal, int op)
{
    // use linearSearch to find the record
    RecId recId = linearSearch(relId, attrName, attrVal, op);

    // if no record found
    if (recId.block == -1 && recId.slot == -1)
    {
        return E_NOTFOUND;
    }

    // copy the record to the output argument
    RecBuffer blockBuffer(recId.block);
    blockBuffer.getRecord(record, recId.slot);

    return SUCCESS;
}

// deletes a relation — frees all its blocks and removes catalog entries
int BlockAccess::deleteRelation(char relName[ATTR_SIZE])
{

    /****** find the relation in RELATIONCAT ******/
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    RecId relCatRecId = linearSearch(
        RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    if (relCatRecId.block == -1 && relCatRecId.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    // read the relation catalog record
    RecBuffer relCatBlock(relCatRecId.block);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, relCatRecId.slot);

    // get first block of the relation
    int firstBlock = (int)relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;

    /****** free all record blocks of the relation ******/
    int blockNum = firstBlock;
    while (blockNum != -1)
    {
        RecBuffer recBuffer(blockNum);
        HeadInfo head;
        recBuffer.getHeader(&head);
        int nextBlock = head.rblock;

        // release this block
        recBuffer.releaseBlock();

        blockNum = nextBlock;
    }

    /****** delete all entries from ATTRIBUTECAT ******/
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    int numAttrs = (int)relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    for (int i = 0; i < numAttrs; i++)
    {
        RecId attrCatRecId = linearSearch(
            ATTRCAT_RELID, (char *)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1)
        {
            break;
        }

        // get the block and read its header
        RecBuffer attrCatBlockBuf(attrCatRecId.block);
        HeadInfo attrCatHeader;
        attrCatBlockBuf.getHeader(&attrCatHeader);

        // mark the slot as unoccupied in slotmap
        unsigned char slotMap[attrCatHeader.numSlots];
        attrCatBlockBuf.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        attrCatBlockBuf.setSlotMap(slotMap);

        // update numEntries in header
        attrCatHeader.numEntries--;
        attrCatBlockBuf.setHeader(&attrCatHeader);

        // update ATTRIBUTECAT's numRecs in cache
        RelCatEntry attrCatEntry;
        RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
        attrCatEntry.numRecs--;
        RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatEntry);

        // if block is now empty, release it
        if (attrCatHeader.numEntries == 0)
        {
            // update linked list pointers
            if (attrCatHeader.lblock != -1)
            {
                RecBuffer leftBlock(attrCatHeader.lblock);
                HeadInfo leftHead;
                leftBlock.getHeader(&leftHead);
                leftHead.rblock = attrCatHeader.rblock;
                leftBlock.setHeader(&leftHead);
            }
            else
            {
                // this was the first block — update firstBlk in cache
                RelCatEntry attrRelEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrRelEntry);
                attrRelEntry.firstBlk = attrCatHeader.rblock;
                RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrRelEntry);
            }

            if (attrCatHeader.rblock != -1)
            {
                RecBuffer rightBlock(attrCatHeader.rblock);
                HeadInfo rightHead;
                rightBlock.getHeader(&rightHead);
                rightHead.lblock = attrCatHeader.lblock;
                rightBlock.setHeader(&rightHead);
            }
            else
            {
                // this was the last block — update lastBlk in cache
                RelCatEntry attrRelEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrRelEntry);
                attrRelEntry.lastBlk = attrCatHeader.lblock;
                RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrRelEntry);
            }

            attrCatBlockBuf.releaseBlock();
        }
    }

    /****** delete the entry from RELATIONCAT ******/
    // mark the slot as unoccupied
    unsigned char slotMap[SLOTMAP_SIZE_RELCAT_ATTRCAT];
    relCatBlock.getSlotMap(slotMap);
    slotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;
    relCatBlock.setSlotMap(slotMap);

    // update numEntries in header
    HeadInfo relCatHeader;
    relCatBlock.getHeader(&relCatHeader);
    relCatHeader.numEntries--;
    relCatBlock.setHeader(&relCatHeader);

    // update RELATIONCAT's numRecs in cache
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
    relCatEntry.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);

    return SUCCESS;
}