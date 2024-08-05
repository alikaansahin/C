#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <linux/msdos_fs.h>

#define SECTORSIZE 512
#define CLUSTERSIZE 1024
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define END_OF_CHAIN 0x0FFFFFFF


int readsector(int fd, unsigned char *buf, uint snum);
int writesector(int fd, unsigned char *buf, uint snum);
void list_files(int fd);
void read_ascii(int fd, char *filename);
void read_binary(int fd, char *filename);
void create_file(int fd, char *filename);
void delete_file(int fd, char *filename);
void write_data(int fd, char *filename, int offset, int n, unsigned char data);
void print_help();
int find_file(int fd, char *filename, struct msdos_dir_entry *dir_entry);
unsigned int get_cluster_offset(int cluster, struct fat_boot_sector *boot_sector);
int get_next_cluster(int fd, unsigned int cluster, struct fat_boot_sector *boot_sector);
void mark_cluster_free(int fd, unsigned int cluster, struct fat_boot_sector *boot_sector);
int is_empty_segment(unsigned char *buf, int size);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        exit(1);
    }

    char *diskimage = argv[1];
    int fd = open(diskimage, O_RDWR);
    if (fd < 0) {
        perror("Failed to open disk image");
        exit(1);
    }

    if (argc == 3 && strcmp(argv[2], "-l") == 0) {
        list_files(fd);
    } else if (argc == 5 && strcmp(argv[2], "-r") == 0 && strcmp(argv[3], "-a") == 0) {
        read_ascii(fd, argv[4]);
    } else if (argc == 5 && strcmp(argv[2], "-r") == 0 && strcmp(argv[3], "-b") == 0) {
        read_binary(fd, argv[4]);
    } else if (argc == 4 && strcmp(argv[2], "-c") == 0) {
        create_file(fd, argv[3]);
    } else if (argc == 4 && strcmp(argv[2], "-d") == 0) {
        delete_file(fd, argv[3]);
    } else if (argc == 7 && strcmp(argv[2], "-w") == 0) {
        int offset = atoi(argv[4]);
        int n = atoi(argv[5]);
        unsigned char data = (unsigned char)atoi(argv[6]);
        write_data(fd, argv[3], offset, n, data);
    } else if (argc == 3 && strcmp(argv[2], "-h") == 0) {
        print_help();
    } else {
        print_help();
    }

    close(fd);
    return 0;
}

int readsector(int fd, unsigned char *buf, uint snum) {
    off_t offset = snum * SECTORSIZE;
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek");
        return -1;
    }
    if (read(fd, buf, SECTORSIZE) != SECTORSIZE) {
        perror("read");
        return -1;
    }
    return 0;
}

int writesector(int fd, unsigned char *buf, uint snum) {
    off_t offset = snum * SECTORSIZE;
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek");
        return -1;
    }
    if (write(fd, buf, SECTORSIZE) != SECTORSIZE) {
        perror("write");
        return -1;
    }
    fsync(fd);
    return 0;
}

void list_files(int fd) {
    struct fat_boot_sector boot_sector;
    if (readsector(fd, (unsigned char *)&boot_sector, 0) == -1) {
        fprintf(stderr, "Failed to read boot sector\n");
        return;
    }

    unsigned int root_cluster = boot_sector.fat32.root_cluster;
    unsigned int cluster_offset = get_cluster_offset(root_cluster, &boot_sector);

    unsigned char cluster[CLUSTERSIZE];
    if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
        perror("lseek");
        return;
    }
    if (read(fd, cluster, CLUSTERSIZE) != CLUSTERSIZE) {
        perror("read");
        return;
    }

    struct msdos_dir_entry *dir_entry = (struct msdos_dir_entry *)cluster;
    while (dir_entry->name[0] != 0x00) {
        if (dir_entry->name[0] != 0xe5 && (dir_entry->attr & ATTR_DIRECTORY) == 0 && dir_entry->size > 0) {
            /**char filename[13];
            strncpy(filename, (char *)dir_entry->name, 8);
            filename[8] = '.';
            strncpy(filename + 9, (char *)(dir_entry->name + 8), 3);
            filename[12] = '\0';*/
            printf("%s %d\n", dir_entry->name, dir_entry->size);
        }
        dir_entry++;
    }
}

