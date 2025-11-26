#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

// VGA text mode buffer
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// VGA color codes
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW        14
#define VGA_COLOR_WHITE         15

void vga_init(void);
void vga_clear(void);
void kputchar(char c);
void kputs(const char* str);
void print_string(const char* str);
void print_dec(uint32_t num);
void print_int(int32_t num);
void print_hex(uint32_t num);
void print_hex8(uint8_t num);
void kprintf(const char* fmt, ...);

// FAT Boot Sector structure (FAT12/16/32)
typedef struct {
    uint8_t  jmp[3];                // Jump instruction
    char     oem[8];                // OEM name
    uint16_t bytes_per_sector;      // Bytes per sector
    uint8_t  sectors_per_cluster;   // Sectors per cluster
    uint16_t reserved_sectors;      // Reserved sectors
    uint8_t  num_fats;              // Number of FATs
    uint16_t root_entries;          // Root directory entries (FAT12/16)
    uint16_t total_sectors_16;      // Total sectors (if < 65536)
    uint8_t  media_type;            // Media descriptor
    uint16_t sectors_per_fat_16;    // Sectors per FAT (FAT12/16)
    uint16_t sectors_per_track;     // Sectors per track
    uint16_t num_heads;             // Number of heads
    uint32_t hidden_sectors;        // Hidden sectors
    uint32_t total_sectors_32;      // Total sectors (if >= 65536)
    
    // FAT32 specific fields
    uint32_t sectors_per_fat_32;    // Sectors per FAT (FAT32)
    uint16_t flags;                 // Flags
    uint16_t version;               // Version
    uint32_t root_cluster;          // Root directory cluster (FAT32)
    uint16_t fsinfo_sector;         // FSInfo sector
    uint16_t backup_boot_sector;    // Backup boot sector
    uint8_t  reserved[12];          // Reserved
    uint8_t  drive_num;             // Drive number
    uint8_t  reserved1;             // Reserved
    uint8_t  boot_sig;              // Boot signature
    uint32_t volume_id;             // Volume ID
    char     volume_label[11];      // Volume label
    char     fs_type[8];            // Filesystem type
} __attribute__((packed)) FAT_BootSector;

// FAT Directory Entry structure
typedef struct {
    char     name[8];               // Filename (space-padded)
    char     ext[3];                // Extension (space-padded)
    uint8_t  attr;                  // File attributes
    uint8_t  reserved;              // Reserved
    uint8_t  create_time_tenth;     // Creation time (tenths of second)
    uint16_t create_time;           // Creation time
    uint16_t create_date;           // Creation date
    uint16_t access_date;           // Last access date
    uint16_t cluster_high;          // High word of first cluster (FAT32)
    uint16_t modify_time;           // Modification time
    uint16_t modify_date;           // Modification date
    uint16_t cluster_low;           // Low word of first cluster
    uint32_t file_size;             // File size in bytes
} __attribute__((packed)) FAT_DirEntry;

// File attributes
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F  // Long filename entry

// FAT type enum
typedef enum {
    FAT_TYPE_12,
    FAT_TYPE_16,
    FAT_TYPE_32
} FAT_Type;

// Global FAT driver state
typedef struct {
    FAT_BootSector boot_sector;
    uint8_t *fat_table;             // Pointer to FAT in memory
    uint32_t fat_size;              // Size of FAT in bytes
    uint32_t data_start_sector;     // First sector of data region
    uint32_t root_dir_sectors;      // Sectors used by root directory
    uint32_t first_data_sector;     // First sector containing data
    FAT_Type fat_type;              // Type of FAT (12/16/32)
    bool initialized;
} FAT_State;

static FAT_State g_fat_state = {0};

// File handle structure
typedef struct {
    uint32_t first_cluster;         // First cluster of file
    uint32_t current_cluster;       // Current cluster being read
    uint32_t file_size;             // Total file size
    uint32_t position;              // Current position in file
    bool is_open;
} FAT_FileHandle;

