#include "inc/uefi.h"
#include <inc/stdio.h>
#include <inc/string.h>
#include <kern/uefi_f.h>

const char * memory_types[] = 
{
    "EfiReservedMemoryType", //0
    "EfiLoaderCode", // 1
    "EfiLoaderData", // 2
    "EfiBootServicesCode", // 3
    "EfiBootServicesData",// 4
    "EfiRuntimeServicesCode", // 5 
    "EfiRuntimeServicesData",// 6 
    "EfiConventionalMemory",// 7
    "EfiUnusableMemory", // 8 
    "EfiACPIReclaimMemory",// 9 
    "EfiACPIMemoryNVS",// 10 
    "EfiMemoryMappedIO", //11
    "EfiMemoryMappedIOPortSpace", //12 
    "EfiPalCode", //13
    "EfiPersistentMemory", //14
    "EfiMaxMemoryType", //15
};

const char * alocate_types[] = 
{
    "AllocateAnyPages",
    "AllocateMaxAddress",
    "AllocateAddress",
    "MaxAllocateType",
};

#define LP_pointer 0x1EB9EC90 // Realizaation For -m 512 
// if -m 512 : 0x1EB9EC90
// if -m 128 : 0x06B9EC90
// if -m 1024: 0x3EB9EC90
// so i think we can't know it ?
#define SPAGES 4096


LOADER_PARAMS * UEFI_LP;

uint64_t AVAIBLE_MEMORY;
uint64_t MEMORY_MAP_SIZE;
uint64_t MEMORY_MAP_ADDR;



int init_memory_map()
{
    UEFI_LP=(void *)LP_pointer;
    uint8_t * MemoryMapPointer = (uint8_t * )UEFI_LP->Memory_Map +
            UEFI_LP->Memory_Map_Size - UEFI_LP->Memory_Map_Descriptor_Size ;
    //point into last element in memory_map    

    EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)MemoryMapPointer;

                // cprintf("  Type: %u,  %s \n", desc->Type,memory_types[desc->Type]); 
                // cprintf("  PhysicalStart: %016llx\n", desc->PhysicalStart);
                // cprintf("  VirtualStart: %016llx\n", desc->VirtualStart);  
                // cprintf("  NumberOfPages: %016llx   (4k)\n", desc->NumberOfPages);
                // cprintf("  Attribute: %016llx\n", desc->Attribute);  
                /// Test May Be useful

    //struct point

    AVAIBLE_MEMORY = desc->PhysicalStart + desc->NumberOfPages * SPAGES; // Avaible memory for working;
    MEMORY_MAP_SIZE = AVAIBLE_MEMORY / SPAGES * UEFI_LP->Memory_Map_Descriptor_Size;
    cprintf("  We have around : %016llx\n", AVAIBLE_MEMORY );
    cprintf("  We lost around : %016llx\n", MEMORY_MAP_SIZE);
    cprintf("  We lost pages around : %016llx\n", MEMORY_MAP_SIZE/UEFI_LP->Memory_Map_Descriptor_Size);
    // Now We Must Find place for new Memory Map 

    uint8_t * startOfMemoryMap = (void *)UEFI_LP->Memory_Map;
    uint8_t * endOfMemoryMap   = startOfMemoryMap + UEFI_LP->Memory_Map_Size;
    uint8_t * offset           = startOfMemoryMap;
    //cprintf("Memory_Map_Size pointer addr %p\r\n", UEFI_LP->Memory_Map); 

    while (offset < endOfMemoryMap)
    {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)(offset);
        // cprintf(" C: Type: %u,  %s \n", desc->Type,memory_types[desc->Type]);
        if ( (desc->Type == EfiConventionalMemory) && ( desc->NumberOfPages >  MEMORY_MAP_SIZE / SPAGES) )
        {
            MEMORY_MAP_ADDR = desc->PhysicalStart;
            break;
        }    
        offset += UEFI_LP->Memory_Map_Descriptor_Size;
    }    

    cprintf("  We find around : %016llx\n", MEMORY_MAP_ADDR);

    // We Must Initialize New Memory Map Array;

    //***********************************************//
    
       uint32_t DungerousPlace = (uint32_t) MEMORY_MAP_ADDR;
       uint8_t * LowBorder = (uint8_t * ) DungerousPlace;
     
    //***********************************************//
    
    startOfMemoryMap = (void *)UEFI_LP->Memory_Map;
    endOfMemoryMap   = startOfMemoryMap + UEFI_LP->Memory_Map_Size;
    offset           = startOfMemoryMap;
    uint8_t * memmap_offset = LowBorder;
    uint64_t  memadr_offset = 0;

    while (offset < endOfMemoryMap)
    {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)(offset);
        EFI_MEMORY_DESCRIPTOR  desctmp = *desc;
        desc->NumberOfPages=1;
        if(desc->PhysicalStart == MEMORY_MAP_ADDR) desc->Type=EfiMaxMemoryType;
        for (int i = 0; i < desctmp.NumberOfPages ; ++i)
        {
            desc->PhysicalStart = memadr_offset;
            if ( i >= MEMORY_MAP_SIZE / SPAGES ) {desc->Type=EfiConventionalMemory;}
            memcpy(memmap_offset,offset,UEFI_LP->Memory_Map_Descriptor_Size);
            memmap_offset+=UEFI_LP->Memory_Map_Descriptor_Size;
            memadr_offset+=SPAGES;
            if ( memadr_offset == 0xA0000 ) {memadr_offset=0x100000;} // I thing UEFI wrong, but who i am
        }
        *desc = desctmp;        
        offset += UEFI_LP->Memory_Map_Descriptor_Size;
    }

    return 0;
}

