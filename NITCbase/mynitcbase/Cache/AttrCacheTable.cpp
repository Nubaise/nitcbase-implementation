#include "AttrCacheTable.h"
#include <cstring>

// static member field - array of 12 pointers to AttrCacheEntry linked lists
// each index holds a linked list of all attributes of that relation
// attrCache[0] = linked list of RELATIONCAT's 6 attributes
// attrCache[1] = linked list of ATTRIBUTECAT's 6 attributes
AttrCacheEntry *AttrCacheTable::attrCache[MAX_OPEN];

// returns the attribute catalog entry for the attribute at 'attrOffset'
// in the relation with the given rel-id
int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuf)
{
    // rel-id must be between 0 and MAX_OPEN-1
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    // if there is no entry at this rel-id, the relation is not open
    if (attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    // traverse the linked list of attribute entries for this relation
    // each node in the list = one attribute of the relation
    for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if (entry->attrCatEntry.offset == attrOffset)
        {
            // found the attribute at the requested offset
            *attrCatBuf = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    // no attribute found at this offset
    return E_ATTRNOTEXIST;
}

// converts a raw record (array of Attributes) from the disk
// into an AttrCatEntry struct that is easier to work with
void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS],
                                          AttrCatEntry *attrCatEntry)
{
    // record[0] = RelName (string)
    strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
    // record[1] = AttributeName (string)
    strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
    // record[2] = AttributeType (number) - 0=NUM, 1=STR
    attrCatEntry->attrType = (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
    // record[3] = PrimaryFlag (number)
    attrCatEntry->primaryFlag = (int)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
    // record[4] = RootBlock (number) - B+ tree root, -1 if no index
    attrCatEntry->rootBlock = (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
    // record[5] = Offset (number) - position of this attribute (0=first, 1=second...)
    attrCatEntry->offset = (int)record[ATTRCAT_OFFSET_INDEX].nVal;
}