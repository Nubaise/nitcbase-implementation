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