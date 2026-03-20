#include <cstring> // for strcmp
#include "Buffer/BlockBuffer.h"
#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[])
{
  // ============================================================
  // NORMAL RUN MODE
  // ============================================================
  // Order matters!
  // 1. Disk creates the run copy
  // 2. StaticBuffer initialises the 32-slot buffer
  // 3. OpenRelTable loads RelCat and AttrCat into cache
  // ============================================================

  Disk disk_run;

  StaticBuffer buffer;

  OpenRelTable cache;

  // FrontendInterface runs in an infinite loop
  // receiving commands from the user
  // exit gracefully using the "exit" command (not Ctrl+C!)
  return FrontendInterface::handleFrontend(argc, argv);
}