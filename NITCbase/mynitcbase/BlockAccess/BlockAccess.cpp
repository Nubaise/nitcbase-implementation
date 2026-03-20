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