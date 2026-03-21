#include "Algebra.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// checks if a string can be parsed as a number
bool isNumber(char *str)
{
    int len;
    float ignore;
    int ret = sscanf(str, "%f %n", &ignore, &len);
    return ret == 1 && len == strlen(str);
}

/* selects all records from srcRel that satisfy the condition
   attr op strVal and prints them to the console
   (in later stages this will insert into targetRel instead)
*/
// updated select — now inserts matching records into targetRel
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                    char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE])
{

    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    // get attribute catalog entry for the filter attribute
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
    if (ret == E_ATTRNOTEXIST)
    {
        return E_ATTRNOTEXIST;
    }

    // convert strVal to correct type
    int type = attrCatEntry.attrType;
    Attribute attrVal;

    if (type == NUMBER)
    {
        if (isNumber(strVal))
        {
            attrVal.nVal = atof(strVal);
        }
        else
        {
            return E_ATTRTYPEMISMATCH;
        }
    }
    else
    {
        strcpy(attrVal.sVal, strVal);
    }

    // get source relation catalog entry
    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);

    // get all attribute names and types from source relation
    int numAttrs = srcRelCatEntry.numAttrs;
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    for (int i = 0; i < numAttrs; i++)
    {
        AttrCatEntry entry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);
        strcpy(attrNames[i], entry.attrName);
        attrTypes[i] = entry.attrType;
    }

    // create the target relation with same schema as source
    ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
    if (ret != SUCCESS)
    {
        return ret;
    }

    // open the target relation
    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    // reset search on source relation
    RelCacheTable::resetSearchIndex(srcRelId);

    // search and insert matching records into target
    Attribute record[numAttrs];
    while (true)
    {
        int searchRet = BlockAccess::search(srcRelId, record, attr, attrVal, op);
        if (searchRet == E_NOTFOUND)
        {
            break;
        }

        ret = BlockAccess::insert(targetRelId, record);
        if (ret != SUCCESS)
        {
            OpenRelTable::closeRel(targetRelId);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // close the target relation
    OpenRelTable::closeRel(targetRelId);

    return SUCCESS;
}

// project all attributes — creates a copy of srcRel into targetRel
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE])
{

    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    // get source relation info
    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
    int numAttrs = srcRelCatEntry.numAttrs;

    // get all attribute names and types
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    for (int i = 0; i < numAttrs; i++)
    {
        AttrCatEntry entry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);
        strcpy(attrNames[i], entry.attrName);
        attrTypes[i] = entry.attrType;
    }

    // create target relation with same schema
    int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
    if (ret != SUCCESS)
    {
        return ret;
    }

    // open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    // reset search on source
    RelCacheTable::resetSearchIndex(srcRelId);

    // copy all records from source to target
    Attribute record[numAttrs];
    while (true)
    {
        int projectRet = BlockAccess::project(srcRelId, record);
        if (projectRet == E_NOTFOUND)
        {
            break;
        }

        ret = BlockAccess::insert(targetRelId, record);
        if (ret != SUCCESS)
        {
            OpenRelTable::closeRel(targetRelId);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    OpenRelTable::closeRel(targetRelId);
    return SUCCESS;
}

// project specified attributes — creates targetRel with only the given attrs
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                     int numAttrs, char attrs[][ATTR_SIZE])
{

    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    // validate all requested attributes exist and get their types
    int attrTypes[numAttrs];
    for (int i = 0; i < numAttrs; i++)
    {
        AttrCatEntry entry;
        int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attrs[i], &entry);
        if (ret == E_ATTRNOTEXIST)
        {
            return E_ATTRNOTEXIST;
        }
        attrTypes[i] = entry.attrType;
    }

    // create target relation with only the specified attributes
    int ret = Schema::createRel(targetRel, numAttrs, attrs, attrTypes);
    if (ret != SUCCESS)
    {
        return ret;
    }

    // open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    // get source relation info
    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
    int srcNumAttrs = srcRelCatEntry.numAttrs;

    // reset search on source
    RelCacheTable::resetSearchIndex(srcRelId);

    // for each record in source, pick only the requested attributes
    Attribute srcRecord[srcNumAttrs];

    while (true)
    {
        int projectRet = BlockAccess::project(srcRelId, srcRecord);
        if (projectRet == E_NOTFOUND)
        {
            break;
        }

        // build the projected record with only requested attributes
        Attribute targetRecord[numAttrs];
        for (int i = 0; i < numAttrs; i++)
        {
            AttrCatEntry entry;
            AttrCacheTable::getAttrCatEntry(srcRelId, attrs[i], &entry);
            targetRecord[i] = srcRecord[entry.offset];
        }

        ret = BlockAccess::insert(targetRelId, targetRecord);
        if (ret != SUCCESS)
        {
            OpenRelTable::closeRel(targetRelId);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    OpenRelTable::closeRel(targetRelId);
    return SUCCESS;
}

int Algebra::insert(char relName[ATTR_SIZE], int attrCount,
                    char attrValues[][ATTR_SIZE])
{
    int relId = OpenRelTable::getRelId(relName);
    if (relId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    // cannot insert directly into relation catalog
    if (relId == RELCAT_RELID)
    {
        return E_NOTPERMITTED;
    }

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    // check number of attributes matches
    if (attrCount != relCatEntry.numAttrs)
    {
        return E_NATTRMISMATCH;
    }

    // trim trailing whitespace/newlines from all string values
    for (int i = 0; i < attrCount; i++)
    {
        int len = strlen(attrValues[i]);
        while (len > 0 && (attrValues[i][len - 1] == '\n' ||
                           attrValues[i][len - 1] == '\r' ||
                           attrValues[i][len - 1] == ' '))
        {
            attrValues[i][--len] = '\0';
        }
    }

    // convert string values to Attribute type
    Attribute record[relCatEntry.numAttrs];

    for (int i = 0; i < relCatEntry.numAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);

        if (attrCatEntry.attrType == NUMBER)
        {
            if (isNumber(attrValues[i]))
            {
                record[i].nVal = atof(attrValues[i]);
            }
            else
            {
                return E_ATTRTYPEMISMATCH;
            }
        }
        else
        {
            strcpy(record[i].sVal, attrValues[i]);
        }
    }

    return BlockAccess::insert(relId, record);
}