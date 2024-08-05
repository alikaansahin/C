#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint-gcc.h>

// Define sector and cluster sizes
#define SECTOR_SIZE 512
#define CLUSTER_SIZE 1024
#define ENTRY_SIZE 32  // Each directory entry is 32 bytes
#define UNUSED_ENTRY 0x00  // Value indicating an unused directory entry
#define ATTR_LONG_NAME 0x0F  // Attribute for long name entries, not used here
#define ATTR_DIRECTORY 0x10  // Attribute indicating a directory entry
#define ATTR_VOLUME_ID 0x08  // Attribute for volume ID, usually the label of the disk
#define ATTR_MASK 0x28  // Mask for system, hidden, and volume ID attributes
#define FAT_ENTRY_SIZE 4  // Each FAT entry is 4 bytes in FAT32
#define ENTRIES_PER_SECTOR (SECTOR_SIZE / ENTRY_SIZE)  // Number of directory entries in a sector
#define FREE_CLUSTER 0x00000000 // Value of a free cluster in the FAT
#define END_OF_CHAIN 0x0FFFFFFF // Value indicating end of cluster chain
#define BOOT_SECTOR 0
#define FAT32_SIGNATURE_OFFSET 0x52
#define FAT32_SIGNATURE "FAT32   "
#define DELETED_ENTRY 0xE5

typedef struct {
    uint32_t sectors_per_cluster;
    uint32_t first_data_sector;      // The first sector of the data region of the filesystem
    uint32_t total_sectors;
    uint32_t total_clusters;
    uint32_t sectors_per_fat;
    uint32_t fat_size;  // Sectors per FAT
    uint32_t root_cluster;
    uint32_t num_fats;
    uint32_t root_dir_sectors;
    uint32_t fat_start_sector;
    uint32_t reserved_sectors;
    uint8_t jmp[3];
    char oem[8];
    uint16_t sector_size;
    uint8_t number_of_fats;
    uint16_t root_dir_entries;
    uint16_t total_sectors_short; // if zero, later field is used
    uint8_t media_descriptor;
    uint16_t fat_size_sectors;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;
    uint16_t flags;
    uint16_t version;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
    uint8_t boot_code[420];
    uint16_t boot_sector_signature;
} BootSector;

typedef struct {
    uint8_t name[11]; // Filename in 8.3 format
    uint8_t attr;     // File attributes
    uint8_t unused[20]; // Unused
    uint16_t fstClusHI; // High word of this entry's first cluster number
    uint16_t fstClusLO; // Low word of this entry's first cluster number
    uint8_t ntRes;    // Reserved for use by Windows NT
    uint8_t crtTimeTenth; // Millisecond stamp at file creation time
    uint16_t crtTime; // Time file was created
    uint16_t crtDate; // Date file was created
    uint16_t lstAccDate; // Last access date
    uint16_t wrtTime; // Time of last write
    uint16_t wrtDate; // Date of last write
    uint32_t fileSize; // Size of the file in bytes
} __attribute__((packed)) DirectoryEntry;  // Ensure packing to avoid padding

int initialize_fs(int fd, BootSector *bs);
int list_files(int fd, const BootSector *bs);
int read_file(int fd, const BootSector *bs, const char *filename, int ascii);
int create_file(int fd, BootSector *bs, const char *filename);
int find_file(int fd, const BootSector *bs, const char *filename, DirectoryEntry *entry);
int delete_file(int fd, BootSector *bs, const char *filename);
int write_data(int fd, BootSector *bs, const char *filename, int offset, int size, unsigned char data);

int read_file_ascii(int fd, const BootSector *bs, const char *filename);
int read_file_binary(int fd, const BootSector *bs, const char *filename);

int read_sector(int fd, uint32_t sector, void *buffer);
int write_sector(int fd, uint32_t sector, const void *buffer);
int calculate_sector_number(BootSector *bs, unsigned int cluster);

