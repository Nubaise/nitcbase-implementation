#include "Buffer/BlockBuffer.h"
#include "Disk_Class/Disk.h"
#include "define/constants.h"
#include <cstdio>
#include <cstring>

int main(int argc, char *argv[])
{
  Disk disk_run;

  RecBuffer relCatBuffer(RELCAT_BLOCK);

  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);

  // Exercise 2 - change "Class" to "Batch" in Students relation
  int attrCatBlockNum = ATTRCAT_BLOCK;

  while (attrCatBlockNum != -1)
  {
    RecBuffer attrCatBuffer(attrCatBlockNum);

    HeadInfo attrCatHeader;
    attrCatBuffer.getHeader(&attrCatHeader);

    unsigned char block[BLOCK_SIZE];
    Disk::readBlock(block, attrCatBlockNum);

    for (int j = 0; j < attrCatHeader.numEntries; j++)
    {

      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
      attrCatBuffer.getRecord(attrCatRecord, j);

      if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Students") == 0 &&
          strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Class") == 0)
      {

        int recordSize = ATTRCAT_NO_ATTRS * ATTR_SIZE;
        int slotMapSize = attrCatHeader.numSlots;
        unsigned char *slotPointer = block + HEADER_SIZE + slotMapSize + (recordSize * j);

        memset(slotPointer + ATTR_SIZE, 0, ATTR_SIZE);
        memcpy(slotPointer + ATTR_SIZE, "Batch", strlen("Batch"));

        Disk::writeBlock(block, attrCatBlockNum);

        printf("Changed 'Class' to 'Batch' in Students\n\n");
        break;
      }
    }

    attrCatBlockNum = attrCatHeader.rblock;
  }

  // print all relations
  for (int i = 0; i < relCatHeader.numEntries; i++)
  {

    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(relCatRecord, i);

    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    int attrCatBlockNum = ATTRCAT_BLOCK;

    while (attrCatBlockNum != -1)
    {
      RecBuffer attrCatBuffer(attrCatBlockNum);

      HeadInfo attrCatHeader;
      attrCatBuffer.getHeader(&attrCatHeader);

      for (int j = 0; j < attrCatHeader.numEntries; j++)
      {

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, j);

        if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                   relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0)
        {

          const char *attrType =
              attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";

          printf("  %s: %s\n",
                 attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
        }
      }

      attrCatBlockNum = attrCatHeader.rblock;
    }

    printf("\n");
  }

  return 0;
}