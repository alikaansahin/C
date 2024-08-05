#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/msdos_fs.h>

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 1024

void print_help() {
    printf("Usage: fatmod DISKIMAGE [OPTIONS]\\n");
    printf("Options:\\n");
    printf("  -l                  List files in root directory\\n");
    printf("  -r -a FILENAME      Read file in ASCII form\\n");
    printf("  -r -b FILENAME      Read file in binary form\\n");
    printf("  -c FILENAME         Create a new file\\n");
    printf("  -d FILENAME         Delete a file\\n");
    printf("  -w FILENAME OFFSET N DATA  Write DATA to file\\n");
    printf("  -h                  Display this help message\\n");
}

// Function to read a sector from the disk image
int read_sector(int fd, unsigned char *buf, unsigned int snum) {
    off_t offset = snum * SECTOR_SIZE;
    if (lseek(fd, offset, SEEK_SET) != offset) {
        perror("lseek");
        return -1;
    }
    if (read(fd, buf, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("read");
        return -1;
    }
    return 0;
}

// Function to write a sector to the disk image
int write_sector(int fd, unsigned char *buf, unsigned int snum) {
    off_t offset = snum * SECTOR_SIZE;
    if (lseek(fd, offset, SEEK_SET) != offset) {
        perror("lseek");
        return -1;
    }
    if (write(fd, buf, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("write");
        return -1;
    }
    fsync(fd);
    return 0;
}

// Function to read a cluster from the disk image
int read_cluster(int fd, unsigned char *buf, unsigned int cnum) {
    for (int i = 0; i < CLUSTER_SIZE / SECTOR_SIZE; i++) {
        if (read_sector(fd, buf + i * SECTOR_SIZE, cnum * (CLUSTER_SIZE / SECTOR_SIZE) + i) != 0) {
            return -1;
        }
    }
    return 0;
}

// Function to write a cluster to the disk image
int write_cluster(int fd, unsigned char *buf, unsigned int cnum) {
    for (int i = 0; i < CLUSTER_SIZE / SECTOR_SIZE; i++) {
        if (write_sector(fd, buf + i * SECTOR_SIZE, cnum * (CLUSTER_SIZE / SECTOR_SIZE) + i) != 0) {
            return -1;
        }
    }
    return 0;
}

// Function to list files in the root directory
void list_files(int fd) {
    struct msdos_dir_entry dir_entry;
    unsigned char buf[CLUSTER_SIZE];
    if (read_cluster(fd, buf, 2) != 0) {
        fprintf(stderr, "Error reading root directory cluster\\n");
        return;
    }
    for (int i = 0; i < CLUSTER_SIZE; i += sizeof(struct msdos_dir_entry)) {
        memcpy(&dir_entry, buf + i, sizeof(struct msdos_dir_entry));
        if (dir_entry.name[0] == 0x00) {
            break; // No more entries
        }
        if (dir_entry.name[0] != 0xe5) { // Skip deleted entries
            char name[9], ext[4];
            strncpy(name, (char*)dir_entry.name, 8);
            name[8] = '\\0';
            strncpy(ext, (char*)dir_entry.name + 8, 3);
            ext[3] = '\\0';
            printf("%s.%s %u\\n", name, ext, dir_entry.size);
        }
    }
}

// Function to find a file in the root directory
int find_file(int fd, const char *filename, struct msdos_dir_entry *dir_entry, int *entry_offset) {
    unsigned char buf[CLUSTER_SIZE];
    if (read_cluster(fd, buf, 2) != 0) {
        fprintf(stderr, "Error reading root directory cluster\\n");
        return -1;
    }
    for (int i = 0; i < CLUSTER_SIZE; i += sizeof(struct msdos_dir_entry)) {
        memcpy(dir_entry, buf + i, sizeof(struct msdos_dir_entry));
        if (dir_entry->name[0] == 0x00) {
            break; // No more entries
        }
        if (dir_entry->name[0] != 0xe5) { // Skip deleted entries
            char name[9], ext[4], full_name[13];
            strncpy(name, (char*)dir_entry->name, 8);
            name[8] = '\\0';
            strncpy(ext, (char*)dir_entry->name + 8, 3);
            ext[3] = '\\0';
            snprintf(full_name, sizeof(full_name), "%s.%s", name, ext);
            if (strcasecmp(filename, full_name) == 0) {
                *entry_offset = i;
                return 0;
            }
        }
    }
    return -1; // File not found
}

// Function to read file content in ASCII form
void read_file_ascii(int fd, const char *filename) {
    struct msdos_dir_entry dir_entry;
    int entry_offset;
    if (find_file(fd, filename, &dir_entry, &entry_offset) != 0) {
        fprintf(stderr, "File not found: %s\\n", filename);
        return;
    }
    unsigned char buf[CLUSTER_SIZE];
    unsigned int cluster = dir_entry.start;
    while (cluster < 0x0FFFFFF8) {
        if (read_cluster(fd, buf, cluster) != 0) {
            fprintf(stderr, "Error reading cluster %u\\n", cluster);
            return;
        }
        write(1, buf, CLUSTER_SIZE); // Write to stdout
        // Read next cluster from FAT table (simplified for this example)
        // In a real implementation, read the next cluster number from the FAT
        cluster++;
    }
}

// Function to read file content in binary form
void read_file_binary(int fd, const char *filename) {
    struct msdos_dir_entry dir_entry;
    int entry_offset;
    if (find_file(fd, filename, &dir_entry, &entry_offset) != 0) {
        fprintf(stderr, "File not found: %s\\n", filename);
        return;
    }
    unsigned char buf[CLUSTER_SIZE];
    unsigned int cluster = dir_entry.start;
    unsigned int offset = 0;
    while (cluster < 0x0FFFFFF8) {
        if (read_cluster(fd, buf, cluster) != 0) {
            fprintf(stderr, "Error reading cluster %u\\n", cluster);
            return;
        }
        for (int i = 0; i < CLUSTER_SIZE; i += 16) {
            printf("%08x: ", offset);
            for (int j = 0; j < 16; j++) {
                printf("%02x ", buf[i + j]);
            }
            printf("\\n");
            offset += 16;
        }
        // Read next cluster from FAT table (simplified for this example)
        // In a real implementation, read the next cluster number from the FAT
        cluster++;
    }
}

// Function to create a file
void create_file(int fd, const char *filename) {
    struct msdos_dir_entry dir_entry;
    int entry_offset;
    if (find_file(fd, filename, &dir_entry, &entry_offset) == 0) {
        fprintf(stderr, "File already exists: %s\\n", filename);
        return;
    }
    unsigned char buf[CLUSTER_SIZE];
    if (read_cluster(fd, buf, 2) != 0) {
        fprintf(stderr, "Error reading root directory cluster\\n");
        return;
    }
    for (int i = 0; i < CLUSTER_SIZE; i += sizeof(struct msdos_dir_entry)) {
        memcpy(&dir_entry, buf + i, sizeof(struct msdos_dir_entry));
        if (dir_entry.name[0] == 0x00 || dir_entry.name[0] == 0xe5) {
            memset(&dir_entry, 0, sizeof(struct msdos_dir_entry));
            strncpy((char*)dir_entry.name, filename, 8);
            dir_entry.attr = 0x20; // Archive attribute
            dir_entry.start = 0; // No clusters allocated yet
            dir_entry.size = 0; // Initial size 0
            memcpy(buf + i, &dir_entry, sizeof(struct msdos_dir_entry));
            if (write_cluster(fd, buf, 2) != 0) {
                fprintf(stderr, "Error writing root directory cluster\\n");
            }
            return;
        }
    }
    fprintf(stderr, "No free directory entries\\n");
}

// Function to delete a file
void delete_file(int fd, const char *filename) {
    struct msdos_dir_entry dir_entry;
    int entry_offset;
    if (find_file(fd, filename, &dir_entry, &entry_offset) != 0) {
        fprintf(stderr, "File not found: %s\\n", filename);
        return;
    }
    unsigned char buf[CLUSTER_SIZE];
    if (read_cluster(fd, buf, 2) != 0) {
        fprintf(stderr, "Error reading root directory cluster\\n");
        return;
    }
    buf[entry_offset] = 0xe5; // Mark entry as deleted
    if (write_cluster(fd, buf, 2) != 0) {
        fprintf(stderr, "Error writing root directory cluster\\n");
    }
}

// Function to write data to a file
void write_file(int fd, const char *filename, unsigned int offset, unsigned int n, unsigned char data) {
    struct msdos_dir_entry dir_entry;
    int entry_offset;
    if (find_file(fd, filename, &dir_entry, &entry_offset) != 0) {
        fprintf(stderr, "File not found: %s\\n", filename);
        return;
    }
    unsigned char buf[CLUSTER_SIZE];
    unsigned int cluster = dir_entry.start;
    unsigned int cluster_offset = offset % CLUSTER_SIZE;
    while (n > 0 && cluster < 0x0FFFFFF8) {
        if (read_cluster(fd, buf, cluster) != 0) {
            fprintf(stderr, "Error reading cluster %u\\n", cluster);
            return;
        }
        unsigned int write_size = CLUSTER_SIZE - cluster_offset;
        if (write_size > n) {
            write_size = n;
        }
        memset(buf + cluster_offset, data, write_size);
        if (write_cluster(fd, buf, cluster) != 0) {
            fprintf(stderr, "Error writing cluster %u\\n", cluster);
            return;
        }
        n -= write_size;
        cluster_offset = 0;
        // Read next cluster from FAT table (simplified for this example)
        // In a real implementation, read the next cluster number from the FAT
        cluster++;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    const char *diskimage = argv[1];
    int fd = open(diskimage, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        print_help();
    } else if (argc == 3 && strcmp(argv[2], "-l") == 0) {
        list_files(fd);
    } else if (argc == 4 && strcmp(argv[2], "-r") == 0 && strcmp(argv[3], "-a") == 0) {
        read_file_ascii(fd, argv[3]);
    } else if (argc == 4 && strcmp(argv[2], "-r") == 0 && strcmp(argv[3], "-b") == 0) {
        read_file_binary(fd, argv[3]);
    } else if (argc == 4 && strcmp(argv[2], "-c") == 0) {
        create_file(fd, argv[3]);
    } else if (argc == 4 && strcmp(argv[2], "-d") == 0) {
        delete_file(fd, argv[3]);
    } else if (argc == 6 && strcmp(argv[2], "-w") == 0) {
        unsigned int offset = atoi(argv[4]);
        unsigned int n = atoi(argv[5]);
        unsigned char data = (unsigned char) atoi(argv[6]);
        write_file(fd, argv[3], offset, n, data);
    } else {
        print_help();
    }
    close(fd);
    return 0;
}