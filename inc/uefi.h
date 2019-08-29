#ifndef JOS_INC_UEFI_H
#define JOS_INC_UEFI_H

#include <inc/types.h>

typedef uint32_t UINTN;
typedef uint64_t EFI_PHYSICAL_ADDRESS __attribute__((__aligned__(8)));
typedef uint64_t EFI_VIRTUAL_ADDRESS __attribute__((__aligned__(8)));
typedef uint64_t ALIGNEDU64 __attribute__((__aligned__(8)));

typedef struct {
  ///
  /// Type of the memory region.
  /// Type EFI_MEMORY_TYPE is defined in the
  /// AllocatePages() function description.
  ///
  uint32_t                Type;
  ///
  /// Physical address of the first byte in the memory region. PhysicalStart must be
  /// aligned on a 4 KiB boundary, and must not be above 0xfffffffffffff000. Type
  /// EFI_PHYSICAL_ADDRESS is defined in the AllocatePages() function description
  ///
  EFI_PHYSICAL_ADDRESS  PhysicalStart;
  ///
  /// Virtual address of the first byte in the memory region.
  /// VirtualStart must be aligned on a 4 KiB boundary,
  /// and must not be above 0xfffffffffffff000.
  ///
  EFI_VIRTUAL_ADDRESS   VirtualStart;
  ///
  /// NumberOfPagesNumber of 4 KiB pages in the memory region.
  /// NumberOfPages must not be 0, and must not be any value
  /// that would represent a memory page with a start address,
  /// either physical or virtual, above 0xfffffffffffff000.
  ///
  ALIGNEDU64                NumberOfPages;
  ///
  /// Attributes of the memory region that describe the bit mask of capabilities
  /// for that memory region, and not necessarily the current settings for that
  /// memory region.
  ///
  ALIGNEDU64                Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef enum {
  ///
  /// A pixel is 32-bits and byte zero represents red, byte one represents green, 
  /// byte two represents blue, and byte three is reserved. This is the definition 
  /// for the physical frame buffer. The byte values for the red, green, and blue 
  /// components represent the color intensity. This color intensity value range 
  /// from a minimum intensity of 0 to maximum intensity of 255.
  ///
  PixelRedGreenBlueReserved8BitPerColor,
  ///
  /// A pixel is 32-bits and byte zero represents blue, byte one represents green, 
  /// byte two represents red, and byte three is reserved. This is the definition 
  /// for the physical frame buffer. The byte values for the red, green, and blue 
  /// components represent the color intensity. This color intensity value range 
  /// from a minimum intensity of 0 to maximum intensity of 255.
  ///
  PixelBlueGreenRedReserved8BitPerColor,
  ///
  /// The Pixel definition of the physical frame buffer.
  ///
  PixelBitMask,
  ///
  /// This mode does not support a physical frame buffer.
  ///
  PixelBltOnly,
  ///
  /// Valid EFI_GRAPHICS_PIXEL_FORMAT enum values are less than this value.
  ///
  PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
  uint32_t            RedMask;
  uint32_t            GreenMask;
  uint32_t            BlueMask;
  uint32_t            ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
  ///
  /// The version of this data structure. A value of zero represents the 
  /// EFI_GRAPHICS_OUTPUT_MODE_INFORMATION structure as defined in this specification.
  ///
  uint32_t                     Version;
  ///
  /// The size of video screen in pixels in the X dimension.
  ///
  uint32_t                     HorizontalResolution;
  ///
  /// The size of video screen in pixels in the Y dimension.
  ///
  uint32_t                     VerticalResolution;
  ///
  /// Enumeration that defines the physical format of the pixel. A value of PixelBltOnly 
  /// implies that a linear frame buffer is not available for this mode.
  ///
  EFI_GRAPHICS_PIXEL_FORMAT  PixelFormat;
  ///
  /// This bit-mask is only valid if PixelFormat is set to PixelPixelBitMask. 
  /// A bit being set defines what bits are used for what purpose such as Red, Green, Blue, or Reserved.
  ///
  EFI_PIXEL_BITMASK          PixelInformation;
  ///
  /// Defines the number of pixel elements per video memory line.
  /// 
  uint32_t                     PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
  ///
  /// The number of modes supported by QueryMode() and SetMode().
  ///
  uint32_t                                 MaxMode;
  ///
  /// Current Mode of the graphics device. Valid mode numbers are 0 to MaxMode -1.
  ///
  uint32_t                                 Mode;
  ///
  /// Pointer to read-only EFI_GRAPHICS_OUTPUT_MODE_INFORMATION data.
  ///
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION   *Info;
  ///
  /// Size of Info structure in bytes.
  ///
  UINTN                                  SizeOfInfo;
  ///
  /// Base address of graphics linear frame buffer.
  /// Offset zero in FrameBufferBase represents the upper left pixel of the display.
  ///
  EFI_PHYSICAL_ADDRESS                   FrameBufferBase;
  ///
  /// Amount of frame buffer needed to support the active mode as defined by 
  /// PixelsPerScanLine xVerticalResolution x PixelElementSize.
  ///
  UINTN                                  FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *GPUArray;             // This array contains the EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE structures for each available framebuffer
  ALIGNEDU64                              NumberOfFrameBuffers; // The number of pointers in the array (== the number of available framebuffers)
} GPU_CONFIG;

typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *GPUArray;             // This array contains the EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE structures for each available framebuffer
  uint64_t                              NumberOfFrameBuffers; // The number of pointers in the array (== the number of available framebuffers)
} GPU_CONFIG2;

