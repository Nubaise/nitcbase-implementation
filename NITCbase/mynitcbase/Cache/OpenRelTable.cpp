#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

// static member field declaration
OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable()
{

    // initialise all 12 entries in relCache, attrCache to nullptr
    // and all tableMetaInfo entries to free
    for (int i = 0; i < MAX_OPEN; i++)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
        tableMetaInfo[i].free = true;
    }

    /****** load RELATIONCAT into Relation Cache (rel-id = 0) ******/

    RecBuffer relCatBlock(RELCAT_BLOCK);

    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    RelCacheTable::relCache[RELCAT_RELID] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

    /****** load ATTRIBUTECAT into Relation Cache (rel-id = 1) ******/

    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

    struct RelCacheEntry attrRelCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &attrRelCacheEntry.relCatEntry);
    attrRelCacheEntry.recId.block = RELCAT_BLOCK;
    attrRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    RelCacheTable::relCache[ATTRCAT_RELID] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID]) = attrRelCacheEntry;

    /****** load RELATIONCAT's attributes into Attribute Cache (rel-id = 0) ******/

    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *prev = nullptr;

    for (int i = 0; i < 6; i++)
    {
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheEntry *entry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);
        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = i;
        entry->next = nullptr;

        if (head == nullptr)
            head = entry;
        if (prev != nullptr)
            prev->next = entry;
        prev = entry;
    }

    AttrCacheTable::attrCache[RELCAT_RELID] = head;

    /****** load ATTRIBUTECAT's attributes into Attribute Cache (rel-id = 1) ******/

    head = nullptr;
    prev = nullptr;

    for (int i = 6; i < 12; i++)
    {
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheEntry *entry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);
        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = i;
        entry->next = nullptr;

        if (head == nullptr)
            head = entry;
        if (prev != nullptr)
            prev->next = entry;
        prev = entry;
    }

    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;

    /****** set up tableMetaInfo for RELATIONCAT and ATTRIBUTECAT ******/

    // rel-id 0 = RELATIONCAT — mark as occupied
    tableMetaInfo[RELCAT_RELID].free = false;
    strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

    // rel-id 1 = ATTRIBUTECAT — mark as occupied
    tableMetaInfo[ATTRCAT_RELID].free = false;
    strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);

    // NOTE: we removed the hardcoded Students loading from here
    // Students will now be opened properly using OPEN TABLE command
}

