//==================================================================================================================================
//  Simple UEFI Bootloader: Kernel Loader and Entry Point Jump
//==================================================================================================================================
//
// Version 2.2
//
// Author:
//  KNNSpeed
//
// Source Code:
//  https://github.com/KNNSpeed/Simple-UEFI-Bootloader
//
// This file contains the multiplatform kernel file loader and bootstrapper.
//

#include "Bootloader.h"
/*
#define LOADER_DEBUG_ENABLED 1
#define ELF_LOADER_DEBUG_ENABLED 1
#define FINAL_LOADER_DEBUG_ENABLED 1
#define MEMMAP_PRINT_ENABLED 1
#define MEMORY_DEBUG_ENABLED 1
#define MEMORY_CHECK_INFO 1
*/
//==================================================================================================================================
//  GoTime: Kernel Loader
//==================================================================================================================================
//
// Load Kernel (64-bit PE32+, ELF, or Mach-O), exit boot services, and jump to the entry point of kernel file
//

EFI_STATUS GoTime(EFI_HANDLE ImageHandle, GPU_CONFIG * Graphics, EFI_CONFIGURATION_TABLE * SysCfgTables, UINTN NumSysCfgTables, UINT32 UEFIVer)
{
#ifdef GOP_DEBUG_ENABLED
  // Integrity check
  for(UINT64 k = 0; k < Graphics->NumberOfFrameBuffers; k++)
  {
    Print(L"GPU Mode: %u of %u\r\n", Graphics->GPUArray[k].Mode, Graphics->GPUArray[k].MaxMode - 1);
    Print(L"GPU FB: 0x%016llx\r\n", Graphics->GPUArray[k].FrameBufferBase);
    Print(L"GPU FB Size: 0x%016llx\r\n", Graphics->GPUArray[k].FrameBufferSize);
    Print(L"GPU SizeOfInfo: %u Bytes\r\n", Graphics->GPUArray[k].SizeOfInfo);
    Print(L"GPU Info Ver: 0x%x\r\n", Graphics->GPUArray[k].Info->Version);
    Print(L"GPU Info Res: %ux%u\r\n", Graphics->GPUArray[k].Info->HorizontalResolution, Graphics->GPUArray[k].Info->VerticalResolution);
    Print(L"GPU Info PxFormat: 0x%x\r\n", Graphics->GPUArray[k].Info->PixelFormat);
    Print(L"GPU Info PxInfo (R,G,B,Rsvd Masks): 0x%08x, 0x%08x, 0x%08x, 0x%08x\r\n", Graphics->GPUArray[k].Info->PixelInformation.RedMask, Graphics->GPUArray[k].Info->PixelInformation.GreenMask, Graphics->GPUArray[k].Info->PixelInformation.BlueMask, Graphics->GPUArray[k].Info->PixelInformation.ReservedMask);
    Print(L"GPU Info PxPerScanLine: %u\r\n", Graphics->GPUArray[k].Info->PixelsPerScanLine);
    Keywait(L"\0");
  }
#endif

#ifdef LOADER_DEBUG_ENABLED
  Print(L"GO GO GO!!!\r\n");
#endif

  EFI_STATUS GoTimeStatus;

  // These hold data for the loader params at the end
  EFI_PHYSICAL_ADDRESS KernelBaseAddress = 0;
  UINTN KernelPages = 0;

  // Load kernel file from somewhere on this drive

	EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

  // Get a pointer to the (loaded image) pointer of BOOTX64.EFI
  // Pointer 1 -> Pointer 2 -> BOOTX64.EFI
  // OpenProtocol wants Pointer 1 as input to give you Pointer 2.
	GoTimeStatus = gST->BootServices->OpenProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"LoadedImage OpenProtocol error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  // Need these for later
  CHAR16 * ESPRootTemp = ConvertDevicePathToText(DevicePathFromHandle(LoadedImage->DeviceHandle), FALSE, FALSE);

  UINT64 ESPRootSize = StrSize(ESPRootTemp);

  // DevicePathToStr allocates memory of type Loadedimage->ImageDataType (this is set by firmware)
  // Instead we want a known data type, so reallocate it:
  CHAR16 * ESPRoot;

  GoTimeStatus = gST->BootServices->AllocatePool(EfiLoaderData, ESPRootSize, (void**)&ESPRoot);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"ESPRoot AllocatePool error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  CopyMem(ESPRoot, ESPRootTemp, ESPRootSize);

  GoTimeStatus = gBS->FreePool(ESPRootTemp);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"Error freeing ESPRootTemp pool. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;

  // Parent device of BOOTX64.EFI (the ImageHandle originally passed in is this very file)
  // Loadedimage is an EFI_LOADED_IMAGE_PROTOCOL pointer that points to BOOTX64.EFI
  GoTimeStatus = gST->BootServices->OpenProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"FileSystem OpenProtocol error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  EFI_FILE *CurrentDriveRoot;

  GoTimeStatus = FileSystem->OpenVolume(FileSystem, &CurrentDriveRoot);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"OpenVolume error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

///
  // Below Kernel64.txt loading & parsing code adapted from V2.1 of https://github.com/KNNSpeed/UEFI-Stub-Loader

  // Locate Kernel64.txt, which should be in the same directory as this program
  // ((FILEPATH_DEVICE_PATH*)LoadedImage->FilePath)->PathName is, e.g., \EFI\BOOT\BOOTX64.EFI

  CHAR16 * BootFilePath = ((FILEPATH_DEVICE_PATH*)LoadedImage->FilePath)->PathName;

#ifdef LOADER_DEBUG_ENABLED
  Print(L"BootFilePath: %s\r\n", BootFilePath);
#endif

  UINTN TxtFilePathPrefixLength = 0;
  UINTN BootFilePathLength = 0;

  while(BootFilePath[BootFilePathLength] != L'\0')
  {
    if(BootFilePath[BootFilePathLength] == L'\\')
    {
      TxtFilePathPrefixLength = BootFilePathLength; // Could use ++BootFilePathLength here instead of the two separate += below, but it's less clear and it doesn't make any meaningful difference to do so.
    }
    BootFilePathLength++;
  }
  BootFilePathLength += 1; // For Null Term
  TxtFilePathPrefixLength += 1; // To account for the last '\' in the file path (file path prefix does not get null-terminated)

#ifdef LOADER_DEBUG_ENABLED
  Print(L"BootFilePathLength: %llu, TxtFilePathPrefixLength: %llu, BootFilePath Size: %llu \r\n", BootFilePathLength, TxtFilePathPrefixLength, StrSize(BootFilePath));
  Keywait(L"\0");
#endif

  CONST CHAR16 TxtFileName[13] = L"Kernel64.txt";

  UINTN TxtFilePathPrefixSize = TxtFilePathPrefixLength * sizeof(CHAR16);
  UINTN TxtFilePathSize = TxtFilePathPrefixSize + sizeof(TxtFileName);

  CHAR16 * TxtFilePath;

  GoTimeStatus = gST->BootServices->AllocatePool(EfiBootServicesData, TxtFilePathSize, (void**)&TxtFilePath);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"TxtFilePathPrefix AllocatePool error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  // Don't really need this. Data is measured to be the right size, meaning every byte in TxtFilePath gets overwritten.
//  ZeroMem(TxtFilePath, TxtFilePathSize);

  CopyMem(TxtFilePath, BootFilePath, TxtFilePathPrefixSize);
  CopyMem(&TxtFilePath[TxtFilePathPrefixLength], TxtFileName, sizeof(TxtFileName));

#ifdef LOADER_DEBUG_ENABLED
  Print(L"TxtFilePath: %s, TxtFilePath Size: %llu\r\n", TxtFilePath, TxtFilePathSize);
  Keywait(L"\0");
#endif

  // Get ready to open the Kernel64.txt file
  EFI_FILE *KernelcmdFile;

  // Open the Kernel64.txt file and assign it to the KernelcmdFile EFI_FILE variable
  // It turns out the Open command can support directory trees with "\" like in Windows. Neat!
  GoTimeStatus = CurrentDriveRoot->Open(CurrentDriveRoot, &KernelcmdFile, TxtFilePath, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
  if (EFI_ERROR(GoTimeStatus))
  {
    Keywait(L"Kernel64.txt file is missing\r\n");
    return GoTimeStatus;
  }

#ifdef LOADER_DEBUG_ENABLED
  Keywait(L"Kernel64.txt file opened.\r\n");
#endif

  // Now to get Kernel64.txt's file size
  UINTN Txt_FileInfoSize;
  // Need to know the size of the file metadata to get the file metadata...
  GoTimeStatus = KernelcmdFile->GetInfo(KernelcmdFile, &gEfiFileInfoGuid, &Txt_FileInfoSize, NULL);
  // GetInfo will intentionally error out and provide the correct Txt_FileInfoSize value

#ifdef LOADER_DEBUG_ENABLED
  Print(L"Txt_FileInfoSize: %llu Bytes\r\n", Txt_FileInfoSize);
#endif

  // Prep metadata destination
  EFI_FILE_INFO *Txt_FileInfo;
  // Reserve memory for file info/attributes and such to prevent it from getting run over
  GoTimeStatus = gST->BootServices->AllocatePool(EfiBootServicesData, Txt_FileInfoSize, (void**)&Txt_FileInfo);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"Txt_FileInfo AllocatePool error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  // Actually get the metadata
  GoTimeStatus = KernelcmdFile->GetInfo(KernelcmdFile, &gEfiFileInfoGuid, &Txt_FileInfoSize, Txt_FileInfo);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"GetInfo error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

#ifdef SHOW_KERNEL_METADATA
  // Show metadata
  Print(L"FileName: %s\r\n", Txt_FileInfo->FileName);
  Print(L"Size: %llu\r\n", Txt_FileInfo->Size);
  Print(L"FileSize: %llu\r\n", Txt_FileInfo->FileSize);
  Print(L"PhysicalSize: %llu\r\n", Txt_FileInfo->PhysicalSize);
  Print(L"Attribute: %llx\r\n", Txt_FileInfo->Attribute);
/*
  NOTE: Attributes:

  #define EFI_FILE_READ_ONLY 0x0000000000000001
  #define EFI_FILE_HIDDEN 0x0000000000000002
  #define EFI_FILE_SYSTEM 0x0000000000000004
  #define EFI_FILE_RESERVED 0x0000000000000008
  #define EFI_FILE_DIRECTORY 0x0000000000000010
  #define EFI_FILE_ARCHIVE 0x0000000000000020
  #define EFI_FILE_VALID_ATTR 0x0000000000000037

*/
  Print(L"Created: %02hhu/%02hhu/%04hu - %02hhu:%02hhu:%02hhu.%u\r\n", Txt_FileInfo->CreateTime.Month, Txt_FileInfo->CreateTime.Day, Txt_FileInfo->CreateTime.Year, Txt_FileInfo->CreateTime.Hour, Txt_FileInfo->CreateTime.Minute, Txt_FileInfo->CreateTime.Second, Txt_FileInfo->CreateTime.Nanosecond);
  Print(L"Last Modified: %02hhu/%02hhu/%04hu - %02hhu:%02hhu:%02hhu.%u\r\n", Txt_FileInfo->ModificationTime.Month, Txt_FileInfo->ModificationTime.Day, Txt_FileInfo->ModificationTime.Year, Txt_FileInfo->ModificationTime.Hour, Txt_FileInfo->ModificationTime.Minute, Txt_FileInfo->ModificationTime.Second, Txt_FileInfo->ModificationTime.Nanosecond);
  Keywait(L"\0");
#endif

  // Read text file into memory now that we know the file size
  CHAR16 * KernelcmdArray;
  // Reserve memory for text file
  GoTimeStatus = gST->BootServices->AllocatePool(EfiBootServicesData, Txt_FileInfo->FileSize, (void**)&KernelcmdArray);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"KernelcmdArray AllocatePool error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  // Actually read the file
UINTN Txt_File_Size = Txt_FileInfo->FileSize;
  GoTimeStatus = KernelcmdFile->Read(KernelcmdFile, &Txt_File_Size, KernelcmdArray);
Txt_FileInfo->FileSize = Txt_File_Size;
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"KernelcmdArray read error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

#ifdef LOADER_DEBUG_ENABLED
  Keywait(L"KernelcmdFile read into memory.\r\n");
#endif

  // UTF-16 format check
  UINT16 BOM_check = UTF16_BOM_LE;
  if(!compare(KernelcmdArray, &BOM_check, 2))
  {
    BOM_check = UTF16_BOM_BE; // Check endianness
    if(compare(KernelcmdArray, &BOM_check, 2))
    {
      Print(L"Error: Kernel64.txt has the wrong endianness for this system.\r\n");
    }
    else // Probably missing a BOM
    {
      Print(L"Error: Kernel64.txt not formatted as UTF-16/UCS-2 with BOM.\r\n\n");
      Print(L"Q: What is a BOM?\r\n\n");
      Print(L"A: The BOM (Byte Order Mark) is a 2-byte identification sequence\r\n");
      Print(L"(U+FEFF) at the start of a UTF16/UCS-2-encoded file.\r\n");
      Print(L"Unfortunately not all editors add it in, and without\r\n");
      Print(L"a BOM present programs like this one cannot easily tell that a\r\n");
      Print(L"text file is encoded in UTF16/UCS-2.\r\n\n");
      Print(L"Windows Notepad & Wordpad and Linux gedit & xed all add BOMs when\r\n");
      Print(L"saving files as .txt with encoding set to \"Unicode\" (Windows)\r\n");
      Print(L"or \"UTF16\" (Linux), so use one of them to make Kernel64.txt.\r\n\n");
    }
    Keywait(L"Please fix the file and try again.\r\n");
    return GoTimeStatus;
  }
  // Parse Kernel64.txt file for location of kernel image and command line
  // Kernel image location line will be of format e.g. \EFI\ubuntu\vmlinuz.efi followed by \n or \r\n
  // Command line will just go until the next \n or \r\n, and should just be loaded as a UTF-16 string

  // Get size of kernel image path & command line and populate the data retention variables
  UINT64 FirstLineLength = 0;
  UINT64 KernelPathSize = 0;

  for(UINT64 i = 1; i < ((Txt_FileInfo->FileSize) >> 1); i++) // i starts at 1 to skip the BOM, ((Txt_FileInfo->FileSize) >> 1) is the number of 2-byte units in the file
  {
    if(KernelcmdArray[i] == L'\n')
    {
      // Account for the L'\n'
      FirstLineLength = i + 1;
      // The extra +1 is to start the command line parse in the correct place
      break;
    }
    else if(KernelcmdArray[i] == L'\r')
    {
      // There'll be a \n after the \r
      FirstLineLength = i + 1 + 1;
      // The extra +1 is to start the command line parse in the correct place
      break;
    }

    if(KernelcmdArray[i] != L' ') // There might be an errant space or two. Ignore them.
    {
      KernelPathSize++;
    }
  }
  UINT64 KernelPathLen = KernelPathSize; // Need this for later
  // Need to add null terminator. Multiply by size of CHAR16 (2 bytes) to get size.
  KernelPathSize = (KernelPathSize + 1) << 1; // (KernelPathSize + 1) * sizeof(CHAR16)

#ifdef LOADER_DEBUG_ENABLED
  Print(L"KernelPathSize: %llu\r\n", KernelPathSize);
#endif

  // Command line's turn
  UINT64 CmdlineSize = 0; // Interestingly, the Linux kernel only takes 256 to 4096 chars for load options depending on architecture. Here's 2^63 UTF-16 characters (-1 to account for null terminator).

  for(UINT64 j = FirstLineLength; j < ((Txt_FileInfo->FileSize) >> 1); j++)
  {
    if((KernelcmdArray[j] == L'\n') || (KernelcmdArray[j] == L'\r')) // Reached the end of the line
    {
      break;
    }

    CmdlineSize++;
  }
  UINT64 CmdlineLen = CmdlineSize; // Need this for later
  // Need to add null terminator. Multiply by size of CHAR16 (2 bytes) to get size.
  CmdlineSize = (CmdlineSize + 1) << 1; // (CmdlineSize + 1) * sizeof(CHAR16)

#ifdef LOADER_DEBUG_ENABLED
  Print(L"CmdlineSize: %llu\r\n", CmdlineSize);
#endif

  CHAR16 * KernelPath; // EFI Kernel file's Path
  GoTimeStatus = gST->BootServices->AllocatePool(EfiLoaderData, KernelPathSize, (void**)&KernelPath);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"KernelPath AllocatePool error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  CHAR16 * Cmdline; // Command line to pass to EFI kernel
  GoTimeStatus = gST->BootServices->AllocatePool(EfiLoaderData, CmdlineSize, (void**)&Cmdline);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"Cmdline AllocatePool error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  for(UINT64 i = 1; i < FirstLineLength; i++)
  {
    if((KernelcmdArray[i] == L'\n') || (KernelcmdArray[i] == L'\r'))
    {
      break;
    }

    if(KernelcmdArray[i] != L' ') // There might be an errant space or two. Ignore them.
    {
      KernelPath[i-1] = KernelcmdArray[i]; // i-1 to ignore the 2 bytes of UTF-16 BOM
    }
  }
  KernelPath[KernelPathLen] = L'\0'; // Need to null-terminate this string

  // Command line's turn
  for(UINT64 j = FirstLineLength; j < ((Txt_FileInfo->FileSize) >> 1); j++)
  {
    if((KernelcmdArray[j] == L'\n') || (KernelcmdArray[j] == L'\r')) // Reached the end of the line
    {
      break;
    }

    Cmdline[j-FirstLineLength] = KernelcmdArray[j];
  }
  Cmdline[CmdlineLen] = L'\0'; // Need to null-terminate this string

#ifdef LOADER_DEBUG_ENABLED
  Print(L"Kernel image path: %s\r\nKernel image path size: %u\r\n", KernelPath, KernelPathSize);
  Print(L"Kernel command line: %s\r\nKernel command line size: %u\r\n", Cmdline, CmdlineSize);
  Keywait(L"Loading image... (might take a second or two after pressing a key)\r\n");
#endif

  // Free pools allocated from before as they are no longer needed
  GoTimeStatus = gBS->FreePool(TxtFilePath);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"Error freeing TxtFilePathPrefix pool. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  GoTimeStatus = gBS->FreePool(KernelcmdArray);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"Error freeing KernelcmdArray pool. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  GoTimeStatus = gBS->FreePool(Txt_FileInfo);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"Error freeing Txt_FileInfo pool. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

///

  EFI_FILE *KernelFile;

  // Open the kernel file from current drive root and point to it with KernelFile
	GoTimeStatus = CurrentDriveRoot->Open(CurrentDriveRoot, &KernelFile, KernelPath, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
	if (EFI_ERROR(GoTimeStatus))
  {
		Print(L"%s file is missing\r\n", KernelPath);
		return GoTimeStatus;
	}

#ifdef LOADER_DEBUG_ENABLED
  Keywait(L"Kernel file opened.\r\n");
#endif

  // Get address of start of file
  // ...Don't need to do this
//  UINT64 FileStartPosition;
//  KernelFile->GetPosition(KernelFile, &FileStartPosition);
//  Keywait(L"Kernel file start position acquired.\r\n");

  // Default ImageBase for 64-bit PE DLLs
  EFI_PHYSICAL_ADDRESS Header_memory = 0x400000;

  UINTN FileInfoSize;

  GoTimeStatus = KernelFile->GetInfo(KernelFile, &gEfiFileInfoGuid, &FileInfoSize, NULL);
  // GetInfo will intentionally error out and provide the correct fileinfosize value

#ifdef LOADER_DEBUG_ENABLED
  Print(L"FileInfoSize: %llu Bytes\r\n", FileInfoSize);
#endif

  EFI_FILE_INFO *FileInfo;
  GoTimeStatus = gST->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo); // Reserve memory for file info/attributes and such, to prevent it from getting run over
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"FileInfo AllocatePool error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

  // Actually get the metadata
  GoTimeStatus = KernelFile->GetInfo(KernelFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"GetInfo error. 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }

#ifdef SHOW_KERNEL_METADATA
  // Show metadata
  Print(L"FileName: %s\r\n", FileInfo->FileName);
  Print(L"Size: %llu\r\n", FileInfo->Size);
  Print(L"FileSize: %llu\r\n", FileInfo->FileSize);
  Print(L"PhysicalSize: %llu\r\n", FileInfo->PhysicalSize);
  Print(L"Attribute: %llx\r\n", FileInfo->Attribute);
/*
  NOTE: Attributes:

  #define EFI_FILE_READ_ONLY 0x0000000000000001
  #define EFI_FILE_HIDDEN 0x0000000000000002
  #define EFI_FILE_SYSTEM 0x0000000000000004
  #define EFI_FILE_RESERVED 0x0000000000000008
  #define EFI_FILE_DIRECTORY 0x0000000000000010
  #define EFI_FILE_ARCHIVE 0x0000000000000020
  #define EFI_FILE_VALID_ATTR 0x0000000000000037

*/
  Print(L"Created: %02hhu/%02hhu/%04hu - %02hhu:%02hhu:%02hhu.%u\r\n", FileInfo->CreateTime.Month, FileInfo->CreateTime.Day, FileInfo->CreateTime.Year, FileInfo->CreateTime.Hour, FileInfo->CreateTime.Minute, FileInfo->CreateTime.Second, FileInfo->CreateTime.Nanosecond);
  Print(L"Last Modified: %02hhu/%02hhu/%04hu - %02hhu:%02hhu:%02hhu.%u\r\n", FileInfo->ModificationTime.Month, FileInfo->ModificationTime.Day, FileInfo->ModificationTime.Year, FileInfo->ModificationTime.Hour, FileInfo->ModificationTime.Minute, FileInfo->ModificationTime.Second, FileInfo->ModificationTime.Nanosecond);
#endif

#ifdef LOADER_DEBUG_ENABLED
  Keywait(L"GetInfo memory allocated and populated.\r\n");
#endif

  // Read file header
//  UINTN size = 0x40; // Size of DOS header
  UINTN size = 0;

  // For the entry point jump, we need to know if the file uses ms_abi (is a PE image) or sysv_abi (*NIX image) calling convention
  UINT8 KernelisPE = 0;

  // Check header
  if (1) // Check if it's an ELF, if it's not a PE32+
  {
    //----------------------------------------------------------------------------------------------------------------------------------
    //  64-Bit ELF Loader
    //----------------------------------------------------------------------------------------------------------------------------------

    // Slightly less terrible way of doing this; just a placeholder.
    GoTimeStatus = KernelFile->SetPosition(KernelFile, 0);
    if(EFI_ERROR(GoTimeStatus))
    {
      Print(L"Reset SetPosition error (ELF). 0x%llx\r\n", GoTimeStatus);
      return GoTimeStatus;
    }

    struct Elf ELF32header;
    size = sizeof(ELF32header); // This works because it's not a pointer

    GoTimeStatus = KernelFile->Read(KernelFile, &size, &ELF32header);
    if(EFI_ERROR(GoTimeStatus))
    {
      Print(L"Header read error (ELF). 0x%llx\r\n", GoTimeStatus);
      return GoTimeStatus;
    }

#ifdef LOADER_DEBUG_ENABLED
    Keywait(L"ELF header read from file.\r\n");
#endif

    if(ELF32header.e_magic == ELF_MAGIC) // Check for \177ELF (hex: \xfELF)
    {
      // ELF!

#ifdef LOADER_DEBUG_ENABLED
      Keywait(L"ELF header passed.\r\n");
#endif

      // Check if 64-bit
      if(1)
      {

#ifdef LOADER_DEBUG_ENABLED
        Keywait(L"ELF32 header passed.\r\n");
#endif
/*
        if (ELF64header.e_type != ET_DYN)
        {
          GoTimeStatus = EFI_INVALID_PARAMETER;
          Print(L"Not a position-independent, executable ELF64 application...\r\n");
          Print(L"e_type: 0x%hx\r\n", ELF64header.e_type); // If it's 3, we're good and won't see this. Hopefully the image was compiled with -fpie and linked with -static-pie
          return GoTimeStatus;
        }
*/
#ifdef LOADER_DEBUG_ENABLED
        Keywait(L"Executable ELF32 header passed.\r\n");
#endif

        UINT64 i; // Iterator
        UINT64 virt_size = 0; // Virtual address max
        UINT64 virt_min = ~0ULL; // Minimum virtual address for page number calculation, -1 wraps around to max 64-bit number
        UINT64 Numofprogheaders = (UINT64)ELF32header.e_phnum;
        size = Numofprogheaders*(UINT64)ELF32header.e_phentsize; // Size of all program headers combined

        struct Proghdr * program_headers_table;

        GoTimeStatus = gST->BootServices->AllocatePool(EfiBootServicesData, size, (void**)&program_headers_table);
        if(EFI_ERROR(GoTimeStatus))
        {
          Print(L"Program headers table AllocatePool error. 0x%llx\r\n", GoTimeStatus);
          return GoTimeStatus;
        }

        GoTimeStatus = KernelFile->SetPosition(KernelFile, ELF32header.e_phoff); // Go to program headers
        if(EFI_ERROR(GoTimeStatus))
        {
          Print(L"Error setting file position for mapping (ELF). 0x%llx\r\n", GoTimeStatus);
          return GoTimeStatus;
        }
        GoTimeStatus = KernelFile->Read(KernelFile, &size, &program_headers_table[0]); // Run right over the section table, it should be exactly the size to hold this data
        if(EFI_ERROR(GoTimeStatus))
        {
          Print(L"Error reading program headers (ELF). 0x%llx\r\n", GoTimeStatus);
          return GoTimeStatus;
        }

        // Only want to include PT_LOAD segments
        for(i = 0; i < Numofprogheaders; i++) // Go through each section of the "sections" section to get the address boundary of the last section
        {
          struct Proghdr *specific_program_header = &program_headers_table[i];

#ifdef ELF_LOADER_DEBUG_ENABLED
          Print(L"current program address: 0x%x, size: 0x%x\r\n", specific_program_header->p_va, specific_program_header->p_memsz);
          Print(L"current program address + size 0x%x\r\n", specific_program_header->p_va + specific_program_header->p_memsz);
#endif

          virt_size = (virt_size > (specific_program_header->p_va + specific_program_header->p_memsz) ? virt_size: (specific_program_header->p_va + specific_program_header->p_memsz));

          if (specific_program_header->p_type == ELF_PROG_LOAD) {
            virt_min = (virt_min < (specific_program_header->p_va)) ? virt_min: (specific_program_header->p_va);
          }

        }

#ifdef ELF_LOADER_DEBUG_ENABLED
        Print(L"virt_size: 0x%llx, virt_min: 0x%llx, difference: 0x%llx\r\n", virt_size, virt_min, virt_size - virt_min);
        Keywait(L"Program Headers table passed.\r\n");
#endif

        // Virt_min is technically also the base address of the loadable segments
        UINT64 pages = EFI_SIZE_TO_PAGES(virt_size - virt_min); //To get number of pages (typically 4KB per), rounded up
        KernelPages = pages;

#ifdef ELF_LOADER_DEBUG_ENABLED
        Print(L"pages: %llu\r\n", pages);
#endif
        EFI_PHYSICAL_ADDRESS AllocatedMemory = 0x100000; // Default for ELF

#ifdef ELF_LOADER_DEBUG_ENABLED
        Print(L"Address of AllocatedMemory: 0x%llx\r\n", &AllocatedMemory);
#endif

        GoTimeStatus = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &AllocatedMemory);
        if(EFI_ERROR(GoTimeStatus))
        {
          Print(L"Could not allocate pages for ELF program segments. Error code: 0x%llx\r\n", GoTimeStatus);
          return GoTimeStatus;
        }

#ifdef ELF_LOADER_DEBUG_ENABLED
        Print(L"AllocatedMemory location: 0x%llx\r\n", AllocatedMemory);
  #ifdef MEMMAP_PRINT_ENABLED
        print_memmap();
        Keywait(L"Done printing MemMap.\r\n");
  #endif
        Keywait(L"Zeroing\r\n");
#endif

        // Zero the allocated pages
        ZeroMem((UINT8*)(uintptr_t)AllocatedMemory, (pages << EFI_PAGE_SHIFT));

#ifdef ELF_LOADER_DEBUG_ENABLED
        Keywait(L"MemZeroed\r\n");
#endif

#ifndef MEMORY_CHECK_DISABLED
        // If that memory isn't actually free due to weird firmware behavior...
        // Iterate through the entirety of what was just allocated and check to make sure it's all zeros
        // Start buggy firmware workaround
        if(VerifyZeroMem(pages << EFI_PAGE_SHIFT, AllocatedMemory))
        {

          // From UEFI Specification 2.7, Errata A (http://www.uefi.org/specifications):
          // MemoryType values in the range 0x80000000..0xFFFFFFFF are reserved for use by
          // UEFI OS loaders that are provided by operating system vendors.
  #ifdef MEMORY_CHECK_INFO
          Print(L"Non-zero memory location allocated. Verifying cause...\r\n");
  #endif
          // Compare what's there with the kernel file's first bytes; the system might have been reset and the non-zero
          // memory is what remains of last time. This can be safely overwritten to avoid cluttering up system RAM.

          // Sure hope there aren't any other page-aligned kernel images floating around in memory marked as free
          // Good thing we know what to expect!

          if(compare((EFI_PHYSICAL_ADDRESS*)AllocatedMemory, ELFMAG, SELFMAG))
          {
            // Do nothing, we're fine
  #ifdef MEMORY_CHECK_INFO
            Print(L"System was reset. No issues.\r\n");
  #endif
          }
          else // Not our remains, proceed with discovery of viable memory address
          {

  #ifdef MEMORY_CHECK_INFO
            Print(L"Searching for actually free memory...\r\nPerhaps the firmware is buggy?\r\n");
  #endif
            // Free the pages (well, return them to the system as they were...)
            GoTimeStatus = gBS->FreePages(AllocatedMemory, pages);
            if(EFI_ERROR(GoTimeStatus))
            {
              Print(L"Could not free pages for ELF sections. Error code: 0x%llx\r\n", GoTimeStatus);
              return GoTimeStatus;
            }

            // NOTE: CANNOT create an array of all compatible free addresses because the array takes up memory. So does the memory map.
            // This results in a paradox, so we need to scan the memory map every time we need to find a new address...

            // It appears that AllocateAnyPages uses a "MaxAddress" approach. This will go bottom-up instead.
            EFI_PHYSICAL_ADDRESS NewAddress = 0; // Start at zero
            EFI_PHYSICAL_ADDRESS OldAllocatedMemory = AllocatedMemory;

            GoTimeStatus = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &NewAddress); // Need to check 0x0
            while(GoTimeStatus != EFI_SUCCESS)
            { // Keep checking free memory addresses until one works

              if(GoTimeStatus == EFI_NOT_FOUND)
              {
                // 0's not a good address (not enough contiguous pages could be found), get another one
                NewAddress = ActuallyFreeAddress(pages, NewAddress);
                // Make sure the new address isn't the known bad one
                if(NewAddress == OldAllocatedMemory)
                {
                  // Get a new address if it is
                  NewAddress = ActuallyFreeAddress(pages, NewAddress);
                }
                // Address can be > 4GB
              }
              else if(EFI_ERROR(GoTimeStatus))
              {
                Print(L"Could not get an address for ELF pages. Error code: 0x%llx\r\n", GoTimeStatus);
                return GoTimeStatus;
              }

              if(NewAddress == ~0ULL)
              {
                // If you get this, you had no memory free anywhere.
                Print(L"No memory marked as EfiConventionalMemory...\r\n");
                return GoTimeStatus;
              }

              // Allocate the new address
              GoTimeStatus = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &NewAddress);
              // This loop shouldn't run more than once, but in the event something is at 0x0 we need to
              // leave the loop with an allocated address

            }

            // Got a new address that's been allocated--save it
            AllocatedMemory = NewAddress;

            // Verify it's empty
            while((NewAddress != ~0ULL) && VerifyZeroMem(pages << EFI_PAGE_SHIFT, AllocatedMemory)) // Loop this in case the firmware is really screwed
            { // It's not empty :(

              // Sure hope there aren't any other page-aligned kernel images floating around in memory marked as free
              if(compare((EFI_PHYSICAL_ADDRESS*)AllocatedMemory, ELFMAG, SELFMAG))
              {
                // Do nothing, we're fine
  #ifdef MEMORY_CHECK_INFO
                Print(L"System appears to have been reset. No issues.\r\n");
  #endif

                break;
              }
              else
              { // Gotta keep looking for a good memory address

  #ifdef MEMORY_DEBUG_ENABLED
                Print(L"Still searching... 0x%llx\r\n", AllocatedMemory);
  #endif

                // It's not actually free...
                GoTimeStatus = gBS->FreePages(AllocatedMemory, pages);
                if(EFI_ERROR(GoTimeStatus))
                {
                  Print(L"Could not free pages for ELF sections (loop). Error code: 0x%llx\r\n", GoTimeStatus);
                  return GoTimeStatus;
                }

                // Allocate a new address
                GoTimeStatus = EFI_NOT_FOUND;
                while((GoTimeStatus != EFI_SUCCESS) && (NewAddress != ~0ULL))
                {
                  if(GoTimeStatus == EFI_NOT_FOUND)
                  {
                    // Get an address (ideally, this should be very rare)
                    NewAddress = ActuallyFreeAddress(pages, NewAddress);
                    // Make sure the new address isn't the known bad one
                    if(NewAddress == OldAllocatedMemory)
                    {
                      // Get a new address if it is
                      NewAddress = ActuallyFreeAddress(pages, NewAddress);
                    }
                    // Address can be > 4GB
                    // This loop will run until we get a good address (shouldn't be more than once, if ever)
                  }
                  else if(EFI_ERROR(GoTimeStatus))
                  {
                    // EFI_OUT_OF_RESOURCES means the firmware's just not gonna load anything.
                    Print(L"Could not get an address for ELF pages (loop). Error code: 0x%llx\r\n", GoTimeStatus);
                    return GoTimeStatus;
                  }
                  // NOTE: The number of times the message "No more free addresses" pops up
                  // helps indicate which NewAddress assignment hit the end.

                  GoTimeStatus = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &NewAddress);
                } // loop

                // It's a new address
                AllocatedMemory = NewAddress;

                // Verify new address is empty (in loop), if not then free it and try again.
              } // else
            } // End VerifyZeroMem while loop

            // Ran out of easy addresses, time for a more thorough check
            // Hopefully no one ever gets here
            if(AllocatedMemory == ~0ULL)
            { // NewAddress is also -1

  #ifdef BY_PAGE_SEARCH_DISABLED // Set this to disable ByPage searching
              Print(L"No easy addresses found with enough space and containing only zeros.\r\nConsider enabling page-by-page search.\r\n");
              return GoTimeStatus;
  #endif

  #ifndef BY_PAGE_SEARCH_DISABLED
    #ifdef MEMORY_CHECK_INFO
              Print(L"Performing page-by-page search.\r\nThis might take a while...\r\n");
    #endif

    #ifdef MEMORY_DEBUG_ENABLED
              Keywait(L"About to search page by page\r\n");
    #endif

              NewAddress = ActuallyFreeAddress(pages, 0); // Start from the first suitable EfiConventionalMemory address.
              // Allocate the page's address
              GoTimeStatus = EFI_NOT_FOUND;
              while(GoTimeStatus != EFI_SUCCESS)
              {
                if(GoTimeStatus == EFI_NOT_FOUND)
                {
                  // Nope, get another one
                  NewAddress = ActuallyFreeAddressByPage(pages, NewAddress);
                  // Make sure the new address isn't the known bad one
                  if(NewAddress == OldAllocatedMemory)
                  {
                    // Get a new address if it is
                    NewAddress = ActuallyFreeAddressByPage(pages, NewAddress);
                  }
                  // Adresses very well might be > 4GB with the filesizes these are allowed to be
                }
                else if(EFI_ERROR(GoTimeStatus))
                {
                  Print(L"Could not get an address for ELF pages by page. Error code: 0x%llx\r\n", GoTimeStatus);
                  return GoTimeStatus;
                }

                if(NewAddress == ~0ULL)
                {
                  // If you somehow get this, you really had no memory free anywhere.
                  Print(L"Hmm... How did you get here?\r\n");
                  return GoTimeStatus;
                }

                GoTimeStatus = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &NewAddress);
              }

              AllocatedMemory = NewAddress;

              while(VerifyZeroMem(pages << EFI_PAGE_SHIFT, AllocatedMemory))
              {
                // Sure hope there aren't any other page-aligned kernel images floating around in memory marked as free
                if(compare((EFI_PHYSICAL_ADDRESS*)AllocatedMemory, ELFMAG, SELFMAG))
                {
                  // Do nothing, we're fine
    #ifdef MEMORY_CHECK_INFO
                  Print(L"System might have been reset. Hopefully no issues.\r\n");
    #endif

                  break;
                }
                else
                {

    #ifdef MEMORY_DEBUG_ENABLED
                Print(L"Still searching by page... 0x%llx\r\n", AllocatedMemory);
    #endif

                  // It's not actually free...
                  GoTimeStatus = gBS->FreePages(AllocatedMemory, pages);
                  if(EFI_ERROR(GoTimeStatus))
                  {
                    Print(L"Could not free pages for ELF sections by page (loop). Error code: 0x%llx\r\n", GoTimeStatus);
                    return GoTimeStatus;
                  }

                  GoTimeStatus = EFI_NOT_FOUND;
                  while(GoTimeStatus != EFI_SUCCESS)
                  {
                    if(GoTimeStatus == EFI_NOT_FOUND)
                    {
                      // Nope, get another one
                      NewAddress = ActuallyFreeAddressByPage(pages, NewAddress);
                      // Make sure the new address isn't the known bad one
                      if(NewAddress == OldAllocatedMemory)
                      {
                        // Get a new address if it is
                        NewAddress = ActuallyFreeAddressByPage(pages, NewAddress);
                      }
                      // Address can be > 4GB
                    }
                    else if(EFI_ERROR(GoTimeStatus))
                    {
                      Print(L"Could not get an address for ELF pages by page (loop). Error code: 0x%llx\r\n", GoTimeStatus);
                      return GoTimeStatus;
                    }

                    if(AllocatedMemory == ~0ULL)
                    {
                      // Well, darn. Something's up with the system memory.
                      return GoTimeStatus;
                    }

                    GoTimeStatus = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &NewAddress);
                  } // loop

                  AllocatedMemory = NewAddress;

                } // else
              } // end ByPage VerifyZeroMem loop
  #endif
            } // End "big guns"

            // Got a good address!
  #ifdef MEMORY_CHECK_INFO
            Print(L"Found!\r\n");
  #endif
          } // End discovery of viable memory address (else)
          // Can move on now
  #ifdef MEMORY_CHECK_INFO
          Print(L"New AllocatedMemory location: 0x%llx\r\n", AllocatedMemory);
  #endif
        } // End VerifyZeroMem buggy firmware workaround (outermost if)
        else
        {
  #ifdef MEMORY_CHECK_INFO
          Print(L"Allocated memory was zeroed OK\r\n");
  #endif
        }
