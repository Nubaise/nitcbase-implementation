#include "Schema.h"
#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE])
{
    // call OpenRelTable::openRel to load the relation into cache
    int ret = OpenRelTable::openRel(relName);

    // openRel returns the rel-id if successful (>= 0)
    // or a negative error code if failed
    if (ret >= 0)
    {
        return SUCCESS;
    }

    return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE])
{
    // relation catalog and attribute catalog cannot be closed
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return E_NOTPERMITTED;
    }

    // get the rel-id of this relation
    int relId = OpenRelTable::getRelId(relName);

    // if relation is not open, return error
    if (relId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char currRelName[ATTR_SIZE], char newRelName[ATTR_SIZE])
{
    // cannot rename the catalogs
    if (strcmp(currRelName, RELCAT_RELNAME) == 0 ||
        strcmp(currRelName, ATTRCAT_RELNAME) == 0 ||
        strcmp(newRelName, RELCAT_RELNAME) == 0 ||
        strcmp(newRelName, ATTRCAT_RELNAME) == 0)
    {
        return E_NOTPERMITTED;
    }

    // relation must be closed before renaming
    if (OpenRelTable::getRelId(currRelName) != E_RELNOTOPEN)
    {
        return E_RELOPEN;
    }

    // call Block Access Layer to do the actual renaming
    int ret = BlockAccess::renameRelation(currRelName, newRelName);
    return ret;
}

int Schema::renameAttr(char relName[ATTR_SIZE], char currAttrName[ATTR_SIZE],
                       char newAttrName[ATTR_SIZE])
{
    // cannot rename attributes of the catalogs
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return E_NOTPERMITTED;
    }

    // relation must be closed before renaming
    if (OpenRelTable::getRelId(relName) != E_RELNOTOPEN)
    {
        return E_RELOPEN;
    }

    // call Block Access Layer to do the actual renaming
    int ret = BlockAccess::renameAttribute(relName, currAttrName, newAttrName);
    return ret;
}

int Schema::createRel(char relName[ATTR_SIZE], int numOfAttributes,
                      char attributes[][ATTR_SIZE], int attributeTypes[])
{

    // cannot create relation with same name as catalogs
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return E_NOTPERMITTED;
    }

    // check for duplicate attribute names
    for (int i = 0; i < numOfAttributes; i++)
    {
        for (int j = i + 1; j < numOfAttributes; j++)
        {
            if (strcmp(attributes[i], attributes[j]) == 0)
            {
                return E_DUPLICATEATTR;
            }
        }
    }

    // check if relation already exists by searching RELATIONCAT
    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    RecId relCatRecId = BlockAccess::linearSearch(
        RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    if (relCatRecId.block != -1 && relCatRecId.slot != -1)
    {
        return E_RELEXIST;
    }

    // check max relations limit
    // RELATIONCAT can hold max 20 entries, 2 are reserved for catalogs = 18 user relations
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
    if (relCatEntry.numRecs >= 18)
    {
        return E_MAXRELATIONS;
    }

    /*** insert a record into RELATIONCAT ***/
    // numSlots = floor(2016 / (16 * numOfAttributes + 1))
    int numSlots = 2016 / (16 * numOfAttributes + 1);

    Attribute relCatRecord[RELCAT_NO_ATTRS];
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = numOfAttributes;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = numSlots;

    int ret = BlockAccess::insert(RELCAT_RELID, relCatRecord);
    if (ret != SUCCESS)
    {
        return ret;
    }

    /*** insert records into ATTRIBUTECAT for each attribute ***/
    for (int i = 0; i < numOfAttributes; i++)
    {
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attributes[i]);
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attributeTypes[i];
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

        ret = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
        if (ret != SUCCESS)
        {
            // if insertion fails midway, we should ideally rollback
            // but for now just return the error
            return ret;
        }
    }

    return SUCCESS;
}

int Schema::deleteRel(char relName[ATTR_SIZE])
{
    // cannot delete the catalogs
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return E_NOTPERMITTED;
    }

    // relation must be closed before deleting
    if (OpenRelTable::getRelId(relName) != E_RELNOTOPEN)
    {
        return E_RELOPEN;
    }

    // call Block Access Layer to delete the relation
    return BlockAccess::deleteRelation(relName);
}