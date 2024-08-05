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
            char filename[13];
            strncpy(filename, (char *)dir_entry->name, 8);
            filename[8] = '.';
            strncpy(filename + 9, (char *)(dir_entry->name + 8), 3);
            filename[12] = '\0';
            printf("%s %d\n", filename, dir_entry->size);
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

void create_file(int fd, char *filename) {
    struct fat_boot_sector boot_sector;
    if (readsector(fd, (unsigned char *)&boot_sector, 0) == -1) {
        fprintf(stderr, "Failed to read boot sector\n");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(fd, filename, &dir_entry) == 0) {
        fprintf(stderr, "File already exists\n");
        return;
    }

    // Find free directory entry
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

    struct msdos_dir_entry *dir_entry_ptr = (struct msdos_dir_entry *)cluster;
    while (dir_entry_ptr->name[0] != 0x00 && dir_entry_ptr->name[0] != 0xe5) {
        dir_entry_ptr++;
    }

    // Create new directory entry
    memset(dir_entry_ptr, 0, sizeof(struct msdos_dir_entry));
    memcpy(dir_entry_ptr->name, filename, strlen(filename));
    dir_entry_ptr->size = 0;
    dir_entry_ptr->start = 0; // No blocks allocated yet

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
        if (strncmp((char *)dir_entry_ptr->name, filename, 8) == 0) {
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
	
	
        memcpy(buf + current_offset, &data, bytes_to_write);
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
        if (strncmp((char *)current_entry->name, filename, 8) == 0) {
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