void format_filename(const char *input, char *formatted);

int update_fat(int fd, unsigned int cluster_number, unsigned int next_cluster);
uint32_t read_fat_entry(int fd, BootSector *bs, uint32_t cluster);
int write_fat_entry(int fd, BootSector *bs, uint32_t cluster, uint32_t value);

int free_cluster_chain(int fd, BootSector *bs, unsigned int start_cluster);
int find_free_cluster(int fd, BootSector *bs);
int read_next_cluster(int fd, BootSector *bs, unsigned int cluster);

int find_free_directory_entry(int fd, BootSector *bs, DirectoryEntry **entry);
int update_directory_entry(int fd, BootSector *bs, const char *filename, DirectoryEntry *entry);
int find_directory_entry(int fd, BootSector *bs, const char *filename, DirectoryEntry *entry, uint32_t *entry_sector, uint32_t *entry_offset);
int is_directory_exists(int fd, BootSector *bs, const char *filename);

int update_fat_entry(int fd, BootSector *bs, unsigned int free_cluster, unsigned int end_of_chain);
int update_fat_and_allocate_clusters(int fd, BootSector *bs, unsigned int cluster, int num_clusters);

int display_help();



int initialize_fs(int fd, BootSector *bs) {
    unsigned char buffer[SECTOR_SIZE];

    // Read the boot sector
    if (pread(fd, buffer, SECTOR_SIZE, BOOT_SECTOR * SECTOR_SIZE) != SECTOR_SIZE) {
        perror("Failed to read boot sector");
        return -1;
    }

    // Check FAT32 signature
    if (memcmp(buffer + FAT32_SIGNATURE_OFFSET, FAT32_SIGNATURE, 8) != 0) {
        fprintf(stderr, "Not a FAT32 volume\n");
        return -1;
    }

    // Parse necessary boot sector information
    bs->sectors_per_cluster = buffer[0x0D];
    bs->sectors_per_fat = *(uint32_t *)(buffer + 0x24);
    bs->root_cluster = *(uint32_t *)(buffer + 0x2C);
    bs->fat_start_sector = *(uint16_t *)(buffer + 0x0E) + *(uint16_t *)(buffer + 0x10); // Reserved sectors + number of FATs
    bs->first_data_sector = bs->fat_start_sector + bs->sectors_per_fat * buffer[0x10]; // FAT start + (sectors per FAT * number of FATs)
    bs->root_dir_sectors = ((buffer[0x11] * 32) + (SECTOR_SIZE - 1)) / SECTOR_SIZE; // Root entries * 32 bytes per entry
    bs->total_sectors = *(uint16_t *)(buffer + 0x13); // Total sectors in the volume
    bs->num_fats = buffer[0x10]; // Number of FATs
    bs->fat_size = bs->sectors_per_fat * bs->num_fats; // Total size of FAT in sectors
    bs->total_clusters = (bs->total_sectors - bs->first_data_sector) / bs->sectors_per_cluster; // Total clusters in the volume
    bs->reserved_sectors = *(uint16_t *)(buffer + 0x0E); // Number of reserved sectors
    return 0;
}

int list_files(int fd, const BootSector *bs) {
    unsigned int root_dir_sect = bs->root_cluster; // assuming root_cluster is sector number for simplicity
    char buffer[SECTOR_SIZE];

    printf("Listing files...\n");
    for (unsigned int i = 0; i < bs->sectors_per_cluster; i++) {
        if (read_sector(fd, root_dir_sect + i, buffer) != 0) {
            printf("Failed to read sector %u\n", root_dir_sect + i);
            return -1;
        }

        // Process each directory entry in the sector
        for (int entryOffset = 0; entryOffset < SECTOR_SIZE; entryOffset += ENTRY_SIZE) {
            DirectoryEntry *entry = (DirectoryEntry *)(buffer + entryOffset);

            // Check if the entry is free (first byte is 0x00 or 0xE5)
            if (entry->name[0] == 0x00) {
                break;  // No more entries after this point in this directory
            }
            if (entry->name[0] == 0xE5) {
                continue;  // This entry is marked as deleted
            }

            // Check if the entry is actually a file and not a long filename, volume, or system file
            if ((entry->attr & ATTR_MASK) == 0 && (entry->attr & ATTR_LONG_NAME) != ATTR_LONG_NAME) {
                char name[13];
                snprintf(name, sizeof(name), "%-8.8s.%3.3s", entry->name, entry->name + 8);

                printf("%s %lu bytes\n", name, (unsigned long)entry->fileSize);
            }
        }
    }
    return 0;
}