#endif

#ifdef ELF_LOADER_DEBUG_ENABLED
        Print(L"New AllocatedMemory location: 0x%llx\r\n", AllocatedMemory);
        Keywait(L"Allocate Pages passed.\r\n");
#endif

        // No need to copy headers to memory for ELFs, just the program itself
        // Only want to include PT_LOAD segments
        for(i = 0; i < Numofprogheaders; i++) // Load sections into memory
        {
          struct Proghdr *specific_program_header = &program_headers_table[i];
          UINTN RawDataSize = specific_program_header->p_filesz; // 64-bit ELFs can have 64-bit file sizes!
          EFI_PHYSICAL_ADDRESS SectionAddress = specific_program_header->p_pa; // 64-bit ELFs use 64-bit addressing!

#ifdef ELF_LOADER_DEBUG_ENABLED
          Print(L"\n%llu. current section address: 0x%x, RawDataSize: 0x%llx\r\n", i+1, specific_program_header->p_va, RawDataSize);
#endif

          if(1)
          {

#ifdef ELF_LOADER_DEBUG_ENABLED
            Print(L"current destination address: 0x%llx, AllocatedMemory base: 0x%llx\r\n", SectionAddress, AllocatedMemory);
            Print(L"PointerToRawData: 0x%llx\r\n", specific_program_header->p_offset);
            Print(L"Check:\r\nSectionAddress: 0x%llx\r\nData there: 0x%016llx%016llx (should be 0)\r\n", SectionAddress, *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)(SectionAddress + 8), *(EFI_PHYSICAL_ADDRESS *)(uintptr_t)SectionAddress); // print the first 128 bits of that address to compare
            Print(L"About to load section %llu of %llu...\r\n", i + 1, Numofprogheaders);
            Keywait(L"\0");
#endif

            GoTimeStatus = KernelFile->SetPosition(KernelFile, specific_program_header->p_offset); // p_offset is a UINT64 relative to the beginning of the file, just like Read() expects!
            if(EFI_ERROR(GoTimeStatus))
            {
              Print(L"Program segment SetPosition error (ELF). 0x%llx\r\n", GoTimeStatus);
              return GoTimeStatus;
            }

            if(RawDataSize != 0) // Apparently some UEFI implementations can't deal with reading 0 byte sections
            {
              GoTimeStatus = KernelFile->Read(KernelFile, &RawDataSize, (VOID*)(uintptr_t)SectionAddress); // (void*)SectionAddress
              if(EFI_ERROR(GoTimeStatus))
              {
                Print(L"Program segment read error (ELF). 0x%llx\r\n", GoTimeStatus);
                return GoTimeStatus;
              }
            }
#ifdef ELF_LOADER_DEBUG_ENABLED
            Print(L"\r\nVerify:\r\nSectionAddress: 0x%llx\r\nData there (first 16 bytes): 0x%016llx%016llx\r\n", SectionAddress, *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)(SectionAddress + 8), *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)SectionAddress); // print the first 128 bits of that address to compare
            Print(L"Last 16 bytes: 0x%016llx%016llx\r\n", *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)(SectionAddress + RawDataSize - 8), *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)(SectionAddress + RawDataSize - 16));
            Print(L"Next 16 bytes: 0x%016llx%016llx (0 unless last section)\r\n", *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)(SectionAddress + RawDataSize + 8), *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)(SectionAddress + RawDataSize));
            // "Next 16 bytes" should be 0 unless last section