// External functions you need to provide in your kernel:
// - disk_read(sector, count, buffer): Read sectors from disk
// - kmalloc(size): Allocate kernel memory
// - kfree(ptr): Free kernel memory
extern int disk_read(uint32_t sector, uint32_t count, void *buffer);
extern void* kmalloc(size_t size);
extern void kfree(void *ptr);

// Helper function: Get total sectors
static uint32_t get_total_sectors(FAT_BootSector *bs) {
    return (bs->total_sectors_16 != 0) ? bs->total_sectors_16 : bs->total_sectors_32;
}

// Helper function: Get sectors per FAT
static uint32_t get_sectors_per_fat(FAT_BootSector *bs) {
    return (bs->sectors_per_fat_16 != 0) ? bs->sectors_per_fat_16 : bs->sectors_per_fat_32;
}

// Helper function: Determine FAT type
static FAT_Type determine_fat_type(FAT_BootSector *bs) {
    uint32_t total_sectors = get_total_sectors(bs);
    uint32_t fat_size = get_sectors_per_fat(bs);
    uint32_t root_dir_sectors = ((bs->root_entries * 32) + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector;
    uint32_t data_sectors = total_sectors - (bs->reserved_sectors + (bs->num_fats * fat_size) + root_dir_sectors);
    uint32_t total_clusters = data_sectors / bs->sectors_per_cluster;
    
    if (total_clusters < 4085) {
        return FAT_TYPE_12;
    } else if (total_clusters < 65525) {
        return FAT_TYPE_16;
    } else {
        return FAT_TYPE_32;
    }
}

// Helper function: Get next cluster from FAT
static uint32_t get_next_cluster(uint32_t cluster) {
    uint32_t next_cluster = 0;
    
    switch (g_fat_state.fat_type) {
        case FAT_TYPE_12: {
            uint32_t fat_offset = cluster + (cluster / 2);  // multiply by 1.5
            uint16_t fat_value = *(uint16_t*)&g_fat_state.fat_table[fat_offset];
            
            if (cluster & 1) {
                next_cluster = fat_value >> 4;
            } else {
                next_cluster = fat_value & 0x0FFF;
            }
            
            if (next_cluster >= 0x0FF8) next_cluster = 0xFFFFFFFF;  // EOC marker
            break;
        }
        case FAT_TYPE_16: {
            uint32_t fat_offset = cluster * 2;
            next_cluster = *(uint16_t*)&g_fat_state.fat_table[fat_offset];
            
            if (next_cluster >= 0xFFF8) next_cluster = 0xFFFFFFFF;  // EOC marker
            break;
        }
        case FAT_TYPE_32: {
            uint32_t fat_offset = cluster * 4;
            next_cluster = *(uint32_t*)&g_fat_state.fat_table[fat_offset] & 0x0FFFFFFF;
            
            if (next_cluster >= 0x0FFFFFF8) next_cluster = 0xFFFFFFFF;  // EOC marker
            break;
        }
    }
    
    return next_cluster;
}

// Helper function: Get first sector of a cluster
static uint32_t cluster_to_sector(uint32_t cluster) {
    return ((cluster - 2) * g_fat_state.boot_sector.sectors_per_cluster) + g_fat_state.first_data_sector;
}

/**
 * fatInit - Initialize the FAT filesystem driver
 * 
 * Reads the boot sector and FAT table into memory.
 * Must be called before using fatOpen or fatRead.
 * 
 * Returns: 0 on success, -1 on failure
 */
int fatInit(void) {
    // Read boot sector
    if (disk_read(0, 1, &g_fat_state.boot_sector) != 0) {
        return -1;
    }
    
    // Validate boot sector signature
    uint8_t *boot_sig = (uint8_t*)&g_fat_state.boot_sector + 510;
    if (boot_sig[0] != 0x55 || boot_sig[1] != 0xAA) {
        return -1;
    }
    
    // Calculate filesystem parameters
    g_fat_state.fat_type = determine_fat_type(&g_fat_state.boot_sector);
    
    uint32_t fat_size = get_sectors_per_fat(&g_fat_state.boot_sector);
    g_fat_state.root_dir_sectors = ((g_fat_state.boot_sector.root_entries * 32) + 
                                    (g_fat_state.boot_sector.bytes_per_sector - 1)) / 
                                    g_fat_state.boot_sector.bytes_per_sector;
    
    g_fat_state.first_data_sector = g_fat_state.boot_sector.reserved_sectors + 
                                    (g_fat_state.boot_sector.num_fats * fat_size) + 
                                    g_fat_state.root_dir_sectors;
    
    // Allocate memory for FAT table
    g_fat_state.fat_size = fat_size * g_fat_state.boot_sector.bytes_per_sector;
    g_fat_state.fat_table = (uint8_t*)kmalloc(g_fat_state.fat_size);
    if (!g_fat_state.fat_table) {
        return -1;
    }
    
    // Read FAT table into memory
    if (disk_read(g_fat_state.boot_sector.reserved_sectors, fat_size, g_fat_state.fat_table) != 0) {
        kfree(g_fat_state.fat_table);
        return -1;
    }
    
    g_fat_state.initialized = true;
    return 0;
}

/**
 * fatOpen - Open a file in the FAT filesystem
 * 
 * Searches for the file in the root directory and initializes a file handle.
 * Currently only supports files in the root directory.
 * 
 * @filename: Name of the file to open (8.3 format, e.g., "FILE.TXT")
 * @handle: Pointer to file handle structure to initialize
 * 
 * Returns: 0 on success, -1 on failure
 */
int fatOpen(const char *filename, FAT_FileHandle *handle) {
    if (!g_fat_state.initialized || !filename || !handle) {
        return -1;
    }
    
    // Parse filename into 8.3 format
    char name[8] = "        ";
    char ext[3] = "   ";
    
    const char *dot = strchr(filename, '.');
    if (dot) {
        int name_len = dot - filename;
        if (name_len > 8) name_len = 8;
        memcpy(name, filename, name_len);
        
        int ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        memcpy(ext, dot + 1, ext_len);
    } else {
        int name_len = strlen(filename);
        if (name_len > 8) name_len = 8;
        memcpy(name, filename, name_len);
    }
    
    // Convert to uppercase
    for (int i = 0; i < 8; i++) {
        if (name[i] >= 'a' && name[i] <= 'z') name[i] -= 32;
    }
    for (int i = 0; i < 3; i++) {
        if (ext[i] >= 'a' && ext[i] <= 'z') ext[i] -= 32;
    }
    
    // Read root directory
    uint32_t root_dir_sector = g_fat_state.boot_sector.reserved_sectors + 
                               (g_fat_state.boot_sector.num_fats * get_sectors_per_fat(&g_fat_state.boot_sector));
    
    FAT_DirEntry *dir_entries = (FAT_DirEntry*)kmalloc(g_fat_state.root_dir_sectors * g_fat_state.boot_sector.bytes_per_sector);
    if (!dir_entries) {
        return -1;
    }
    
    if (disk_read(root_dir_sector, g_fat_state.root_dir_sectors, dir_entries) != 0) {
        kfree(dir_entries);
        return -1;
    }
    
    // Search for file
    uint32_t num_entries = (g_fat_state.root_dir_sectors * g_fat_state.boot_sector.bytes_per_sector) / sizeof(FAT_DirEntry);
    bool found = false;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        FAT_DirEntry *entry = &dir_entries[i];
        
        // Check for end of directory
        if (entry->name[0] == 0x00) break;
        
        // Skip deleted entries and LFN entries
        if (entry->name[0] == 0xE5 || entry->attr == FAT_ATTR_LFN) continue;
        
        // Skip directories and volume labels
        if (entry->attr & (FAT_ATTR_DIRECTORY | FAT_ATTR_VOLUME_ID)) continue;
        
        // Compare filename
        if (memcmp(entry->name, name, 8) == 0 && memcmp(entry->ext, ext, 3) == 0) {
            // File found!
            uint32_t cluster = entry->cluster_low | ((uint32_t)entry->cluster_high << 16);
            
            handle->first_cluster = cluster;
            handle->current_cluster = cluster;
            handle->file_size = entry->file_size;
            handle->position = 0;
            handle->is_open = true;
            
            found = true;
            break;
        }
    }
    
    kfree(dir_entries);
    return found ? 0 : -1;
}