void read_ascii(int fd, char *filename) {
    struct fat_boot_sector boot_sector;
    if (readsector(fd, (unsigned char *)&boot_sector, 0) == -1) {
        fprintf(stderr, "Failed to read boot sector\n");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(fd, filename, &dir_entry) == -1) {
        fprintf(stderr, "File not found\n");
        return;
    }

    unsigned char buf[CLUSTERSIZE];
    unsigned int cluster = dir_entry.start;
    unsigned int size = dir_entry.size;

    while (size > 0 && cluster < 0x0FFFFFF8) {
        unsigned int cluster_offset = get_cluster_offset(cluster, &boot_sector);
        if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
            perror("lseek");
            return;
        }
        int bytes_to_read = (size > CLUSTERSIZE) ? CLUSTERSIZE : size;
        if (read(fd, buf, bytes_to_read) != bytes_to_read) {
            perror("read");
            return;
        }

        if (!is_empty_segment(buf, bytes_to_read)) {
            write(1, buf, bytes_to_read);
        }

        size -= bytes_to_read;
        cluster = get_next_cluster(fd, cluster, &boot_sector);
    }
}

void read_binary(int fd, char *filename) {
    struct fat_boot_sector boot_sector;
    if (readsector(fd, (unsigned char *)&boot_sector, 0) == -1) {
        fprintf(stderr, "Failed to read boot sector\n");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(fd, filename, &dir_entry) == -1) {
        fprintf(stderr, "File not found\n");
        return;
    }

    unsigned char buf[CLUSTERSIZE];
    unsigned int cluster = dir_entry.start;
    unsigned int size = dir_entry.size;
    unsigned int offset = 0;

    while (size > 0 && cluster < 0x0FFFFFF8) {
        printf("Reading cluster %d\n", cluster);
        unsigned int cluster_offset = get_cluster_offset(cluster, &boot_sector);
        if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
            perror("lseek");
            return;
        }
        int bytes_to_read = (size > CLUSTERSIZE) ? CLUSTERSIZE : size;
        if (read(fd, buf, bytes_to_read) != bytes_to_read) {
            perror("read");
            return;
        }

        if (!is_empty_segment(buf, bytes_to_read)) {
            for (int i = 0; i < bytes_to_read; i += 16) {
                printf("%08x: ", offset + i);
                for (int j = 0; j < 16 && (i + j) < bytes_to_read; j++) {
                    printf("%02x ", buf[i + j]);
                }
                printf("\n");
            }
        }

        offset += bytes_to_read;
        size -= bytes_to_read;
        cluster = get_next_cluster(fd, cluster, &boot_sector);
    }
}


int read_fat_entry(int fd, struct fat_boot_sector *boot_sector, unsigned int cluster, unsigned char *fat_entry) {
    unsigned int fat_offset = cluster * 4;  // Each FAT entry in FAT32 is 4 bytes
    unsigned int fat_sector_number = boot_sector->reserved + (fat_offset / SECTORSIZE);
    unsigned int entry_offset_in_sector = fat_offset % SECTORSIZE;

    unsigned char sector_buffer[SECTORSIZE];

    if (readsector(fd, sector_buffer, fat_sector_number) == -1) {
        fprintf(stderr, "Error reading FAT sector %u\n", fat_sector_number);
        return -1;  // Failed to read sector
    }

    // Copy the 4 bytes of the FAT entry into the provided buffer
    memcpy(fat_entry, sector_buffer + entry_offset_in_sector, 4);
    return 0;  // Success
}


unsigned int find_free_cluster(int fd, struct fat_boot_sector *boot_sector) {
    unsigned int total_clusters = (boot_sector->total_sect - (boot_sector->reserved + boot_sector->fats * boot_sector->fat32.length)) / boot_sector->sec_per_clus;
    unsigned char fat_entry[4];

    for (unsigned int cluster = 2; cluster < total_clusters; cluster++) {
        if (read_fat_entry(fd, boot_sector, cluster, fat_entry) == -1) {
            fprintf(stderr, "Failed to read FAT for cluster %u\n", cluster);
            return 0xFFFFFFFF;
        }

        if (*((unsigned int *)fat_entry) == 0) {  // Free cluster
            return cluster;
        }
    }

    return 0xFFFFFFFF;  // No free clusters found
}

void mark_cluster_used(int fd, unsigned int cluster, struct fat_boot_sector *boot_sector) {
    unsigned int fat_offset = boot_sector->reserved * SECTORSIZE + cluster * 4;
    unsigned int eoc_marker = END_OF_CHAIN;  // Mark as end of chain

    if (lseek(fd, fat_offset, SEEK_SET) == -1) {
        perror("lseek failed to find FAT entry");
        return;
    }
    if (write(fd, &eoc_marker, sizeof(eoc_marker)) != sizeof(eoc_marker)) {
        perror("write failed to update FAT entry");
        return;
    }
    fsync(fd);  // Make sure the change is written to disk
}