typedef struct {
  uint16_t  Year;
  uint8_t   Month;
  uint8_t   Day;
  uint8_t   Hour;
  uint8_t   Minute;
  uint8_t   Second;
  uint8_t   Pad1;
  uint32_t  Nanosecond;
  int16_t   TimeZone;
  uint8_t   Daylight;
  uint8_t   Pad2;
} EFI_TIME;

typedef struct {
  ///
  /// The size of the EFI_FILE_INFO structure, including the Null-terminated FileName string.
  ///
  ALIGNEDU64    Size;
  ///
  /// The size of the file in bytes.
  ///
  ALIGNEDU64    FileSize;
  ///
  /// PhysicalSize The amount of physical space the file consumes on the file system volume.
  ///
  ALIGNEDU64    PhysicalSize;
  ///
  /// The time the file was created.
  ///
  EFI_TIME  CreateTime;
  ///
  /// The time when the file was last accessed.
  ///
  EFI_TIME  LastAccessTime;
  ///
  /// The time when the file's contents were last modified.
  ///
  EFI_TIME  ModificationTime;
  ///
  /// The attribute bits for the file.
  ///
  ALIGNEDU64    Attribute;
  ///
  /// The Null-terminated name of the file.
  ///
  char16_t    FileName[1];
} EFI_FILE_INFO;

typedef struct {
  uint32_t  Data1;
  uint16_t  Data2;
  uint16_t  Data3;
  uint8_t   Data4[8];
} GUID;

typedef GUID EFI_GUID;

typedef struct {
  ///
  /// The 128-bit GUID value that uniquely identifies the system configuration table.
  ///
  EFI_GUID                          VendorGuid;
  ///
  /// A pointer to the table associated with VendorGuid.
  ///
  void                              *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
  uint32_t                    UEFI_Version;                   // The system UEFI version
  uint32_t                    Bootloader_MajorVersion;        // The major version of the bootloader
  uint32_t                    Bootloader_MinorVersion;        // The minor version of the bootloader

  uint32_t                    Memory_Map_Descriptor_Version;  // The memory descriptor version
  UINTN                     Memory_Map_Descriptor_Size;     // The size of an individual memory descriptor
  EFI_MEMORY_DESCRIPTOR    *Memory_Map;                     // The system memory map as an array of EFI_MEMORY_DESCRIPTOR structs
  UINTN                     Memory_Map_Size;                // The total size of the system memory map

  EFI_PHYSICAL_ADDRESS      Kernel_BaseAddress;             // The base memory address of the loaded kernel file

  UINTN                     Kernel_Pages;                   // The number of pages (1 page == 4096 bytes) allocated for the kernel file

  char16_t                   *ESP_Root_Device_Path;           // A UTF-16 string containing the drive root of the EFI System Partition as converted from UEFI device path format
  ALIGNEDU64                    ESP_Root_Size;                  // The size (in bytes) of the above ESP root string
  char16_t                   *Kernel_Path;                    // A UTF-16 string containing the kernel's file path relative to the EFI System Partition root (it's the first line of Kernel64.txt)

  ALIGNEDU64                    Kernel_Path_Size;               // The size (in bytes) of the above kernel file path
  char16_t                   *Kernel_Options;                 // A UTF-16 string containing various load options (it's the second line of Kernel64.txt)

  ALIGNEDU64                    Kernel_Options_Size;            // The size (in bytes) of the above load options string

  //TODO add EFI_RUNTIME_SERVICES description
  void/*EFI_RUNTIME_SERVICES*/     *RTServices;                     // UEFI Runtime Services
  GPU_CONFIG               *GPU_Configs;                    // Information about available graphics output devices; see below GPU_CONFIG struct for details
  EFI_FILE_INFO            *FileMeta;                       // Kernel file metadata
  EFI_CONFIGURATION_TABLE  *ConfigTables;                   // UEFI-installed system configuration tables (ACPI, SMBIOS, etc.)
  UINTN                     Number_of_ConfigTables;         // The number of system configuration tables
} LOADER_PARAMS;

extern LOADER_PARAMS* UEFI_LP;

#endif //JOS_INC_UEFI_H