#endif
          }
          else
          {

#ifdef ELF_LOADER_DEBUG_ENABLED
            Print(L"Not a PT_LOAD section. Type: 0x%x\r\n", specific_program_header->p_type);
#endif
          }
        }

        // Done with program_headers_table
        if(program_headers_table)
        {
          GoTimeStatus = gBS->FreePool(program_headers_table);
          if(EFI_ERROR(GoTimeStatus))
          {
            Print(L"Error freeing program headers table pool. 0x%llx\r\n", GoTimeStatus);
            Keywait(L"\0");
          }
        }

#ifdef ELF_LOADER_DEBUG_ENABLED
        Keywait(L"\nLoad file sections into allocated pages passed.\r\n");
#endif

        // Link kernel with -static-pie and there's no need for relocations beyond the base-relative ones just done. YUS!

        // e_entry should be a 64-bit relative memory address, and gives the kernel's entry point
        KernelBaseAddress = AllocatedMemory;
        Header_memory = ELF32header.e_entry;

#ifdef ELF_LOADER_DEBUG_ENABLED
        Print(L"Header_memory: 0x%llx, AllocatedMemory: 0x%llx, EntryPoint: 0x%x\r\n", Header_memory, AllocatedMemory, ELF32header.e_entry);
        Print(L"Data at Header_memory (first 16 bytes): 0x%016llx%016llx\r\n", *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)(Header_memory + 8), *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)Header_memory);
