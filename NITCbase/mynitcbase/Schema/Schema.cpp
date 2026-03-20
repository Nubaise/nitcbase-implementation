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