OpenRelTable::~OpenRelTable()
{
    // close all open relations from rel-id 2 onwards
    for (int i = 2; i < MAX_OPEN; i++)
    {
        if (!tableMetaInfo[i].free)
        {
            OpenRelTable::closeRel(i);
        }
    }

    /****** write back RELATIONCAT cache if dirty ******/
    if (RelCacheTable::relCache[RELCAT_RELID]->dirty == true)
    {
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(
            &RelCacheTable::relCache[RELCAT_RELID]->relCatEntry, relCatRecord);
        RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(relCatRecord, recId.slot);
    }

    /****** write back ATTRIBUTECAT cache if dirty ******/
    if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty == true)
    {
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(
            &RelCacheTable::relCache[ATTRCAT_RELID]->relCatEntry, relCatRecord);
        RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(relCatRecord, recId.slot);
    }

    /****** free memory for RELATIONCAT (rel-id 0) ******/
    free(RelCacheTable::relCache[RELCAT_RELID]);

    /****** free memory for ATTRIBUTECAT (rel-id 1) ******/
    free(RelCacheTable::relCache[ATTRCAT_RELID]);

    /****** free attribute cache linked lists for rel-id 0 and 1 ******/
    for (int i = 0; i <= 1; i++)
    {
        AttrCacheEntry *entry = AttrCacheTable::attrCache[i];
        while (entry != nullptr)
        {
            AttrCacheEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
}

// finds the first free slot in tableMetaInfo
// returns the index of the free slot or E_CACHEFULL if none available
int OpenRelTable::getFreeOpenRelTableEntry()
{
    // scan tableMetaInfo from index 2 onwards
    // (0 and 1 are always reserved for catalogs)
    for (int i = 2; i < MAX_OPEN; i++)
    {
        if (tableMetaInfo[i].free)
        {
            return i; // found a free slot
        }
    }

    // no free slot found
    return E_CACHEFULL;
}

// returns the rel-id of a relation if it is open
// or E_RELNOTOPEN if it is not open
int OpenRelTable::getRelId(char relName[ATTR_SIZE])
{
    // scan through all tableMetaInfo entries
    for (int i = 0; i < MAX_OPEN; i++)
    {
        // check if this slot is occupied and the relation name matches
        if (!tableMetaInfo[i].free &&
            strcmp(tableMetaInfo[i].relName, relName) == 0)
        {
            return i; // return the rel-id
        }
    }

    // relation is not open
    return E_RELNOTOPEN;
}

// opens a relation by loading its catalog entries into the caches
// returns the rel-id assigned to the relation or an error code
int OpenRelTable::openRel(char relName[ATTR_SIZE])
{
    // check if the relation is already open
    int relId = getRelId(relName);
    if (relId != E_RELNOTOPEN)
    {
        // already open, just return its rel-id
        return relId;
    }

    // find a free slot in the cache
    int freeSlot = getFreeOpenRelTableEntry();
    if (freeSlot == E_CACHEFULL)
    {
        return E_CACHEFULL;
    }

    /****** search for the relation in RELATIONCAT ******/

    // reset search on RELATIONCAT to start from beginning
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    // search for the relation name in RELATIONCAT
    Attribute attrVal;
    strcpy(attrVal.sVal, relName);

    // find the record in RELATIONCAT where RelName = relName
    RecId relCatRecId = BlockAccess::linearSearch(
        RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, attrVal, EQ);

    // if not found, relation does not exist
    if (relCatRecId.block == -1 && relCatRecId.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    /****** load relation catalog entry into relation cache ******/

    // read the record from RELATIONCAT
    RecBuffer relCatBlock(relCatRecId.block);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, relCatRecId.slot);

    // convert to RelCacheEntry and store
    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = relCatRecId.block;
    relCacheEntry.recId.slot = relCatRecId.slot;

    // allocate on heap and store at freeSlot
    RelCacheTable::relCache[freeSlot] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[freeSlot]) = relCacheEntry;

    /****** load attribute catalog entries into attribute cache ******/

    // reset search on ATTRIBUTECAT to start from beginning
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    // get the number of attributes this relation has
    int numAttrs = relCacheEntry.relCatEntry.numAttrs;

    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *prev = nullptr;

    // find all attribute entries for this relation in ATTRIBUTECAT
    for (int i = 0; i < numAttrs; i++)
    {
        // search for next attribute of this relation
        RecId attrCatRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID, (char *)ATTRCAT_ATTR_RELNAME, attrVal, EQ);

        // read the attribute catalog record
        RecBuffer attrCatBlock(attrCatRecId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);

        // allocate a new AttrCacheEntry node
        AttrCacheEntry *entry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);
        entry->recId.block = attrCatRecId.block;
        entry->recId.slot = attrCatRecId.slot;
        entry->next = nullptr;

        // link into the linked list
        if (head == nullptr)
            head = entry;
        if (prev != nullptr)
            prev->next = entry;
        prev = entry;
    }

    // store the linked list in attrCache at freeSlot
    AttrCacheTable::attrCache[freeSlot] = head;

    /****** update tableMetaInfo ******/
    tableMetaInfo[freeSlot].free = false;
    strcpy(tableMetaInfo[freeSlot].relName, relName);

    return freeSlot; // return the rel-id assigned
}

int OpenRelTable::closeRel(int relId)
{
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
    {
        return E_NOTPERMITTED;
    }
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }
    if (tableMetaInfo[relId].free)
    {
        return E_RELNOTOPEN;
    }

    /****** write back relation cache if dirty ******/
    if (RelCacheTable::relCache[relId]->dirty == true)
    {
        // convert cache entry back to a record
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(
            &RelCacheTable::relCache[relId]->relCatEntry, relCatRecord);

        // get the rec-id of this entry in the relation catalog
        RecId recId = RelCacheTable::relCache[relId]->recId;

        // write back to the buffer
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(relCatRecord, recId.slot);
    }

    /****** free relation cache entry ******/
    free(RelCacheTable::relCache[relId]);
    RelCacheTable::relCache[relId] = nullptr;

    /****** free attribute cache linked list ******/
    AttrCacheEntry *entry = AttrCacheTable::attrCache[relId];
    while (entry != nullptr)
    {
        AttrCacheEntry *next = entry->next;
        free(entry);
        entry = next;
    }
    AttrCacheTable::attrCache[relId] = nullptr;

    /****** mark slot as free ******/
    tableMetaInfo[relId].free = true;

    return SUCCESS;
}