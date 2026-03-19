#include "Buffer/BlockBuffer.h"
#include "Buffer/StaticBuffer.h"
#include "Cache/AttrCacheTable.h"
#include "Cache/OpenRelTable.h"
#include "Cache/RelCacheTable.h"
#include "Disk_Class/Disk.h"
#include "define/constants.h"
#include <cstdio>
#include <cstring>

int main(int argc, char *argv[])
{

  // IMPORTANT: order matters here!
  // 1. Disk() creates the run copy of the disk
  // 2. StaticBuffer() initialises the 32-slot buffer
  // 3. OpenRelTable() loads RelCat and AttrCat into the cache
  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  // loop through rel-id 0, 1 and 2 (RELATIONCAT, ATTRIBUTECAT, Students)
  for (int i = 0; i <= 2; i++)
  {

    // get the relation catalog entry for this rel-id from the cache
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(i, &relCatEntry);

    printf("Relation: %s\n", relCatEntry.relName);

    // loop through all attributes of this relation using their offset (0, 1, 2...)
    for (int j = 0; j < relCatEntry.numAttrs; j++)
    {

      // get the attribute catalog entry for offset j from the cache
      AttrCatEntry attrCatEntry;
      AttrCacheTable::getAttrCatEntry(i, j, &attrCatEntry);

      const char *attrType = attrCatEntry.attrType == NUMBER ? "NUM" : "STR";
      printf("  %s: %s\n", attrCatEntry.attrName, attrType);
    }
    printf("\n");
  }

  return 0;
}