int read_file(int fd, const BootSector *bs, const char *filename, int ascii) {
    if (ascii) {
        return read_file_ascii(fd, bs,filename);
    } else {
        return read_file_binary(fd, bs, filename);
    }
}


int create_file(int fd, BootSector *bs, const char *filename) {
    printf("Creating file %s...\n", filename);
    printf("bs: %p\n", bs);
    DirectoryEntry entry;
    uint32_t entry_sector, entry_offset;

    if (is_directory_exists(fd, bs, filename) == 1) {
        fprintf(stderr, "A file with the name %s already exists.\n", filename);
        return -1;
    }

    DirectoryEntry *free_entry;
    if (find_free_directory_entry(fd, bs, &free_entry) != 0) {
        fprintf(stderr, "No free directory entries available.\n");
        return -1;
    }

    int free_cluster = find_free_cluster(fd, bs);
    if (free_cluster == -1) {
        fprintf(stderr, "No free clusters available.\n");
        return -1;
    }

    format_filename(filename, (char *)free_entry->name);

    // Update directory entry
    memcpy(free_entry->name, filename, 11);  // Assume filename is correctly formatted
    free_entry->attr = 0x20;  // Archive attribute
    free_entry->fstClusHI = (free_cluster >> 16) & 0xFFFF;
    free_entry->fstClusLO = free_cluster & 0xFFFF;
    free_entry->fileSize = 0;

    // Write the updated directory entry back to disk
    if (write_sector(fd, entry_sector, free_entry) != 0) {
        fprintf(stderr, "Failed to write updated directory entry.\n");
        return -1;
    }

    // Update FAT
    if (update_fat_entry(fd, bs, free_cluster, END_OF_CHAIN) != 0) {
        fprintf(stderr, "Failed to update FAT.\n");
        return -1;
    }

    printf("%s created successfully.\n", filename);

    return 0;
}


int delete_file(int fd, BootSector *bs, const char *filename) {
    printf("Deleting file %s...\n", filename);

    DirectoryEntry entry;
    uint32_t entry_sector, entry_offset;
    if (find_directory_entry(fd, bs, filename, &entry, &entry_sector, &entry_offset) != 0) {
        fprintf(stderr, "File not found.\n");
        return -1;
    }

    // Free the cluster chain in the FAT
    unsigned int start_cluster = ((unsigned int)entry.fstClusHI << 16) | entry.fstClusLO;
    if (free_cluster_chain(fd, bs, start_cluster) != 0) {
        fprintf(stderr, "Failed to free cluster chain.\n");
        return -1;
    }

    // Clear the directory entry
    memset(&entry, 0, sizeof(entry));
    entry.name[0] = DELETED_ENTRY;  // Mark the entry as deleted
    if (write_sector(fd, entry_sector, &entry) != 0) {
        fprintf(stderr, "Failed to clear directory entry.\n");
        return -1;
    }

    return 0;
}