#endif

        // Loaded! On to memorymap and exitbootservices...
        // NOTE: Executable entry point is now defined in Header_memory's contained address, which is AllocatedMemory + ELF64header.e_entry

      }
      else
      {
        GoTimeStatus = EFI_INVALID_PARAMETER;
        Print(L"Hey! 64-bit (x86_64) ELFs only.\r\n");
        return GoTimeStatus;
      }
    }
    else
    {
      GoTimeStatus = EFI_INVALID_PARAMETER;
      Print(L"Neither PE32+, ELF, nor Mach-O image supplied as kernel file. Check the binary.\r\n");
      return GoTimeStatus;
    }
  }

#ifdef FINAL_LOADER_DEBUG_ENABLED
  Print(L"Header_memory: 0x%llx\r\n", Header_memory);
  Print(L"Data at Header_memory (first 16 bytes): 0x%016llx%016llx\r\n", *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)(Header_memory + 8), *(EFI_PHYSICAL_ADDRESS*)(uintptr_t)Header_memory);

  if(KernelisPE)
  {
    Print(L"Kernel uses MS ABI\r\n");
  }
  else
  {
    Print(L"Kernel uses SYSV ABI\r\n");
  }

  Keywait(L"\0");

  // Integrity check
  for(UINT64 k = 0; k < Graphics->NumberOfFrameBuffers; k++)
  {
    Print(L"GPU Mode: %u of %u\r\n", Graphics->GPUArray[k].Mode, Graphics->GPUArray[k].MaxMode - 1);
    Print(L"GPU FB: 0x%016llx\r\n", Graphics->GPUArray[k].FrameBufferBase);
    Print(L"GPU FB Size: 0x%016llx\r\n", Graphics->GPUArray[k].FrameBufferSize);
    Print(L"GPU SizeOfInfo: %u Bytes\r\n", Graphics->GPUArray[k].SizeOfInfo);
    Print(L"GPU Info Ver: 0x%x\r\n", Graphics->GPUArray[k].Info->Version);
    Print(L"GPU Info Res: %ux%u\r\n", Graphics->GPUArray[k].Info->HorizontalResolution, Graphics->GPUArray[k].Info->VerticalResolution);
    Print(L"GPU Info PxFormat: 0x%x\r\n", Graphics->GPUArray[k].Info->PixelFormat);
    Print(L"GPU Info PxInfo (R,G,B,Rsvd Masks): 0x%08x, 0x%08x, 0x%08x, 0x%08x\r\n", Graphics->GPUArray[k].Info->PixelInformation.RedMask, Graphics->GPUArray[k].Info->PixelInformation.GreenMask, Graphics->GPUArray[k].Info->PixelInformation.BlueMask, Graphics->GPUArray[k].Info->PixelInformation.ReservedMask);
    Print(L"GPU Info PxPerScanLine: %u\r\n", Graphics->GPUArray[k].Info->PixelsPerScanLine);
    Keywait(L"\0");
  }

  Print(L"Config table address: 0x%llx\r\n", gST->ConfigurationTable);