int PrintMemoryMap()
{

    UEFI_LP =  (void *)LP_pointer;

    //***********************************************//
    
       uint32_t DungerousPlace = (uint32_t) MEMORY_MAP_ADDR;
       uint8_t * LowBorder = (uint8_t * ) DungerousPlace;
     
    //***********************************************//


    uint8_t *startOfMemoryMap = LowBorder;
    uint8_t *endOfMemoryMap = startOfMemoryMap + MEMORY_MAP_SIZE ;
    uint8_t *offset = startOfMemoryMap;
    uint32_t counter = 0;
    int flag = 0;
    uint64_t num  = 0;
    EFI_MEMORY_DESCRIPTOR *desctmp = NULL;


    while (offset < endOfMemoryMap)
    {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)offset;
   
        if(flag)
        {
            if (desc->Type != desctmp->Type) // Maybe was worked
            {
                cprintf("Map %d:\n", counter);
                cprintf("  Type: %u,  %s \n", desctmp->Type,memory_types[desctmp->Type]); 
                cprintf("  PhysicalStart: %016llx\n", desctmp->PhysicalStart);
                cprintf("  VirtualStart: %016llx\n", desctmp->VirtualStart);  
                cprintf("  NumberOfPages: %016llx   (4k)\n", num);
                cprintf("  Attribute: %016llx\n", desctmp->Attribute);    
                desctmp = (EFI_MEMORY_DESCRIPTOR *) desc;
                counter++;
                flag=1;
                num=0;  
            } 

            num=num+desc->NumberOfPages;
        }
        else 
        {
            desctmp = (EFI_MEMORY_DESCRIPTOR *) desc;
            flag=1;
            num=0;
            num=num+desc->NumberOfPages;
        }
        counter--;
        offset += UEFI_LP->Memory_Map_Descriptor_Size;
        counter++;
    }
    return 0;
}

