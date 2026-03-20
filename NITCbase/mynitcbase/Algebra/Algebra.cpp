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
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                    char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE])
{

    // get the rel-id of the source relation
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    // get the attribute catalog entry for the attribute we're filtering on
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
    if (ret == E_ATTRNOTEXIST)
    {
        return E_ATTRNOTEXIST;
    }

    /*** convert strVal (string) to the correct attribute type ***/
    int type = attrCatEntry.attrType;
    Attribute attrVal;

    if (type == NUMBER)
    {
        if (isNumber(strVal))
        {
            // convert the string to a number
            attrVal.nVal = atof(strVal);
        }
        else
        {
            return E_ATTRTYPEMISMATCH;
        }
    }
    else if (type == STRING)
    {
        strcpy(attrVal.sVal, strVal);
    }

    /*** reset search so we start from the first record ***/
    RelCacheTable::resetSearchIndex(srcRelId);

    /*** get the relation catalog entry to know numAttrs ***/
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    /*** print the header row with attribute names ***/
    printf("|");
    for (int i = 0; i < relCatEntry.numAttrs; i++)
    {
        AttrCatEntry attrEntry;
        // get each attribute by offset to print its name
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrEntry);
        printf(" %s |", attrEntry.attrName);
    }
    printf("\n");

    /*** search and print matching records ***/
    while (true)
    {
        // get the next matching record
        RecId searchRes = BlockAccess::linearSearch(srcRelId, attr, attrVal, op);

        if (searchRes.block != -1 && searchRes.slot != -1)
        {
            // a matching record was found — read and print it
            RecBuffer blockBuffer(searchRes.block);

            Attribute record[relCatEntry.numAttrs];
            blockBuffer.getRecord(record, searchRes.slot);

            // print each attribute value
            printf("|");
            for (int i = 0; i < relCatEntry.numAttrs; i++)
            {
                AttrCatEntry attrEntry;
                AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrEntry);

                if (attrEntry.attrType == NUMBER)
                {
                    printf(" %g |", record[i].nVal);
                }
                else
                {
                    printf(" %s |", record[i].sVal);
                }
            }
            printf("\n");
        }
        else
        {
            // no more matching records
            break;
        }
    }

    return SUCCESS;
}