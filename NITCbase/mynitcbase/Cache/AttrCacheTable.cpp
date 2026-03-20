#include "AttrCacheTable.h"
#include <cstring>

// static member field - array of 12 pointers to AttrCacheEntry linked lists
// each index holds a linked list of all attributes of that relation
// attrCache[0] = linked list of RELATIONCAT's 6 attributes
// attrCache[1] = linked list of ATTRIBUTECAT's 6 attributes
AttrCacheEntry *AttrCacheTable::attrCache[MAX_OPEN];

/* returns the attribute catalog entry for the attribute with name attrName
   for the relation with the given relId
   this is an overload of getAttrCatEntry that searches by name instead of offset
*/
int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuf)
{
    // check if relId is valid
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    // check if the relation is open
    if (attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    // traverse the linked list of attribute entries for this relation
    for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        // check if this entry's attribute name matches what we're looking for
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0)
        {
            // found it! copy to output and return
            *attrCatBuf = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    // no attribute with this name found
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

// returns the attribute catalog entry for the attribute at offset attrOffset
// for the relation with the given relId
int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuf)
{
    // check if relId is valid
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    // check if the relation is open
    if (attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    // traverse the linked list of attribute entries for this relation
    for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if (entry->attrCatEntry.offset == attrOffset)
        {
            // found it! copy to output and return
            *attrCatBuf = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    // no attribute at this offset
    return E_ATTRNOTEXIST;
}