// INFO   TEST   FUCTION
int LP_info()
{
	UEFI_LP=(void *)LP_pointer;
	cprintf("Memory_Map_Descriptor_Size pointer addr %p\r\n", &(UEFI_LP->Memory_Map_Descriptor_Size));
    cprintf("Memory_Map pointer addr %p\r\n", &(UEFI_LP->Memory_Map));
    cprintf("Memory_Map_Size pointer addr %p\r\n", &(UEFI_LP->Memory_Map_Size));
    cprintf("Kernel_BaseAddress pointer addr %p\r\n", &(UEFI_LP->Kernel_BaseAddress));
    cprintf("Kernel_Pages pointer addr %p\r\n", &(UEFI_LP->Kernel_Pages));
    cprintf("ESP_Root_Device_Path pointer addr %p\r\n", &(UEFI_LP->ESP_Root_Device_Path));
    cprintf("ESP_Root_Size pointer addr %p\r\n", &(UEFI_LP->ESP_Root_Size));
    cprintf("Kernel_Path pointer addr %p\r\n", &(UEFI_LP->Kernel_Path));
    cprintf("Kernel_Path_Size pointer addr %p\r\n", &(UEFI_LP->Kernel_Path_Size));
    cprintf("Kernel_Options pointer addr %p\r\n", &(UEFI_LP->Kernel_Options));
    cprintf("Kernel_Options_Size pointer addr %p\r\n", &(UEFI_LP->Kernel_Options_Size));
    cprintf("RTServices pointer addr %p\r\n", &(UEFI_LP->RTServices));
    cprintf("GPU_Configs pointer addr %p\n", &(UEFI_LP->GPU_Configs));
	return 0;
}
// Return information about addresses in struct LOADER_PARAMS;

///*************************************************************************************///
// предпологаем, что адрес выровнен по страницам т.е. адрес указывает на начало страницы //
// для этого достаточно, чтобы последние три цифры адреса тождественно равнялись 0.      //
///*************************************************************************************///

EFI_ALLOCATE_ERROR
AllocatePages( EFI_ALLOCATE_TYPE a_type, EFI_MEMORY_TYPE m_type, UINTN pages, EFI_PHYSICAL_ADDRESS * mem ) 
{
    if ((a_type != AllocateAnyPages) && (a_type != AllocateMaxAddress) && (a_type != AllocateAddress)) 
    return EFI_INVALID_PARAMETER; // standart

    if ( (mem!=NULL) && (*mem%SPAGES) ) return EFI_INVALID_PARAMETER; // I think this right

    if ((mem == NULL) && (a_type != AllocateAnyPages) ) return EFI_INVALID_PARAMETER; // standart

    if (m_type == EfiPersistentMemory) return EFI_INVALID_PARAMETER;

    if ((m_type >= EfiMaxMemoryType) && (m_type <= 0x6FFFFFFF)) return EFI_INVALID_PARAMETER;

    //cprintf("HI HIH IH\n");

    UEFI_LP=(void *)LP_pointer;
    //***********************************************//
    
       uint32_t DungerousPlace = (uint32_t) MEMORY_MAP_ADDR;
       uint8_t * LowBorder = (uint8_t * ) DungerousPlace;
     
    //***********************************************//

    uint8_t * startOfMemoryMap = LowBorder;
    uint8_t * endOfMemoryMap = startOfMemoryMap + MEMORY_MAP_SIZE;
    uint8_t *offset = startOfMemoryMap;
    
    uint8_t *save_offset=NULL;
    UINTN num_pages = pages;

    if ( a_type == AllocateAnyPages )
    {
        while( offset < endOfMemoryMap )
        {
            EFI_MEMORY_DESCRIPTOR * desc = (EFI_MEMORY_DESCRIPTOR *)offset;
            if( desc->Type == EfiConventionalMemory )
            {
                if (save_offset == NULL) {save_offset=offset; num_pages=pages-1;}
                else{num_pages--;}
            
                if(num_pages == 0)
                {
                    * mem = desc->PhysicalStart;
                    for (int i = 0; i < pages; ++i)
                    {
                        EFI_MEMORY_DESCRIPTOR * desctmp = (EFI_MEMORY_DESCRIPTOR *)save_offset;
                        desctmp->Type=m_type;
                        save_offset += UEFI_LP->Memory_Map_Descriptor_Size;
                    } 
                    return EFI_SUCCESS;
                } 
            }
            else
            {
                num_pages=pages;
                save_offset=NULL;
            }
           

            offset += UEFI_LP->Memory_Map_Descriptor_Size;
        } 
        return EFI_OUT_OF_RESOURCES;
    }



    if(a_type == AllocateMaxAddress )
    {
        
        while( offset < endOfMemoryMap )
        {
            EFI_MEMORY_DESCRIPTOR * desc = (EFI_MEMORY_DESCRIPTOR *)offset;

            if( desc->PhysicalStart > *mem) { return EFI_OUT_OF_RESOURCES; }

            if( desc->Type == EfiConventionalMemory )
            {
                if (save_offset == NULL) {save_offset=offset; num_pages=pages-1;}
                else{num_pages--;}
            }
            else
            {
                num_pages=pages;
                save_offset=NULL;
            }
            if(num_pages == 0)
            {
                * mem = desc->PhysicalStart;
                for (int i = 0; i < pages; ++i)
                {
                    EFI_MEMORY_DESCRIPTOR * desctmp = (EFI_MEMORY_DESCRIPTOR *)save_offset;
                    desctmp->Type=m_type;
                    save_offset += UEFI_LP->Memory_Map_Descriptor_Size;
                } 
                return EFI_SUCCESS;
            } 
            offset += UEFI_LP->Memory_Map_Descriptor_Size;
        } 
        return EFI_OUT_OF_RESOURCES;
    }

    if(a_type == AllocateAddress)
    {
        
        if( (*mem) > AVAIBLE_MEMORY ) return EFI_NOT_FOUND; // We can use only page under this addres; Sorry :( my mistake         

        while( offset < endOfMemoryMap )
        
        {
            EFI_MEMORY_DESCRIPTOR * desc = (EFI_MEMORY_DESCRIPTOR *)offset;
            if( (desc->Type != EfiConventionalMemory) && (desc->PhysicalStart > *mem) ) return EFI_OUT_OF_RESOURCES;

            if( (desc->Type == EfiConventionalMemory) && (desc->PhysicalStart == *mem) )
            {
               save_offset=offset;
               for (int i = 0; i < pages; ++i)
                {
                    EFI_MEMORY_DESCRIPTOR * desctmp = (EFI_MEMORY_DESCRIPTOR *)save_offset;
                    if (desctmp->Type != EfiConventionalMemory) return EFI_OUT_OF_RESOURCES;
                    save_offset += UEFI_LP->Memory_Map_Descriptor_Size;
                }  
                save_offset=offset;
                for (int i = 0; i < pages; ++i)
                {
                    EFI_MEMORY_DESCRIPTOR * desctmp = (EFI_MEMORY_DESCRIPTOR *)save_offset;
                    desctmp->Type=m_type;
                    save_offset += UEFI_LP->Memory_Map_Descriptor_Size;
                } 
                return EFI_SUCCESS;
            } 

            offset += UEFI_LP->Memory_Map_Descriptor_Size;
        } 
        return EFI_OUT_OF_RESOURCES;
    } 


return EFI_OUT_OF_RESOURCES;   
}



