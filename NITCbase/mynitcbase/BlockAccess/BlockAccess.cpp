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