void create_file(int fd, char *filename) {
    struct fat_boot_sector boot_sector;
    struct msdos_dir_entry dir_entry;
    unsigned char cluster[CLUSTERSIZE];

    if (readsector(fd, (unsigned char *)&boot_sector, 0) == -1) {
        fprintf(stderr, "Failed to read boot sector\n");
        return;
    }

    if (find_file(fd, filename, &dir_entry) == 0) {
        fprintf(stderr, "File already exists\n");
        return;
    }

    unsigned int free_cluster = find_free_cluster(fd, &boot_sector);
    if (free_cluster == 0xFFFFFFFF) {
        fprintf(stderr, "No free clusters available\n");
        return;
    }

    mark_cluster_used(fd, free_cluster, &boot_sector);

    // Find free directory entry
    unsigned int root_cluster = boot_sector.fat32.root_cluster;
    unsigned int cluster_offset = get_cluster_offset(root_cluster, &boot_sector);
    if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
        perror("lseek");
        return;
    }
    if (read(fd, cluster, CLUSTERSIZE) != CLUSTERSIZE) {
        perror("read");
        return;
    }

    struct msdos_dir_entry *dir_entry_ptr = (struct msdos_dir_entry *)cluster;
    while (dir_entry_ptr->name[0] != 0x00 && dir_entry_ptr->name[0] != 0xe5) {
        dir_entry_ptr++;
    }

    // Create new directory entry
    memset(dir_entry_ptr, 0, sizeof(struct msdos_dir_entry));
    strncpy((char *)dir_entry_ptr->name, filename, MSDOS_NAME);
    dir_entry_ptr->attr = ATTR_ARCHIVE; // Set file attribute
    dir_entry_ptr->size = 0;
    dir_entry_ptr->start = free_cluster;
    printf("Created file %s with start cluster %d\n", filename, free_cluster);

    // Write the updated directory entry back to the disk
    if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
        perror("lseek");
        return;
    }
    if (write(fd, cluster, CLUSTERSIZE) != CLUSTERSIZE) {
        perror("write");
        return;
    }

    fsync(fd);
}


void mark_cluster_free(int fd, unsigned int cluster, struct fat_boot_sector *boot_sector) {
    unsigned int fat_offset = boot_sector->reserved * SECTORSIZE + cluster * 4; // Calculate FAT offset
    unsigned char zero[4] = {0, 0, 0, 0}; // Four bytes of zero to mark cluster as free

    if (lseek(fd, fat_offset, SEEK_SET) == -1) {
        perror("lseek");
        return;
    }
    if (write(fd, zero, 4) != 4) {
        perror("write");
        return;
    }
    fsync(fd);
}

void delete_file(int fd, char *filename) {
    struct fat_boot_sector boot_sector;
    if (readsector(fd, (unsigned char *)&boot_sector, 0) == -1) {
        fprintf(stderr, "Failed to read boot sector\n");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(fd, filename, &dir_entry) == -1) {
        fprintf(stderr, "File not found\n");
        return;
    }

    // Deallocate clusters
    unsigned int cluster = dir_entry.start;
    while (cluster < 0x0FFFFFF8) {
        unsigned int next_cluster = get_next_cluster(fd, cluster, &boot_sector);
        mark_cluster_free(fd, cluster, &boot_sector);
        if (cluster == next_cluster) {
            break; // Prevent infinite loop
        }
        cluster = next_cluster;
    }

    // Find the directory entry again to mark it as deleted
    unsigned int root_cluster = boot_sector.fat32.root_cluster;
    unsigned int cluster_offset = get_cluster_offset(root_cluster, &boot_sector);

    unsigned char cluster_buf[CLUSTERSIZE];
    if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
        perror("lseek");
        return;
    }
    if (read(fd, cluster_buf, CLUSTERSIZE) != CLUSTERSIZE) {
        perror("read");
        return;
    }

    struct msdos_dir_entry *dir_entry_ptr = (struct msdos_dir_entry *)cluster_buf;
    while (dir_entry_ptr->name[0] != 0x00) {
        if (strncmp((char *)dir_entry_ptr->name, filename, MSDOS_NAME) == 0) {
            dir_entry_ptr->name[0] = 0xe5;
            if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
                perror("lseek");
                return;
            }
            if (write(fd, cluster_buf, CLUSTERSIZE) != CLUSTERSIZE) {
                perror("write");
                return;
            }
            fsync(fd);
            return;
        }
        dir_entry_ptr++;
    }
}