#endif

  // Reserve memory for the loader block
  LOADER_PARAMS * Loader_block;
  GoTimeStatus = gBS->AllocatePool(EfiLoaderData, sizeof(LOADER_PARAMS), (void**)&Loader_block);
  if(EFI_ERROR(GoTimeStatus))
  {
    Print(L"Error allocating loader block pool. Error: 0x%llx\r\n", GoTimeStatus);
    return GoTimeStatus;
  }
/*
  Loader_block->UEFI_Version = UEFIVer;
  Loader_block->Bootloader_MajorVersion = MAJOR_VER;
  Loader_block->Bootloader_MinorVersion = MINOR_VER;

//  Loader_block->Memory_Map_Descriptor_Version = MemMapDescriptorVersion;
//  Loader_block->Memory_Map_Descriptor_Size = MemMapDescriptorSize;
//  Loader_block->Memory_Map = MemMap;
//  Loader_block->Memory_Map_Size = MemMapSize;

  Loader_block->Kernel_BaseAddress = KernelBaseAddress;
  Loader_block->Kernel_Pages = KernelPages;

  Loader_block->ESP_Root_Device_Path = ESPRoot;
  Loader_block->ESP_Root_Size = ESPRootSize;
  Loader_block->Kernel_Path = KernelPath;
  Loader_block->Kernel_Path_Size = KernelPathSize;
  Loader_block->Kernel_Options = Cmdline;
  Loader_block->Kernel_Options_Size = CmdlineSize;

  Loader_block->RTServices = gRT;
  Loader_block->GPU_Configs = Graphics;
  Loader_block->FileMeta = FileInfo;

  Loader_block->ConfigTables = SysCfgTables;
  Loader_block->Number_of_ConfigTables = NumSysCfgTables;
  Print(L"memory map pointer addr %p\r\n", &(Loader_block->Memory_Map));
  Print(L"frame buffer pointer addr %p\r\n", &(Loader_block->GPU_Configs[0].GPUArray[0].FrameBufferBase));

  Print(L"Memory_Map_Descriptor_Size pointer addr %p\r\n", &(Loader_block->Memory_Map_Descriptor_Size));
  Print(L"Memory_Map pointer addr %p\r\n", &(Loader_block->Memory_Map));
  Print(L"Memory_Map_Size pointer addr %p\r\n", &(Loader_block->Memory_Map_Size));
  Print(L"Kernel_BaseAddress pointer addr %p\r\n", &(Loader_block->Kernel_BaseAddress));
  Print(L"Kernel_Pages pointer addr %p\r\n", &(Loader_block->Kernel_Pages));
  Print(L"ESP_Root_Device_Path pointer addr %p\r\n", &(Loader_block->ESP_Root_Device_Path));
  Print(L"ESP_Root_Size pointer addr %p\r\n", &(Loader_block->ESP_Root_Size));

  Print(L"Kernel_Path pointer addr %p\r\n", &(Loader_block->Kernel_Path));
  Print(L"Kernel_Path_Size pointer addr %p\r\n", &(Loader_block->Kernel_Path_Size));
  Print(L"Kernel_Options pointer addr %p\r\n", &(Loader_block->Kernel_Options));
  Print(L"Kernel_Options_Size pointer addr %p\r\n", &(Loader_block->Kernel_Options_Size));
  Print(L"RTServices pointer addr %p\r\n", &(Loader_block->RTServices));
  Print(L"GPU_Configs pointer addr %p\n", &(Loader_block->GPU_Configs));
*/