int write_data(int fd, BootSector *bs, const char *filename, int offset, int size, unsigned char data) {
    printf("Writing data to %s...\n", filename);

    DirectoryEntry entry;
    if (find_file(fd, bs, filename, &entry) != 0) {
        fprintf(stderr, "File not found.\n");
        return -1;
    }

    unsigned int start_cluster = ((unsigned int)entry.fstClusHI << 16) | entry.fstClusLO;
    unsigned int cluster = start_cluster;
    int cluster_offset = offset / CLUSTER_SIZE;
    int byte_offset = offset % CLUSTER_SIZE;
    int written = 0;

    // Navigate to the correct cluster based on the offset
    while (cluster_offset > 0) {
        cluster = read_next_cluster(fd, bs, cluster);  // Function to get the next cluster from FAT
        if (cluster >= END_OF_CHAIN) {
            // Need to allocate new clusters if we run out before reaching the offset
            if (update_fat_and_allocate_clusters(fd, bs, cluster, 1) != 0) {
                fprintf(stderr, "Failed to allocate new cluster.\n");
                return -1;
            }
        }
        cluster_offset--;
    }

    // Write data to the clusters
    char buffer[CLUSTER_SIZE];
    while (written < size) {
        if (read_sector(fd, calculate_sector_number(bs, cluster), buffer) != 0) {
            fprintf(stderr, "Failed to read cluster.\n");
            return -1;
        }

        int to_write = CLUSTER_SIZE - byte_offset;
        if (written + to_write > size) to_write = size - written;
        memset(buffer + byte_offset, data, to_write);
        if (write_sector(fd, calculate_sector_number(bs, cluster), buffer) != 0) {
            fprintf(stderr, "Failed to write cluster.\n");
            return -1;
        }

        written += to_write;
        byte_offset = 0;  // Reset byte offset for subsequent clusters

        // Get next cluster, allocate if end of chain
        unsigned int next_cluster = read_next_cluster(fd, bs, cluster);
        if (next_cluster >= END_OF_CHAIN) {
            if (update_fat_and_allocate_clusters(fd, bs, cluster, 1) != 0) {
                fprintf(stderr, "Failed to allocate new cluster.\n");
                return -1;
            }
        }
        cluster = next_cluster;
    }

    // Update file size in directory entry if necessary
    if ((uint32_t) (offset + size) > entry.fileSize) {
        entry.fileSize = offset + size;
        if (update_directory_entry(fd, bs, filename, &entry) != 0) {
            fprintf(stderr, "Failed to update directory entry.\n");
            return -1;
        }
    }

    return 0;
}


int read_file_ascii(int fd, const BootSector *bs, const char *filename) {
    printf("Reading file %s in ASCII mode...\n", filename);

    DirectoryEntry entry;
    if (find_file(fd, bs, filename, &entry) != 0) {
        fprintf(stderr, "File not found.\n");
        return -1;
    }

    // Calculate the first cluster number
    unsigned int cluster = ((unsigned int)entry.fstClusHI << 16) | entry.fstClusLO;
    unsigned int nextCluster = cluster;
    unsigned char *buffer = malloc(CLUSTER_SIZE);
    if (!buffer) {
        perror("Memory allocation failed");
        return -1;
    }

    // Read clusters until end of file
    do {
        // Compute sector number from cluster number
        unsigned int sector_number = bs->root_cluster + (nextCluster - 2) * bs->sectors_per_cluster;
        for (unsigned int i = 0; i < bs->sectors_per_cluster; i++) {
            if (read_sector(fd, sector_number + i, buffer + (i * SECTOR_SIZE)) != 0) {
                free(buffer);
                return -1;
            }
        }

        // Print out the data in ASCII format
        fwrite(buffer, 1, CLUSTER_SIZE, stdout);  // This might print extra data beyond file size!

        // Fetch next cluster number from FAT
        // Assume function read_fat_entry() which reads the FAT for the next cluster
        nextCluster = read_fat_entry(fd, bs, nextCluster);
    } while (nextCluster < END_OF_CHAIN);

    free(buffer);
    return 0;
}