void write_data(int fd, char *filename, int offset, int n, unsigned char data) {
    struct fat_boot_sector boot_sector;
    if (readsector(fd, (unsigned char *)&boot_sector, 0) == -1) {
        fprintf(stderr, "Failed to read boot sector\n");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(fd, filename, &dir_entry) == -1) {
        fprintf(stderr, "File not found\n");
        return;
    }

    unsigned int cluster = dir_entry.start;
    unsigned int cluster_offset;
    unsigned char buf[CLUSTERSIZE];
    int remaining_bytes = n;

    while (offset >= CLUSTERSIZE) {
        cluster = get_next_cluster(fd, cluster, &boot_sector);
        if (cluster >= 0x0FFFFFF8) {
            fprintf(stderr, "Offset exceeds file size\n");
            return;
        }
        offset -= CLUSTERSIZE;
    }

    while (remaining_bytes > 0) {
        cluster_offset = get_cluster_offset(cluster, &boot_sector);

        if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
            perror("lseek");
            return;
        }
        if (read(fd, buf, CLUSTERSIZE) != CLUSTERSIZE) {
            perror("read");
            return;
        }

        int bytes_to_write = CLUSTERSIZE - offset;
        if (bytes_to_write > remaining_bytes) {
            bytes_to_write = remaining_bytes;
        }

        memset(buf + offset, data, bytes_to_write);
        offset = 0;  // Reset offset for next cluster

        if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
            perror("lseek");
            return;
        }
        if (write(fd, buf, CLUSTERSIZE) != CLUSTERSIZE) {
            perror("write");
            return;
        }

        remaining_bytes -= bytes_to_write;
        if (remaining_bytes > 0) {
            cluster = get_next_cluster(fd, cluster, &boot_sector);
            if (cluster >= 0x0FFFFFF8) {
                fprintf(stderr, "Insufficient clusters for writing data\n");
                return;
            }
        }
    }

    fsync(fd);
}

void print_help() {
    printf("Usage: fatmod DISKIMAGE [OPTION]...\n");
    printf("Manipulate a FAT32 disk image.\n\n");
    printf("Options:\n");
    printf("  -l                      List files in the root directory\n");
    printf("  -r -a FILENAME          Read file in ASCII form\n");
    printf("  -r -b FILENAME          Read file in binary form\n");
    printf("  -c FILENAME             Create a new file\n");
    printf("  -d FILENAME             Delete a file\n");
    printf("  -w FILENAME OFFSET N DATA  Write DATA byte N times to the file starting at OFFSET\n");
    printf("  -h                      Display this help message\n");
}

int find_file(int fd, char *filename, struct msdos_dir_entry *dir_entry) {
    struct fat_boot_sector boot_sector;
    if (readsector(fd, (unsigned char *)&boot_sector, 0) == -1) {
        fprintf(stderr, "Failed to read boot sector\n");
        return -1;
    }

    unsigned int root_start_cluster = boot_sector.fat32.root_cluster;
    unsigned int cluster_offset = get_cluster_offset(root_start_cluster, &boot_sector);

    unsigned char cluster[CLUSTERSIZE];
    if (lseek(fd, cluster_offset, SEEK_SET) == -1) {
        perror("lseek");
        return -1;
    }
    if (read(fd, cluster, CLUSTERSIZE) != CLUSTERSIZE) {
        perror("read");
        return -1;
    }

    struct msdos_dir_entry *current_entry = (struct msdos_dir_entry *)cluster;
    while (current_entry->name[0] != 0x00) {
        if (strncmp((char *)current_entry->name, filename, MSDOS_NAME) == 0) {
            if (sizeof(*dir_entry) >= sizeof(*current_entry)) {
                memcpy(dir_entry, current_entry, sizeof(struct msdos_dir_entry));
                return 0;
            } else {
                fprintf(stderr, "Buffer overflow detected\n");
                return -1;
            }
        }
        current_entry++;
    }

    return -1;
}

unsigned int get_cluster_offset(int cluster, struct fat_boot_sector *boot_sector) {
    return (boot_sector->reserved + boot_sector->fats * boot_sector->fat32.length + (cluster - 2) * boot_sector->sec_per_clus) * SECTORSIZE;
}

int get_next_cluster(int fd, unsigned int cluster, struct fat_boot_sector *boot_sector) {
    unsigned int fat_offset = boot_sector->reserved * SECTORSIZE + cluster * 4;
    unsigned char fat[SECTORSIZE];
    if (readsector(fd, fat, fat_offset / SECTORSIZE) == -1) {
        fprintf(stderr, "Failed to read FAT sector\n");
        return 0xFFFFFFFF;
    }
    return *((unsigned int *)(fat + (fat_offset % SECTORSIZE))) & 0x0FFFFFFF;
}

int is_empty_segment(unsigned char *buf, int size) {
    for (int i = 0; i < size; i++) {
        if (buf[i] != 0) {
            return 0; // Segment is not empty
        }
    }
    return 1; // Segment is empty
}