#ifdef FINAL_LOADER_DEBUG_ENABLED
  Print(L"Loader block allocated, size of structure: %llu\r\n", sizeof(LOADER_PARAMS));
  Print(L"Loader block address: %p\r\n", Loader_block);
  Keywait(L"About to get MemMap and exit boot services...\r\n");
#endif

 //----------------------------------------------------------------------------------------------------------------------------------
 //  Get Memory Map and Exit Boot Services
 //----------------------------------------------------------------------------------------------------------------------------------

  // UINTN is the largest uint type supported. For x86_64, this is uint64_t
  UINTN MemMapSize = 0, MemMapKey, MemMapDescriptorSize;
  UINT32 MemMapDescriptorVersion;
  EFI_MEMORY_DESCRIPTOR * MemMap = NULL;

/*
  // Simple version:
  GoTimeStatus = gBS->GetMemoryMap(&MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  // Will error intentionally
  GoTimeStatus = gBS->AllocatePool(EfiLoaderData, MemMapSize, (void **)&MemMap);
  if(EFI_ERROR(GoTimeStatus)) // Error! Wouldn't be safe to continue.
    {
      Print(L"MemMap AllocatePool error. 0x%llx\r\n", GoTimeStatus);
      return GoTimeStatus;
    }
  GoTimeStatus = gBS->GetMemoryMap(&MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  GoTimeStatus = gBS->ExitBootServices(ImageHandle, MemMapKey);
*/

