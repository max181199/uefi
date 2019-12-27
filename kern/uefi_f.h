#include "inc/uefi.h"


int LP_info();
int PrintMemoryMap();
int init_memory_map();
EFI_ALLOCATE_ERROR AllocatePages( EFI_ALLOCATE_TYPE a_type, EFI_MEMORY_TYPE m_type, UINTN pages, EFI_PHYSICAL_ADDRESS * mem );
EFI_ALLOCATE_ERROR FreePages(  EFI_PHYSICAL_ADDRESS * mem , UINTN pages ); 