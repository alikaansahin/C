#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/msdos_fs.h>
#include <errno.h>
#include <stdint-gcc.h>

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 1024
#define ROOTDIR_CLUSTERS 1

// Function prototypes
int read_sector(int fd, unsigned char *buf, uint32_t sector_number);
int write_sector(int fd, unsigned char *buf, uint32_t sector_number);
void list_files(const char *disk_image);
void read_file_ascii(const char *disk_image, const char *filename);
void read_file_binary(const char *disk_image, const char *filename);
void create_file(const char *disk_image, const char *filename);
void delete_file(const char *disk_image, const char *filename);
void write_data(const char *disk_image, const char *filename, uint32_t offset, uint32_t n, uint8_t data);
void print_help();
int find_file(const char *disk_image, const char *filename, struct msdos_dir_entry *dir_entry);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return EXIT_FAILURE;
    }

    const char *disk_image = argv[1];

    if (strcmp(argv[2], "-l") == 0) {
        list_files(disk_image);
    } else if (strcmp(argv[2], "-r") == 0) {
        if (argc < 5) {
            print_help();
            return EXIT_FAILURE;
        }

        if (strcmp(argv[3], "-a") == 0) {
            read_file_ascii(disk_image, argv[4]);
        } else if (strcmp(argv[3], "-b") == 0) {
            read_file_binary(disk_image, argv[4]);
        } else {
            print_help();
            return EXIT_FAILURE;
        }
    } else if (strcmp(argv[2], "-c") == 0) {
        if (argc < 4) {
            print_help();
            return EXIT_FAILURE;
        }
        create_file(disk_image, argv[3]);
    } else if (strcmp(argv[2], "-d") == 0) {
        if (argc < 4) {
            print_help();
            return EXIT_FAILURE;
        }
        delete_file(disk_image, argv[3]);
    } else if (strcmp(argv[2], "-w") == 0) {
        if (argc < 7) {
            print_help();
            return EXIT_FAILURE;
        }
        uint32_t offset = atoi(argv[4]);
        uint32_t n = atoi(argv[5]);
        uint8_t data = (uint8_t)atoi(argv[6]);
        write_data(disk_image, argv[3], offset, n, data);
    } else if (strcmp(argv[2], "-h") == 0) {
        print_help();
    } else {
        print_help();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int read_sector(int fd, unsigned char *buf, uint32_t sector_number) {
    off_t offset = sector_number * SECTOR_SIZE;
    if (lseek(fd, offset, SEEK_SET) == (off_t) -1) {
        perror("lseek");
        return -1;
    }
    if (read(fd, buf, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("read");
        return -1;
    }
    return 0;
}

int write_sector(int fd, unsigned char *buf, uint32_t sector_number) {
    off_t offset = sector_number * SECTOR_SIZE;
    if (lseek(fd, offset, SEEK_SET) == (off_t) -1) {
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

void list_files(const char *disk_image) {
    int fd = open(disk_image, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    unsigned char cluster[CLUSTER_SIZE];
    if (read_sector(fd, cluster, 32) == -1) {
        close(fd);
        return;
    }

    struct msdos_dir_entry *dir = (struct msdos_dir_entry *)cluster;
    for (size_t i = 0; i < CLUSTER_SIZE / sizeof(struct msdos_dir_entry); ++i, ++dir) {
        if (dir->name[0] == 0x00) break;
        if (dir->name[0] == 0xE5) continue;

        char name[9], ext[4];
        memset(name, 0, sizeof(name));
        memset(ext, 0, sizeof(ext));

        memcpy(name, dir->name, 8);
        memcpy(ext, dir->name + 8, 3);

        // Trim trailing spaces from name and extension
        for (int j = 7; j >= 0 && name[j] == ' '; j--) {
            name[j] = '\0';
        }
        for (int j = 2; j >= 0 && ext[j] == ' '; j--) {
            ext[j] = '\0';
        }

        if (ext[0] != '\0') {
            printf("%s.%s %u\n", name, ext, dir->size);
        } else {
            printf("%s %u\n", name, dir->size);
        }
    }

    close(fd);
}

void read_file_ascii(const char *disk_image, const char *filename) {
    int fd = open(disk_image, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(disk_image, filename, &dir_entry) == -1) {
        fprintf(stderr, "File not found: %s\n", filename);
        close(fd);
        return;
    }

    unsigned char cluster[CLUSTER_SIZE];
    uint32_t cluster_number = dir_entry.start;

    while (cluster_number < 0x0FFFFFF8) {
        if (read_sector(fd, cluster, 32 + cluster_number * 2) == -1) {
            close(fd);
            return;
        }

        for (size_t i = 0; i < CLUSTER_SIZE; ++i) {
            if (cluster[i] == '\0') {
                close(fd);
                return;
            }
            putchar(cluster[i]);
        }

        cluster_number = ((uint32_t *)cluster)[cluster_number];
    }

    close(fd);
}

void read_file_binary(const char *disk_image, const char *filename) {
    int fd = open(disk_image, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(disk_image, filename, &dir_entry) == -1) {
        fprintf(stderr, "File not found: %s\n", filename);
        close(fd);
        return;
    }

    unsigned char cluster[CLUSTER_SIZE];
    uint32_t cluster_number = dir_entry.start;
    uint32_t offset = 0;

    while (cluster_number < 0x0FFFFFF8) {
        if (read_sector(fd, cluster, 32 + cluster_number * 2) == -1) {
            close(fd);
            return;
        }

        for (size_t i = 0; i < CLUSTER_SIZE; ++i) {
            if (offset % 16 == 0) {
                printf("%08X: ", offset);
            }
            printf("%02X ", cluster[i]);
            if (++offset % 16 == 0) {
                printf("\n");
            }
        }

        cluster_number = ((uint32_t *)cluster)[cluster_number];
    }

    if (offset % 16 != 0) {
        printf("\n");
    }

    close(fd);
}

void create_file(const char *disk_image, const char *filename) {
    int fd = open(disk_image, O_RDWR);
    if (fd == -1) {
        perror("open");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(disk_image, filename, &dir_entry) != -1) {
        fprintf(stderr, "File already exists: %s\n", filename);
        close(fd);
        return;
    }

    unsigned char cluster[CLUSTER_SIZE];
    if (read_sector(fd, cluster, 32) == -1) {
        close(fd);
        return;
    }

    struct msdos_dir_entry *dir = (struct msdos_dir_entry *)cluster;
    for (size_t i = 0; i < CLUSTER_SIZE / sizeof(struct msdos_dir_entry); ++i, ++dir) {
        if (dir->name[0] == 0x00 || dir->name[0] == 0xE5) {
            memset(dir, 0, sizeof(struct msdos_dir_entry));
            strncpy((char *)dir->name, filename, 8);
            dir->attr = 0x20;
            dir->start = 0;
            dir->size = 0;
            break;
        }
    }

    if (write_sector(fd, cluster, 32) == -1) {
        close(fd);
        return;
    }

    close(fd);
}

void delete_file(const char *disk_image, const char *filename) {
    int fd = open(disk_image, O_RDWR);
    if (fd == -1) {
        perror("open");
        return;
    }

    unsigned char cluster[CLUSTER_SIZE];
    if (read_sector(fd, cluster, 32) == -1) {
        close(fd);
        return;
    }

    struct msdos_dir_entry *dir = (struct msdos_dir_entry *)cluster;
    for (size_t i = 0; i < CLUSTER_SIZE / sizeof(struct msdos_dir_entry); ++i, ++dir) {
        if (strncmp((char *)dir->name, filename, 8) == 0) {
            dir->name[0] = 0xE5;
            break;
        }
    }

    if (write_sector(fd, cluster, 32) == -1) {
        close(fd);
        return;
    }

    close(fd);
}

void write_data(const char *disk_image, const char *filename, uint32_t offset, uint32_t n, uint8_t data) {
    int fd = open(disk_image, O_RDWR);
    if (fd == -1) {
        perror("open");
        return;
    }

    struct msdos_dir_entry dir_entry;
    if (find_file(disk_image, filename, &dir_entry) == -1) {
        fprintf(stderr, "File not found: %s\n", filename);
        close(fd);
        return;
    }

    unsigned char cluster[CLUSTER_SIZE];
    uint32_t cluster_number = dir_entry.start;
    uint32_t cluster_offset = offset / CLUSTER_SIZE;
    uint32_t byte_offset = offset % CLUSTER_SIZE;

    while (cluster_number < 0x0FFFFFF8 && cluster_offset--) {
        cluster_number = ((uint32_t *)cluster)[cluster_number];
    }

    if (cluster_number >= 0x0FFFFFF8) {
        fprintf(stderr, "Offset out of bounds\n");
        close(fd);
        return;
    }

    if (read_sector(fd, cluster, 32 + cluster_number * 2) == -1) {
        close(fd);
        return;
    }

    for (uint32_t i = 0; i < n; ++i) {
        if (byte_offset == CLUSTER_SIZE) {
            if (write_sector(fd, cluster, 32 + cluster_number * 2) == -1) {
                close(fd);
                return;
            }

            cluster_number = ((uint32_t *)cluster)[cluster_number];
            if (cluster_number >= 0x0FFFFFF8) {
                fprintf(stderr, "Offset out of bounds\n");
                close(fd);
                return;
            }

            if (read_sector(fd, cluster, 32 + cluster_number * 2) == -1) {
                close(fd);
                return;
            }

            byte_offset = 0;
        }

        cluster[byte_offset++] = data;
    }

    if (write_sector(fd, cluster, 32 + cluster_number * 2) == -1) {
        close(fd);
        return;
    }

    close(fd);
}

void print_help() {
    printf("Usage: fatmod DISKIMAGE [OPTION] [PARAMETERS]\n");
    printf("Options:\n");
    printf("  -l                List files in root directory\n");
    printf("  -r -a FILENAME    Read file content as ASCII\n");
    printf("  -r -b FILENAME    Read file content as binary\n");
    printf("  -c FILENAME       Create a file\n");
    printf("  -d FILENAME       Delete a file\n");
    printf("  -w FILENAME OFFSET N DATA  Write data to file\n");
    printf("  -h                Display this help message\n");
}

int find_file(const char *disk_image, const char *filename, struct msdos_dir_entry *dir_entry) {
    int fd = open(disk_image, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    unsigned char cluster[CLUSTER_SIZE];
    if (read_sector(fd, cluster, 32) == -1) {
        close(fd);
        return -1;
    }

    struct msdos_dir_entry *dir = (struct msdos_dir_entry *)cluster;
    for (size_t i = 0; i < CLUSTER_SIZE / sizeof(struct msdos_dir_entry); ++i, ++dir) {
        if (strncmp((char *)dir->name, filename, 8) == 0) {
            memcpy(dir_entry, dir, sizeof(struct msdos_dir_entry));
            close(fd);
            return 0;
        }
    }

    close(fd);
    return -1;
}