/**
 * fatRead - Read data from an open file
 * 
 * Reads up to 'size' bytes from the current position in the file.
 * Advances the file position by the number of bytes read.
 * 
 * @handle: Pointer to open file handle
 * @buffer: Buffer to read data into
 * @size: Maximum number of bytes to read
 * 
 * Returns: Number of bytes read, or -1 on error
 */
int fatRead(FAT_FileHandle *handle, void *buffer, uint32_t size) {
    if (!g_fat_state.initialized || !handle || !handle->is_open || !buffer) {
        return -1;
    }
    
    // Check if we're at end of file
    if (handle->position >= handle->file_size) {
        return 0;
    }
    
    // Limit read size to remaining file size
    if (handle->position + size > handle->file_size) {
        size = handle->file_size - handle->position;
    }
    
    uint32_t bytes_read = 0;
    uint32_t cluster_size = g_fat_state.boot_sector.sectors_per_cluster * g_fat_state.boot_sector.bytes_per_sector;
    
    // Allocate temporary cluster buffer
    uint8_t *cluster_buffer = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buffer) {
        return -1;
    }
    
    while (bytes_read < size && handle->current_cluster != 0xFFFFFFFF) {
        // Calculate offset within current cluster
        uint32_t cluster_offset = handle->position % cluster_size;
        uint32_t bytes_to_read = cluster_size - cluster_offset;
        
        if (bytes_to_read > size - bytes_read) {
            bytes_to_read = size - bytes_read;
        }
        
        // Read cluster from disk
        uint32_t sector = cluster_to_sector(handle->current_cluster);
        if (disk_read(sector, g_fat_state.boot_sector.sectors_per_cluster, cluster_buffer) != 0) {
            kfree(cluster_buffer);
            return -1;
        }
        
        // Copy data to output buffer
        memcpy((uint8_t*)buffer + bytes_read, cluster_buffer + cluster_offset, bytes_to_read);
        
        bytes_read += bytes_to_read;
        handle->position += bytes_to_read;
        
        // Move to next cluster if needed
        if ((handle->position % cluster_size) == 0 && bytes_read < size) {
            handle->current_cluster = get_next_cluster(handle->current_cluster);
        }
    }
    
    kfree(cluster_buffer);
    return bytes_read;
}