EFI_ALLOCATE_ERROR
FreePages(  EFI_PHYSICAL_ADDRESS * mem , UINTN pages ) 
{
    if ( (mem != NULL) && (*mem%SPAGES)) return EFI_INVALID_PARAMETER;

    if( *mem > AVAIBLE_MEMORY ) return EFI_INVALID_PARAMETER; // we can only use address under
    
    UEFI_LP=(void *)LP_pointer;

    //***********************************************//
    
       uint32_t DungerousPlace = (uint32_t) MEMORY_MAP_ADDR;
       uint8_t * LowBorder = (uint8_t * ) DungerousPlace;
     
    //***********************************************//

    uint8_t * startOfMemoryMap = LowBorder;
    uint8_t * endOfMemoryMap = startOfMemoryMap + MEMORY_MAP_SIZE;
    uint8_t * offset = startOfMemoryMap;
    uint8_t * save_offset=NULL;
    //UINTN num_pages = pages;

    while( offset < endOfMemoryMap ) 
    {
        EFI_MEMORY_DESCRIPTOR * desc = ( EFI_MEMORY_DESCRIPTOR * ) offset;
        if( (desc->PhysicalStart > *mem) ) return EFI_NOT_FOUND;
        if( (desc->PhysicalStart == *mem) )
        {
            save_offset=offset;
            for (int i = 0; i < pages; ++i)
            {
                EFI_MEMORY_DESCRIPTOR * desctmp = (EFI_MEMORY_DESCRIPTOR *)save_offset;
                desctmp->Type=EfiConventionalMemory;
                save_offset += UEFI_LP->Memory_Map_Descriptor_Size;
            } 
            return EFI_SUCCESS;

        }    
        offset += UEFI_LP->Memory_Map_Descriptor_Size;
    }
    return EFI_SUCCESS;
}














