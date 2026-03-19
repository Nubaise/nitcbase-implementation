#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTable::OpenRelTable()
{

    // initialise all 12 entries in both caches to nullptr (nothing is open yet)
    for (int i = 0; i < MAX_OPEN; i++)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }

    /****** load RELATIONCAT into Relation Cache (rel-id = 0) ******/

    // create a RecBuffer object to read from block 4 (RELCAT_BLOCK)
    RecBuffer relCatBlock(RELCAT_BLOCK);

    // read the record at slot 0 (that's RELATIONCAT's own entry)
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

    // convert the raw record into a RelCatEntry struct
    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);

    // remember where on disk this entry came from (block 4, slot 0)
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    // allocate memory on heap and store it at rel-id 0
    RelCacheTable::relCache[RELCAT_RELID] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

    /****** load ATTRIBUTECAT into Relation Cache (rel-id = 1) ******/

    // read the record at slot 1 (that's ATTRIBUTECAT's entry in RELCAT)
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

    // convert and store similarly
    struct RelCacheEntry attrRelCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &attrRelCacheEntry.relCatEntry);
    attrRelCacheEntry.recId.block = RELCAT_BLOCK;
    attrRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    RelCacheTable::relCache[ATTRCAT_RELID] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID]) = attrRelCacheEntry;

    /****** load RELATIONCAT's attributes into Attribute Cache (rel-id = 0) ******/

    // create a RecBuffer object to read from block 5 (ATTRCAT_BLOCK)
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    // RELATIONCAT has 6 attributes stored at slots 0-5 of the attribute catalog
    // we build a linked list of AttrCacheEntry, one node per attribute
    AttrCacheEntry *head = nullptr; // head of the linked list
    AttrCacheEntry *prev = nullptr; // previous node (to link the list)

    for (int i = 0; i < 6; i++)
    {
        // read attribute i of RELATIONCAT
        attrCatBlock.getRecord(attrCatRecord, i);

        // allocate a new node on the heap
        AttrCacheEntry *entry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

        // convert the raw record to AttrCatEntry struct
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);

        // remember where on disk this entry came from
        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = i;
        entry->next = nullptr;

        // link this node into the list
        if (head == nullptr)
        {
            head = entry; // first node becomes the head
        }
        if (prev != nullptr)
        {
            prev->next = entry; // link previous node to this one
        }
        prev = entry;
    }

    // store the head of the linked list at rel-id 0
    AttrCacheTable::attrCache[RELCAT_RELID] = head;

    /****** load ATTRIBUTECAT's attributes into Attribute Cache (rel-id = 1) ******/

    // ATTRIBUTECAT has 6 attributes stored at slots 6-11 of the attribute catalog
    head = nullptr;
    prev = nullptr;

    for (int i = 6; i < 12; i++)
    {
        // read attribute i of ATTRIBUTECAT
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheEntry *entry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);

        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = i;
        entry->next = nullptr;

        if (head == nullptr)
        {
            head = entry;
        }
        if (prev != nullptr)
        {
            prev->next = entry;
        }
        prev = entry;
    }

    // store the head of the linked list at rel-id 1
    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;

    /****** load Students into Relation Cache (rel-id = 2) ******/

    // slot 2 in RelCat = Students (slot 0=RELATIONCAT, 1=ATTRIBUTECAT, 2=Students)
    relCatBlock.getRecord(relCatRecord, 2);

    // convert and store at rel-id 2
    struct RelCacheEntry studentRelCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &studentRelCacheEntry.relCatEntry);
    studentRelCacheEntry.recId.block = RELCAT_BLOCK;
    studentRelCacheEntry.recId.slot = 2;

    RelCacheTable::relCache[2] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[2]) = studentRelCacheEntry;

    /****** load Students' attributes into Attribute Cache (rel-id = 2) ******/

    // Students has 4 attributes at slots 12-15 in AttrCat
    // (slots 0-5 = RELATIONCAT attrs, 6-11 = ATTRIBUTECAT attrs, 12-15 = Students attrs)
    head = nullptr;
    prev = nullptr;

    for (int i = 12; i < 16; i++)
    {
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheEntry *entry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);

        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = i;
        entry->next = nullptr;

        if (head == nullptr)
        {
            head = entry;
        }
        if (prev != nullptr)
        {
            prev->next = entry;
        }
        prev = entry;
    }

    AttrCacheTable::attrCache[2] = head;
}

OpenRelTable::~OpenRelTable()
{
    // free all memory we allocated in the constructor
    for (int i = 0; i < MAX_OPEN; i++)
    {

        // free the relation cache entry if it exists
        if (RelCacheTable::relCache[i] != nullptr)
        {
            free(RelCacheTable::relCache[i]);
        }

        // free the attribute cache linked list if it exists
        if (AttrCacheTable::attrCache[i] != nullptr)
        {
            // traverse the linked list and free each node
            AttrCacheEntry *entry = AttrCacheTable::attrCache[i];
            while (entry != nullptr)
            {
                AttrCacheEntry *next = entry->next; // save next before freeing
                free(entry);
                entry = next;
            }
        }
    }
}