// ============================================================================
// STRING FUNCTIONS (freestanding implementations)
// ============================================================================

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }

    return 0;
}

void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

char* strchr(const char* s, int c) {
    while (*s != '\0') {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }

    if (c == '\0') {
        return (char*)s;
    }

    return NULL;
}

// ============================================================================
// SIMPLE MEMORY ALLOCATOR
// ============================================================================

// Simple bump allocator for kernel (you should replace with a proper allocator)
#define HEAP_SIZE (1024 * 1024)  // 1MB heap
static uint8_t heap[HEAP_SIZE];
static size_t heap_offset = 0;

void* kmalloc(size_t size) {
    // Align to 4-byte boundary
    size = (size + 3) & ~3;

    if (heap_offset + size > HEAP_SIZE) {
        return NULL;  // Out of memory
    }

    void* ptr = &heap[heap_offset];
    heap_offset += size;

    return ptr;
}

void kfree(void* ptr) {
    // Simple bump allocator doesn't support freeing
    // You should implement a proper allocator (like a linked list or buddy system)
    (void)ptr;
}

// Port I/O functions
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void inw_rep(uint16_t port, void* buffer, uint32_t count) {
    __asm__ volatile ("rep insw" : "+D"(buffer), "+c"(count) : "d"(port) : "memory");
}