// Below is a better, but more complex version. EFI Spec recommends this method; apparently some systems need a second call to ExitBootServices.

  // Get memory map and exit boot services
  GoTimeStatus = gBS->GetMemoryMap(&MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  if(GoTimeStatus == EFI_BUFFER_TOO_SMALL)
  {
    GoTimeStatus = gBS->AllocatePool(EfiLoaderData, MemMapSize, (void **)&MemMap); // Allocate pool for MemMap (it should always be resident in memory)
    if(EFI_ERROR(GoTimeStatus)) // Error! Wouldn't be safe to continue.
    {
      Print(L"MemMap AllocatePool error. 0x%llx\r\n", GoTimeStatus);
      return GoTimeStatus;
    }
    GoTimeStatus = gBS->GetMemoryMap(&MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  }

  GoTimeStatus = gBS->ExitBootServices(ImageHandle, MemMapKey);

  if(GoTimeStatus == EFI_INVALID_PARAMETER) // Error! EFI_INVALID_PARAMETER, MemMapKey is incorrect
  {

    GoTimeStatus = gBS->FreePool(MemMap);
    if(EFI_ERROR(GoTimeStatus)) // Error! Wouldn't be safe to continue.
    {
      Print(L"Error freeing MemMap pool from failed ExitBootServices. 0x%llx\r\n", GoTimeStatus);
      Keywait(L"\0");
    }

#ifdef LOADER_DEBUG_ENABLED
    Print(L"ExitBootServices #1 failed. 0x%llx, Trying again...\r\n", GoTimeStatus);
    Keywait(L"\0");
#endif

    MemMapSize = 0;
    GoTimeStatus = gBS->GetMemoryMap(&MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
    if(GoTimeStatus == EFI_BUFFER_TOO_SMALL)
    {
      GoTimeStatus = gBS->AllocatePool(EfiLoaderData, MemMapSize, (void **)&MemMap);
      if(EFI_ERROR(GoTimeStatus)) // Error! Wouldn't be safe to continue.
      {
        Print(L"MemMap AllocatePool error #2. 0x%llx\r\n", GoTimeStatus);
        return GoTimeStatus;
      }
      GoTimeStatus = gBS->GetMemoryMap(&MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
    }

    GoTimeStatus = gBS->ExitBootServices(ImageHandle, MemMapKey);

  } else if (GoTimeStatus == EFI_SUCCESS) {
    GoTimeStatus = EFI_SUCCESS;
  }

  // This applies to both the simple and larger versions of the above.
  if(GoTimeStatus == EFI_INVALID_PARAMETER)
  {
    Print(L"Could not exit boot services... 0x%llx\r\n", GoTimeStatus);
    GoTimeStatus = gBS->FreePool(MemMap);
    if(EFI_ERROR(GoTimeStatus)) // Error! Wouldn't be safe to continue.
    {
      Print(L"Error freeing MemMap pool. 0x%llx\r\n", GoTimeStatus);
    }
    Print(L"MemMapSize: %llx, MemMapKey: %llx\r\n", MemMapSize, MemMapKey);
    Print(L"DescriptorSize: %llx, DescriptorVersion: %x\r\n", MemMapDescriptorSize, MemMapDescriptorVersion);
    return GoTimeStatus;
  }

  //----------------------------------------------------------------------------------------------------------------------------------
  //  Entry Point Jump
  //----------------------------------------------------------------------------------------------------------------------------------

/*
  // Example entry point jump
  typedef void (EFIAPI *EntryPointFunction)();
  EntryPointFunction EntryPointPlaceholder = (EntryPointFunction)(Header_memory);
//  Print(L"EntryPointPlaceholder: %llx\r\n", (UINT64)EntryPointPlaceholder);
  EntryPointPlaceholder();
*/

  // Example one-liners for an entry point jump
  // ((EFIAPI void(*)(EFI_RUNTIME_SERVICES*)) Header_memory)(RT);
  // ((EFIAPI void(*)(void)) Header_memory)(); // A void-returning function that takes no arguments. Neat!

/*
  // Another example entry point jump
  typedef void (EFIAPI *EntryPointFunction)(EFI_MEMORY_DESCRIPTOR * Memory_Map, EFI_RUNTIME_SERVICES* RTServices, EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* GPU_Mode, EFI_FILE_INFO* FileMeta, void * RSDP); // Placeholder names for jump
  EntryPointFunction EntryPointPlaceholder = (EntryPointFunction)(Header_memory);
  EntryPointPlaceholder(MemMap, RT, &Graphics, FileInfo, RSDPTable);
*/

/*
  // Loader block defined in header
  typedef struct {
    UINT32                    UEFI_Version;                   // The system UEFI version
    UINT32                    Bootloader_MajorVersion;        // The major version of the bootloader
    UINT32                    Bootloader_MinorVersion;        // The minor version of the bootloader

    UINT32                    Memory_Map_Descriptor_Version;  // The memory descriptor version
    UINTN                     Memory_Map_Descriptor_Size;     // The size of an individual memory descriptor
    EFI_MEMORY_DESCRIPTOR    *Memory_Map;                     // The system memory map as an array of EFI_MEMORY_DESCRIPTOR structs
    UINTN                     Memory_Map_Size;                // The total size of the system memory map

    EFI_PHYSICAL_ADDRESS      Kernel_BaseAddress;             // The base memory address of the loaded kernel file
    UINTN                     Kernel_Pages;                   // The number of pages (1 page == 4096 bytes) allocated for the kernel file

    CHAR16                   *ESP_Root_Device_Path;           // A UTF-16 string containing the drive root of the EFI System Partition as converted from UEFI device path format
    UINT64                    ESP_Root_Size;                  // The size (in bytes) of the above ESP root string
    CHAR16                   *Kernel_Path;                    // A UTF-16 string containing the kernel's file path relative to the EFI System Partition root (it's the first line of Kernel64.txt)
    UINT64                    Kernel_Path_Size;               // The size (in bytes) of the above kernel file path
    CHAR16                   *Kernel_Options;                 // A UTF-16 string containing various load options (it's the second line of Kernel64.txt)
    UINT64                    Kernel_Options_Size;            // The size (in bytes) of the above load options string

    EFI_RUNTIME_SERVICES     *RTServices;                     // UEFI Runtime Services
    GPU_CONFIG               *GPU_Configs;                    // Information about available graphics output devices; see below GPU_CONFIG struct for details
    EFI_FILE_INFO            *FileMeta;                       // Kernel file metadata

    EFI_CONFIGURATION_TABLE  *ConfigTables;                   // UEFI-installed system configuration tables (ACPI, SMBIOS, etc.)
    UINTN                     Number_of_ConfigTables;         // The number of system configuration tables
  } LOADER_PARAMS;
*/

  // This shouldn't modify the memory map.
  Loader_block->UEFI_Version = UEFIVer;
  Loader_block->Bootloader_MajorVersion = MAJOR_VER;
  Loader_block->Bootloader_MinorVersion = MINOR_VER;

  Loader_block->Memory_Map_Descriptor_Version = MemMapDescriptorVersion;
  Loader_block->Memory_Map_Descriptor_Size = MemMapDescriptorSize;
  Loader_block->Memory_Map = MemMap;
  Loader_block->Memory_Map_Size = MemMapSize;

  Loader_block->Kernel_BaseAddress = KernelBaseAddress;
  Loader_block->Kernel_Pages = KernelPages;

  Loader_block->ESP_Root_Device_Path = ESPRoot;
  Loader_block->ESP_Root_Size = ESPRootSize;
  Loader_block->Kernel_Path = KernelPath;
  Loader_block->Kernel_Path_Size = KernelPathSize;
  Loader_block->Kernel_Options = Cmdline;
  Loader_block->Kernel_Options_Size = CmdlineSize;

  Loader_block->RTServices = gRT;
  Loader_block->GPU_Configs = Graphics;
  Loader_block->FileMeta = FileInfo;

  Loader_block->ConfigTables = SysCfgTables;
  Loader_block->Number_of_ConfigTables = NumSysCfgTables;

  // Jump to entry point, and WE ARE LIVE!!
//  ((EFIAPI void(*)(void)) (uintptr_t)Header_memory)(); //TODO remove

  if(KernelisPE)
  {
    typedef void (__attribute__((ms_abi)) *EntryPointFunction)(LOADER_PARAMS * LP); // Placeholder names for jump
    EntryPointFunction EntryPointPlaceholder = (EntryPointFunction)(uintptr_t)(Header_memory);
    EntryPointPlaceholder(Loader_block);
  }
  else
  {
    typedef void (__attribute__((sysv_abi)) *EntryPointFunction)(LOADER_PARAMS * LP); // Placeholder names for jump
    EntryPointFunction EntryPointPlaceholder = (EntryPointFunction)(uintptr_t)(Header_memory);
    EntryPointPlaceholder(Loader_block);
  }

  // Should never get here
  return GoTimeStatus;
}