int read_file_binary(int fd, const BootSector *bs, const char *filename) {
    printf("Reading file %s in binary mode...\n", filename);

    DirectoryEntry entry;
    if (find_file(fd, bs, filename, &entry) != 0) {
        fprintf(stderr, "File not found.\n");
        return -1;
    }

    // Calculate the first cluster number
    unsigned int cluster = ((unsigned int)entry.fstClusHI << 16) | entry.fstClusLO;
    unsigned int nextCluster = cluster;
    unsigned char *buffer = malloc(CLUSTER_SIZE);
    if (!buffer) {
        perror("Memory allocation failed");
        return -1;
    }

    // Read clusters until end of file
    do {
        // Compute sector number from cluster number
        unsigned int sector_number = bs->root_cluster + (nextCluster - 2) * bs->sectors_per_cluster;
        for (unsigned int i = 0; i < bs->sectors_per_cluster; i++) {
            if (read_sector(fd, sector_number + i, buffer + (i * SECTOR_SIZE)) != 0) {
                free(buffer);
                return -1;
            }
        }

        // Print out the data in binary format
        fwrite(buffer, 1, CLUSTER_SIZE, stdout);  // This might print extra data beyond file size!

        // Fetch next cluster number from FAT
        // Assume function read_fat_entry() which reads the FAT for the next cluster
        nextCluster = read_fat_entry(fd, bs, nextCluster);
    } while (nextCluster < END_OF_CHAIN);

    free(buffer);
    return 0;
}


int read_sector(int fd, unsigned int sector_number, void *buffer) {
    off_t offset = sector_number * SECTOR_SIZE;
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        perror("Failed to seek");
        return -1;
    }
    if (read(fd, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("Failed to read sector");
        return -1;
    }
    return 0;
}


int write_sector(int fd, unsigned int sector_number, const void *buffer) {
    off_t offset = sector_number * SECTOR_SIZE;
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        perror("Failed to seek");
        return -1;
    }
    if (write(fd, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("Failed to write sector");
        return -1;
    }
    return 0;
}


void format_filename(const char *input, char *formatted) {
    memset(formatted, ' ', 11);  // Initialize with spaces
    int i = 0, j = 0;
    while (input[i] != '\0' && j < 11) {
        if (input[i] == '.') {
            j = 8;  // Skip to the extension part
        } else {
            formatted[j++] = (input[i] >= 'a' && input[i] <= 'z') ? input[i] - 'a' + 'A' : input[i];
        }
        i++;
    }
}