// ATA PIO disk reading
#define ATA_PRIMARY_IO 0x1F0
#define ATA_DATA        (ATA_PRIMARY_IO + 0)
#define ATA_SECTOR_COUNT (ATA_PRIMARY_IO + 2)
#define ATA_LBA_LOW     (ATA_PRIMARY_IO + 3)
#define ATA_LBA_MID     (ATA_PRIMARY_IO + 4)
#define ATA_LBA_HIGH    (ATA_PRIMARY_IO + 5)
#define ATA_DRIVE       (ATA_PRIMARY_IO + 6)
#define ATA_COMMAND     (ATA_PRIMARY_IO + 7)
#define ATA_STATUS      (ATA_PRIMARY_IO + 7)
#define ATA_CMD_READ_PIO 0x20
#define ATA_STATUS_BSY   0x80
#define ATA_STATUS_DRQ   0x08

int disk_read(uint32_t sector, uint32_t count, void* buffer) {
    if (count == 0 || count > 256) return -1;

    uint8_t* buf = (uint8_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t lba = sector + i;

        // Wait for disk to be ready
        for (int j = 0; j < 1000; j++) {
            if (!(inb(ATA_STATUS) & ATA_STATUS_BSY)) break;
        }

        // Send read command
        outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_SECTOR_COUNT, 1);
        outb(ATA_LBA_LOW, lba & 0xFF);
        outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
        outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
        outb(ATA_COMMAND, ATA_CMD_READ_PIO);

        // Wait for data ready
        for (int j = 0; j < 1000000; j++) {
            uint8_t status = inb(ATA_STATUS);
            if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) break;
        }

        // Read 512 bytes
        inw_rep(ATA_DATA, buf + (i * 512), 256);

        // Delay
        for (int j = 0; j < 4; j++) inb(ATA_STATUS);
    }

    return 0;
}

