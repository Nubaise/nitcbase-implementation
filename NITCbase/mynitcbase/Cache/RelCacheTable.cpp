#include "RelCacheTable.h"
#include <cstring>

// static member field - array of 12 pointers to RelCacheEntry
// index = rel-id of the relation
// relCache[0] = RELATIONCAT, relCache[1] = ATTRIBUTECAT, etc.
RelCacheEntry *RelCacheTable::relCache[MAX_OPEN];

// returns the relation catalog entry for the relation with the given rel-id
// copies the entry into relCatBuf
int RelCacheTable::getRelCatEntry(int relId, RelCatEntry *relCatBuf)
{
    // rel-id must be between 0 and MAX_OPEN-1
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    // if there is no entry at this rel-id, the relation is not open
    if (relCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    // copy the relation catalog entry to the output argument
    *relCatBuf = relCache[relId]->relCatEntry;

    return SUCCESS;
}

// converts a raw record (array of Attributes) from the disk
// into a RelCatEntry struct that is easier to work with
void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS],
                                        RelCatEntry *relCatEntry)
{
    // record[0] = RelName (string)
    strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
    // record[1] = #Attributes (number)
    relCatEntry->numAttrs = (int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    // record[2] = #Records (number)
    relCatEntry->numRecs = (int)record[RELCAT_NO_RECORDS_INDEX].nVal;
    // record[3] = FirstBlock (number)
    relCatEntry->firstBlk = (int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;
    // record[4] = LastBlock (number)
    relCatEntry->lastBlk = (int)record[RELCAT_LAST_BLOCK_INDEX].nVal;
    // record[5] = #Slots (number)
    relCatEntry->numSlotsPerBlk = (int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}

/* returns the searchIndex for the relation with the given relId
   searchIndex stores the rec-id of the last record found during linear search
   {-1,-1} means search should start from the beginning
*/
int RelCacheTable::getSearchIndex(int relId, RecId *searchIndex)
{
    // check if relId is valid
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    // check if the relation is open
    if (relCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    // copy the searchIndex from the cache entry to the output argument
    *searchIndex = relCache[relId]->searchIndex;

    return SUCCESS;
}

// updates the searchIndex for the relation with the given relId
int RelCacheTable::setSearchIndex(int relId, RecId *searchIndex)
{
    // check if relId is valid
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    // check if the relation is open
    if (relCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    // update the searchIndex in the cache entry
    relCache[relId]->searchIndex = *searchIndex;

    return SUCCESS;
}

// resets the searchIndex to {-1,-1} so search starts from the beginning
int RelCacheTable::resetSearchIndex(int relId)
{
    // create a rec-id with {-1,-1} meaning "start from beginning"
    RecId resetId = {-1, -1};

    // use setSearchIndex to reset it
    return setSearchIndex(relId, &resetId);
}

// updates the relation catalog entry in the cache and marks it dirty
int RelCacheTable::setRelCatEntry(int relId, RelCatEntry *relCatBuf)
{
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }
    if (relCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }
    // update the entry and mark as dirty so it gets written back on close
    relCache[relId]->relCatEntry = *relCatBuf;
    relCache[relId]->dirty = true;
    return SUCCESS;
}

// converts a RelCatEntry struct back to a raw record array
// used when writing cache back to disk on relation close
void RelCacheTable::relCatEntryToRecord(RelCatEntry *relCatEntry,
                                        union Attribute record[RELCAT_NO_ATTRS])
{
    strcpy(record[RELCAT_REL_NAME_INDEX].sVal, relCatEntry->relName);
    record[RELCAT_NO_ATTRIBUTES_INDEX].nVal = relCatEntry->numAttrs;
    record[RELCAT_NO_RECORDS_INDEX].nVal = relCatEntry->numRecs;
    record[RELCAT_FIRST_BLOCK_INDEX].nVal = relCatEntry->firstBlk;
    record[RELCAT_LAST_BLOCK_INDEX].nVal = relCatEntry->lastBlk;
    record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = relCatEntry->numSlotsPerBlk;
}