char to_fat_filename_char(char ch) {
    // Convert ASCII character to uppercase FAT filename character
    if (ch == 0) return ' ';
    return (ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch;
}


int write_fat_entry(int fd, BootSector *bs, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * FAT_ENTRY_SIZE;
    uint32_t fat_sector = bs->fat_start_sector + (fat_offset / SECTOR_SIZE);
    uint32_t entry_offset_in_sector = fat_offset % SECTOR_SIZE;

    unsigned char buffer[SECTOR_SIZE];
    if (read_sector(fd, fat_sector, buffer) != 0) {
        return -1;
    }

    // Set the new value in the buffer
    *((uint32_t *)(buffer + entry_offset_in_sector)) = value;

    // Write the modified sector back to the disk
    return write_sector(fd, fat_sector, buffer);
}


int free_cluster_chain(int fd, BootSector *bs, unsigned int start_cluster) {
    uint32_t cluster = start_cluster;
    uint32_t next_cluster;

    while (cluster < END_OF_CHAIN) {
        // Read the next cluster from the FAT
        next_cluster = read_fat_entry(fd, bs, cluster);

        // Free the current cluster
        if (write_fat_entry(fd, bs, cluster, FREE_CLUSTER) != 0) {
            fprintf(stderr, "Failed to write FAT entry for cluster %u\n", cluster);
            return -1;
        }

        // Check if we've reached the end of the chain
        if (next_cluster >= END_OF_CHAIN) {
            break;
        }

        cluster = next_cluster;
    }

    return 0;
}


uint32_t read_fat_entry(int fd, BootSector *bs, uint32_t cluster) {
    // Assuming FAT starts at a fixed sector and FAT entries are 4 bytes
    uint32_t fat_offset = cluster * FAT_ENTRY_SIZE;
    uint32_t fat_sector = bs->fat_start_sector + (fat_offset / SECTOR_SIZE);
    uint32_t entry_offset_in_sector = fat_offset % SECTOR_SIZE;

    unsigned char buffer[SECTOR_SIZE];
    if (read_sector(fd, fat_sector, buffer) != 0) {
        printf("Failed to read FAT sector\n");
        return END_OF_CHAIN;  // Indicate error
    }

    // Assume little-endian
    return *((uint32_t *)(buffer + entry_offset_in_sector)) & 0x0FFFFFFF; // Mask to 28 bits
}


int find_file(int fd, const BootSector *bs, const char *filename, DirectoryEntry *entry) {
    char fat_filename[12];
    format_filename(filename, fat_filename);
    fat_filename[11] = '\0';  // Ensure null termination for debugging

    uint32_t sector = bs->first_data_sector;  // Assuming this is the start of the root directory
    unsigned char buffer[SECTOR_SIZE];

    while (sector < bs->first_data_sector + bs->sectors_per_cluster) {
        if (read_sector(fd, sector, buffer) != 0) {
            perror("Failed to read sector");
            return -1;
        }

        for (int i = 0; i < ENTRIES_PER_SECTOR; i++) {
            DirectoryEntry *current = (DirectoryEntry *)(buffer + i * ENTRY_SIZE);
            if (memcmp(current->name, fat_filename, 11) == 0) {
                memcpy(entry, current, sizeof(DirectoryEntry));
                return 0;  // File found
            }
        }

        sector++;  // Move to the next sector
    }
    printf("File not found\n");
    return -1;  // File not found
}

int find_free_directory_entry(int fd, BootSector *bs, DirectoryEntry **entryOut) {
    uint32_t sector = bs->first_data_sector;  // Assuming this is where the directory starts
    unsigned char buffer[SECTOR_SIZE];
    uint32_t total_sectors = bs->root_dir_sectors;  // This would typically be calculated based on volume info

    for (uint32_t i = 0; i < total_sectors; ++i) {
        if (read_sector(fd, sector + i, buffer) != 0) {
            perror("Failed to read sector");
            return -1;
        }

        for (int j = 0; j < ENTRIES_PER_SECTOR; j++) {
            DirectoryEntry *current = (DirectoryEntry *)(buffer + j * ENTRY_SIZE);
            printf("Entry %d, First Char: %02X\n", j, current->name[0]);  // Debugging output
            if (current->name[0] == UNUSED_ENTRY || current->name[0] == DELETED_ENTRY) {
                *entryOut = current;  // Assign the found free entry to the output parameter
                return 0;  // Free entry found
            }
        }
    }

    return -1;  // No free entry found
}

int find_free_cluster(int fd, BootSector *bs) {
    uint32_t cluster_number = 2;  // Cluster numbering starts at 2 in FAT32
    uint32_t value = 0;

    while (cluster_number < bs->total_clusters) {
        if (read_fat_entry(fd, bs, cluster_number) != 0) {
            fprintf(stderr, "Error reading FAT entry for cluster %u\n", cluster_number);
            return -1;  // Error reading the FAT
        }

        if (value == FREE_CLUSTER) {
            return cluster_number;  // Free cluster found
        }

        cluster_number++;
    }

    return -1;  // No free cluster found
}

int update_fat_entry(int fd, BootSector *bs, unsigned int free_cluster, unsigned int end_of_chain) {
    uint32_t fat_offset = free_cluster * FAT_ENTRY_SIZE;
    uint32_t fat_sector = bs->fat_start_sector + (fat_offset / SECTOR_SIZE);
    uint32_t entry_offset_in_sector = fat_offset % SECTOR_SIZE;

    unsigned char buffer[SECTOR_SIZE];
    // Read the sector containing the FAT entry
    if (read_sector(fd, fat_sector, buffer) != 0) {
        perror("Failed to read FAT sector");
        return -1;
    }

    // Update the FAT entry
    uint32_t *entry_ptr = (uint32_t *)(buffer + entry_offset_in_sector);
    *entry_ptr = end_of_chain; // Set to the end of chain marker or other specified value

    // Write the updated sector back to the disk
    if (write_sector(fd, fat_sector, buffer) != 0) {
        perror("Failed to write FAT sector");
        return -1;
    }

    return 0;
}

int read_next_cluster(int fd, BootSector *bs, unsigned int cluster) {
    uint32_t next_cluster = read_fat_entry(fd, bs, cluster);

    // Check if the next cluster is valid or marks the end of the chain
    if (next_cluster >= END_OF_CHAIN) {
        return -1;  // No further clusters in the chain or error occurred
    }

    return next_cluster;  // Return the next cluster number
}


int update_fat_and_allocate_clusters(int fd, BootSector *bs, unsigned int cluster, int num_clusters) {
    uint32_t current_cluster = cluster;
    uint32_t last_allocated = 0;

    // Navigate to the end of the current chain
    while (1) {
        uint32_t next_cluster = read_fat_entry(fd, bs, current_cluster);
        if (next_cluster == END_OF_CHAIN || next_cluster == FREE_CLUSTER) {
            break;  // Found the end of the chain
        }
        current_cluster = next_cluster;
    }

    // Allocate new clusters and update the FAT
    for (int i = 0; i < num_clusters; i++) {
        uint32_t new_cluster = find_free_cluster(fd, bs);
        if (new_cluster == (uint32_t)-1) {
            fprintf(stderr, "No free clusters available\n");
            return -1;  // No free clusters available
        }

        // Link the new cluster
        if (write_fat_entry(fd, bs, current_cluster, new_cluster) != 0) {
            fprintf(stderr, "Failed to update FAT entry\n");
            return -1;  // Failed to write FAT entry
        }

        current_cluster = new_cluster;  // Move to the newly allocated cluster
        last_allocated = new_cluster;
    }

    // Mark the last new cluster as the end of the chain
    if (write_fat_entry(fd, bs, last_allocated, END_OF_CHAIN) != 0) {
        fprintf(stderr, "Failed to mark the last cluster\n");
        return -1;
    }

    return 0;  // Success
}

int calculate_sector_number(BootSector *bs, unsigned int cluster) {
    if (cluster < 2) {
        // Invalid cluster number (0 and 1 are reserved)
        return -1;
    }

    // Calculate the first sector of the specified cluster
    return (cluster - 2) * bs->sectors_per_cluster + bs->first_data_sector;
}

int update_directory_entry(int fd, BootSector *bs, const char *filename, DirectoryEntry *entry) {
    DirectoryEntry foundEntry;
    uint32_t entry_sector;
    uint32_t entry_offset;

    // Find the directory entry for the filename
    if (find_directory_entry(fd, bs, filename, &foundEntry, &entry_sector, &entry_offset) != 0) {
        fprintf(stderr, "Directory entry not found for %s\n", filename);
        return -1;  // Entry not found
    }

    // Update the directory entry with new data
    memcpy(&foundEntry, entry, sizeof(DirectoryEntry));

    // Read the sector containing the directory entry
    unsigned char buffer[SECTOR_SIZE];
    if (read_sector(fd, entry_sector, buffer) != 0) {
        perror("Failed to read sector");
        return -1;
    }

    // Modify the specific directory entry within the sector
    memcpy(buffer + entry_offset, &foundEntry, sizeof(DirectoryEntry));

    // Write the updated sector back to the disk
    if (write_sector(fd, entry_sector, buffer) != 0) {
        perror("Failed to write sector");
        return -1;
    }

    return 0;  // Successfully updated
}

int is_directory_exists(int fd, BootSector *bs, const char *filename) {
    DirectoryEntry entry;
    if (find_directory_entry(fd, bs, filename, &entry, NULL, NULL) == 0) {
        return 1;
    }
    return 0;
}

int find_directory_entry(int fd, BootSector *bs, const char *filename, DirectoryEntry *entry, uint32_t *entry_sector, uint32_t *entry_offset) {
    char fat_filename[11];
    format_filename(filename, fat_filename);
    fat_filename[11] = '\0'; // Ensure null termination

    uint32_t sector_count = bs->root_dir_sectors;
    printf("sector_count: %d\n", sector_count);
    uint32_t sector = bs->first_data_sector - sector_count; // Calculate the first sector of the root directory

    u_char buffer[SECTOR_SIZE];
    for (uint32_t i = 0; i < sector_count; ++i, ++sector) {
        if (read_sector(fd, sector, buffer) != 0) {
            fprintf(stderr, "Error reading sector %u\n", sector);
            return -1;
        }

        for (int j = 0; j < ENTRIES_PER_SECTOR; ++j) {
            uint32_t offset = j * ENTRY_SIZE;
            DirectoryEntry *current = (DirectoryEntry *)(buffer + offset);
            if (current->name[0] == UNUSED_ENTRY || current->name[0] == DELETED_ENTRY) {
                continue; // Skip this entry if it's unused or deleted
            }
            if (memcmp(current->name, fat_filename, 11) == 0) {
                memcpy(entry, current, sizeof(DirectoryEntry));
                if (entry_sector) *entry_sector = sector;
                if (entry_offset) *entry_offset = offset;
                return 0; // Found the file
            }
        }
    }
    fprintf(stderr, "File not found or all sectors are empty.\n");
    return -1; // File not found or all sectors are empty
}

int display_help() {
    printf("Usage: <program_name> <option> [additional arguments]\n");
    printf("Options:\n");
    printf("-l: List files\n");
    printf("-r: Read file\n");
    printf("-c: Create file\n");
    printf("-d: Delete file\n");
    printf("-w: Write data to file\n");
    printf("-h: Display this help message\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "Usage: %s <disk_image> <option> [additional arguments]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0) {
        return display_help();
    }

    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("Failed to open disk image");
        return 1;
    }

    BootSector bs;
    if (read(fd, &bs, sizeof(bs)) != sizeof(bs)) {
        perror("Failed to read boot sector");
        close(fd);
        return 1;
    }

    // Output some key values for debugging
    printf("Sectors Per Cluster: %d\n", bs.sectors_per_cluster);
    printf("Number of FATs: %d\n", bs.number_of_fats);
    printf("Total Sectors: %d\n", bs.total_sectors_long);

    if (strcmp(argv[2], "-l") == 0) {
        return list_files(fd , &bs);
    } else if (strcmp(argv[2], "-r") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Insufficient arguments for read operation\n");
            return 1;
        }
        int ascii = strcmp(argv[3], "-a") == 0;
        return read_file(fd, &bs, argv[4], ascii);
    } else if (strcmp(argv[2], "-c") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Insufficient arguments for create operation\n");
            return 1;
        }
        return create_file(fd, &bs, argv[3]);
    } else if (strcmp(argv[2], "-d") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Insufficient arguments for delete operation\n");
            return 1;
        }
        return delete_file(fd, &bs, argv[3]);
    } else if (strcmp(argv[2], "-w") == 0) {
        if (argc < 7) {
            fprintf(stderr, "Insufficient arguments for write operation\n");
            return 1;
        }
        int offset = atoi(argv[4]);
        int size = atoi(argv[5]);
        unsigned char data = (unsigned char)atoi(argv[6]);
        return write_data(fd, &bs, argv[3], offset, size, data);
    } else {
        fprintf(stderr, "Invalid option\n");
        return 1;
    }

    close(fd);
    return 0;
}