void main() {
    char *vram = (char*)0xb8000; // Base address of video mem
    const char color = 7; // gray text on black background
    int current_offset = 0;


    print_string("Kernel starting...\n");
    print_string("Initializing FAT filesystem...\n");

    // Initialize the FAT filesystem
    if (fatInit() != 0) {
        print_string("ERROR: Failed to initialize FAT filesystem!\n");
        print_string("Make sure there's a FAT-formatted disk attached.\n");
        goto halt;
    }

    print_string("FAT filesystem initialized successfully!\n\n");

    // ========================================================================
    // Example 1: Read a simple text file
    // ========================================================================
    print_string("=== Example 1: Reading README.TXT ===\n");

    FAT_FileHandle readme_file;
    if (fatOpen("README.TXT", &readme_file) == 0) {
        print_string("Successfully opened README.TXT\n");
        print_string("File size: ");
        print_dec(readme_file.file_size);
        print_string(" bytes\n\n");

        // Read and display the first 512 bytes
        char buffer[512];
        int bytes_read = fatRead(&readme_file, buffer, sizeof(buffer));

        if (bytes_read > 0) {
            print_string("Content:\n");
            print_string("----------------------------------------\n");
            for (int i = 0; i < bytes_read; i++) {
                kputchar(buffer[i]);
            }
            print_string("\n----------------------------------------\n");
        } else {
            print_string("ERROR: Failed to read file!\n");
        }
    } else {
        print_string("Could not open README.TXT (file may not exist)\n");
    }

    print_string("\n");

    // ========================================================================
    // Example 2: Read a binary file (like a kernel module)
    // ========================================================================
    print_string("=== Example 2: Reading KERNEL.BIN ===\n");

    FAT_FileHandle kernel_file;
    if (fatOpen("KERNEL.BIN", &kernel_file) == 0) {
        print_string("Successfully opened KERNEL.BIN\n");
        print_string("File size: ");
        print_dec(kernel_file.file_size);
        print_string(" bytes\n");

        // Read first 16 bytes and display as hex dump
        uint8_t header[16];
        int bytes_read = fatRead(&kernel_file, header, sizeof(header));

        if (bytes_read > 0) {
            print_string("First 16 bytes (hex):\n");
            for (int i = 0; i < bytes_read; i++) {
                if (header[i] < 0x10) kputchar('0');
                print_hex(header[i]);
                kputchar(' ');
                if ((i + 1) % 8 == 0) kputchar('\n');
            }
            print_string("\n");
        }
    } else {
        print_string("Could not open KERNEL.BIN\n");
    }

    print_string("\n");

    // ========================================================================
    // Example 3: Read a file in chunks (useful for large files)
    // ========================================================================
    print_string("=== Example 3: Reading DATA.DAT in chunks ===\n");

    FAT_FileHandle data_file;
    if (fatOpen("DATA.DAT", &data_file) == 0) {
        print_string("Successfully opened DATA.DAT\n");
        print_string("File size: ");
        print_dec(data_file.file_size);
        print_string(" bytes\n");

        // Read file in 256-byte chunks
        uint8_t chunk[256];
        uint32_t total_read = 0;
        int chunk_num = 0;

        while (data_file.position < data_file.file_size) {
            int bytes_read = fatRead(&data_file, chunk, sizeof(chunk));

            if (bytes_read <= 0) {
                print_string("Error reading chunk!\n");
                break;
            }

            total_read += bytes_read;
            chunk_num++;

            // Process chunk here (for demo, just count it)
        }

        print_string("Read ");
        print_dec(chunk_num);
        print_string(" chunks (");
        print_dec(total_read);
        print_string(" bytes total)\n");
    } else {
        print_string("Could not open DATA.DAT\n");
    }

    print_string("\n");

    // ========================================================================
    // Example 4: Load a file into memory at a specific address
    // ========================================================================
    print_string("=== Example 4: Loading PROGRAM.BIN into memory ===\n");

    FAT_FileHandle program_file;
    if (fatOpen("PROGRAM.BIN", &program_file) == 0) {
        print_string("Successfully opened PROGRAM.BIN\n");

        // Allocate memory for the entire file
        void* program_memory = kmalloc(program_file.file_size);

        if (program_memory) {
            // Read entire file into memory
            int bytes_read = fatRead(&program_file, program_memory, program_file.file_size);

            if (bytes_read == (int)program_file.file_size) {
                print_string("Successfully loaded ");
                print_dec(bytes_read);
                print_string(" bytes at address ");
                print_hex((uint32_t)program_memory);
                print_string("\n");

                // Now you could execute it or process it
                // For example, if it's a function:
                // void (*program_func)() = (void(*)())program_memory;
                // program_func();

            } else {
                print_string("ERROR: Failed to read complete file!\n");
            }

            // Don't forget to free when done (if your kfree works)
            // kfree(program_memory);
        } else {
            print_string("ERROR: Failed to allocate memory!\n");
        }
    } else {
        print_string("Could not open PROGRAM.BIN\n");
    }

    print_string("\n");

    // ========================================================================
    // Example 5: Verify file contents
    // ========================================================================
    print_string("=== Example 5: Verifying CONFIG.TXT ===\n");

    FAT_FileHandle config_file;
    if (fatOpen("CONFIG.TXT", &config_file) == 0) {
        char line_buffer[128];
        int bytes_read = fatRead(&config_file, line_buffer, sizeof(line_buffer) - 1);

        if (bytes_read > 0) {
            line_buffer[bytes_read] = '\0';  // Null terminate

            print_string("Configuration loaded:\n");
            print_string(line_buffer);
            print_string("\n");

            // You could parse configuration here
            // e.g., look for "resolution=1024x768" etc.
        }
    } else {
        print_string("Could not open CONFIG.TXT (using defaults)\n");
    }

    print_string("\n");

    // ========================================================================
    // Done!
    // ========================================================================
    print_string("=== FAT filesystem demo complete! ===\n");
    print_string("All file operations successful.\n\n");

halt:
    print_string("Kernel halting.\n");

    // Halt the CPU
    while (1) {
        __asm__ volatile ("hlt");
    }
}		
