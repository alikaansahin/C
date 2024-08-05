#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/msdos_fs.h>

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 1024
#define SECTORS_PER_CLUSTER (CLUSTER_SIZE / SECTOR_SIZE)
#define DIR_ENTRY_SIZE 32
#define ROOT_CLUSTER 2  // Assuming the root directory starts at cluster 2 for simplicity
#define DATA_START_SECTOR 33
#define ATTR_VOLUME_ID 0x08
#define ATTR_READ_ONLY 0x01
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)


// Function prototypes
void list_files(int fd, unsigned int root_cluster);
void read_file_ascii(int fd, unsigned int root_cluster, const char *filename);
void read_file_binary(int fd, unsigned int root_cluster, const char *filename);
void create_file(int fd, unsigned int root_cluster, const char *filename);
void delete_file(int fd, unsigned int root_cluster, const char *filename);
void write_file(int fd, unsigned int root_cluster, const char *filename, int offset, int numBytes, unsigned char data);
int open_disk_image(const char *path);
int read_sector(int fd, unsigned int sector_num, void *buffer);
int write_sector(int fd, unsigned int sector_num, const void *buffer);
void format_filename(const char *input, char *formatted);
int read_cluster(int fd, unsigned int cluster_num, void *buffer);
int write_cluster(int fd, unsigned int cluster_num, const void *buffer);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <diskImage> <command> [options]\n", argv[0]);
        return 1;
    }

    int fd = open_disk_image(argv[1]);  // Open the disk image file
    if (fd == -1) {
        perror("Failed to open disk image");
        return 1;
    }

    const char *command = argv[2];

    if (strcmp(command, "-l") == 0) {
        list_files(fd, ROOT_CLUSTER);
    } else if (strcmp(command, "-r") == 0 && argc > 4) {
        const char *mode = argv[3];
        const char *filename = argv[4];
        if (strcmp(mode, "-a") == 0) {
            read_file_ascii(fd, ROOT_CLUSTER, filename);
        } else if (strcmp(mode, "-b") == 0) {
            read_file_binary(fd, ROOT_CLUSTER, filename);
        } else {
            printf("Invalid read mode. Use -a for ASCII or -b for Binary.\n");
        }
    } else if (strcmp(command, "-c") == 0 && argc > 3) {
        const char *filename = argv[3];
        create_file(fd, ROOT_CLUSTER, filename);
    } else if (strcmp(command, "-d") == 0 && argc > 3) {
        const char *filename = argv[3];
        delete_file(fd, ROOT_CLUSTER, filename);
    } else if (strcmp(command, "-w") == 0 && argc > 6) {
        const char *filename = argv[3];
        int offset = atoi(argv[4]);
        int numBytes = atoi(argv[5]);
        unsigned char data = (unsigned char)atoi(argv[6]);
        write_file(fd, ROOT_CLUSTER, filename, offset, numBytes, data);
    } else {
        printf("Invalid command or insufficient arguments\n");
    }

    close(fd);  // Close the disk image file
    return 0;
}

int read_sector(int fd, unsigned int sector_num, void *buffer) {
    off_t offset = sector_num * SECTOR_SIZE;
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking in disk image");
        return -1;
    }
    if (read(fd, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("Error reading sector");
        return -1;
    }
    return 0;
}

int write_sector(int fd, unsigned int sector_num, const void *buffer) {
    off_t offset = sector_num * SECTOR_SIZE;
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking in disk image");
        return -1;
    }
    if (write(fd, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
        perror("Error writing sector");
        return -1;
    }
    fsync(fd);  // Ensure data is written to disk
    return 0;
}


void list_files(int fd, unsigned int root_cluster) {
    unsigned char buffer[CLUSTER_SIZE];
    struct msdos_dir_entry *de;

    // Read the root directory cluster
    if (read_cluster(fd, root_cluster, buffer) != 0) {
        printf("Failed to read root directory cluster\n");
        return;
    }

    // Parse directory entries
    de = (struct msdos_dir_entry *) buffer;
    for (int i = 0; i < CLUSTER_SIZE / DIR_ENTRY_SIZE; i++, de++) {
        // Check for free (unused) directory entry
        if (de->name[0] == 0x00) {
            break;  // No more entries
        }

        // Skip deleted entries and long name entries
        if (de->name[0] == 0xE5 || (de->attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
            continue;
        }

        // Skip volume label and other non-file entries
        if ((de->attr & ATTR_VOLUME_ID) || (de->attr & ATTR_DIRECTORY)) {
            continue;
        }

        // Convert space-padded name and extension into a more readable form
        char name[13];
        strncpy(name, (char *)de->name, 8);
        name[8] = '\0'; // Null-terminate the base part of the name
        // Trim trailing spaces
        char *end = name + strlen(name) - 1;
        while (end > name && *end == ' ') {
            *end = '\0';
            end--;
        }
        strcat(name, ".");
        strncat(name, (char *)de->name + 8, 3); // Append the extension
        // Trim trailing spaces from extension
        end = name + strlen(name) - 1;
        while (end > name && *end == ' ') {
            *end = '\0';
            end--;
        }

        printf("%s %lu\n", name, (unsigned long)de->size);
    }
}

void read_file_ascii(int fd, unsigned int root_cluster, const char *filename) {
    unsigned char buffer[CLUSTER_SIZE];
    struct msdos_dir_entry *de;
    int found = 0;

    if (read_cluster(fd, root_cluster, buffer) != 0) {
        printf("Failed to read root directory cluster\n");
        return;
    }

    de = (struct msdos_dir_entry *) buffer;
    for (int i = 0; i < CLUSTER_SIZE / DIR_ENTRY_SIZE; i++, de++) {
        char name[12];
        memcpy(name, de->name, 11);
        name[11] = '\0';
        if (strcmp(name, filename) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("File not found\n");
        return;
    }

    // Assuming file content fits into one cluster for simplicity
    if (read_cluster(fd, de->start, buffer) != 0) {
        printf("Failed to read file data\n");
        return;
    }

    int read_size = de->size < CLUSTER_SIZE ? de->size : CLUSTER_SIZE;
    for (int i = 0; i < read_size; i++) {
        printf("%c", buffer[i]);
    }
    printf("\n");
}



void read_file_binary(int fd, unsigned int root_cluster, const char *filename) {
    unsigned char buffer[CLUSTER_SIZE];
    struct msdos_dir_entry *de;
    int found = 0;

    // Read the root directory cluster to find the file's directory entry
    if (read_cluster(fd, root_cluster, buffer) != 0) {
        printf("Failed to read root directory cluster\n");
        return;
    }

    // Find the directory entry for the specified file
    de = (struct msdos_dir_entry *) buffer;
    for (int i = 0; i < CLUSTER_SIZE / DIR_ENTRY_SIZE; i++, de++) {
        char name[12];
        memcpy(name, de->name, 11);
        name[11] = '\0';  // Ensure null-termination
        if (strcmp(name, filename) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("File not found\n");
        return;
    }

    // Read the cluster containing the file's data
    if (read_cluster(fd, de->start, buffer) != 0) {
        printf("Failed to read file data\n");
        return;
    }

    // Output the file's data in hexadecimal format
    int read_size = de->size < CLUSTER_SIZE ? de->size : CLUSTER_SIZE;
    for (int i = 0; i < read_size; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (read_size % 16 != 0) printf("\n");  // Ensure ending a new line if the data doesn't exactly fill the last line
}



void create_file(int fd, unsigned int root_cluster, const char *filename) {
    unsigned char buffer[CLUSTER_SIZE];
    struct msdos_dir_entry *de;

    if (read_cluster(fd, root_cluster, buffer) != 0) {
        printf("Failed to read root directory cluster\n");
        return;
    }

    de = (struct msdos_dir_entry *) buffer;
    for (int i = 0; i < CLUSTER_SIZE / DIR_ENTRY_SIZE; i++, de++) {
        if (de->name[0] == 0x00 || de->name[0] == 0xE5) {  // Free entry found
            memset(de, 0, sizeof(struct msdos_dir_entry));
            strncpy((char *)de->name, filename, 11);
            de->size = 0;
            de->start = 0;  // No cluster allocated yet
            write_cluster(fd, root_cluster, buffer);
            printf("File created\n");
            return;
        }
    }
    printf("No space in directory\n");
}


void delete_file(int fd, unsigned int root_cluster, const char *filename) {
    unsigned char buffer[CLUSTER_SIZE];
    struct msdos_dir_entry *de;
    int found = 0;

    if (read_cluster(fd, root_cluster, buffer) != 0) {
        printf("Failed to read root directory cluster\n");
        return;
    }

    de = (struct msdos_dir_entry *) buffer;
    for (int i = 0; i < CLUSTER_SIZE / DIR_ENTRY_SIZE; i++, de++) {
        char name[12];
        memcpy(name, de->name, 11);
        name[11] = '\0';
        if (strcmp(name, filename) == 0) {
            de->name[0] = 0xE5;  // Mark as deleted
            write_cluster(fd, root_cluster, buffer);
            printf("File deleted\n");
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("File not found\n");
    }
}

void write_file(int fd, unsigned int root_cluster, const char *filename, int offset, int numBytes, unsigned char data) {
    unsigned char buffer[CLUSTER_SIZE];
    struct msdos_dir_entry *de;
    int found = 0;

    // Reading the directory cluster to find the file's directory entry
    if (read_cluster(fd, root_cluster, buffer) != 0) {
        printf("Failed to read root directory cluster\n");
        return;
    }

    de = (struct msdos_dir_entry *) buffer;
    for (int i = 0; i < CLUSTER_SIZE / DIR_ENTRY_SIZE; i++, de++) {
        char name[12];
        memcpy(name, de->name, 11);
        name[11] = '\0';
        if (strcmp(name, filename) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("File not found\n");
        return;
    }

    // Reading the file data cluster to modify its contents
    if (read_cluster(fd, de->start, buffer) != 0) {
        printf("Failed to read file data cluster\n");
        return;
    }

    // Check if the write operation extends the file size
    if (offset + numBytes > de->size) {
        if (offset + numBytes > CLUSTER_SIZE) {
            printf("Write exceeds cluster size\n");
            return;
        }
        de->size = offset + numBytes; // Update the file size
    }

    // Modify the buffer with the specified data
    memset(buffer + offset, data, numBytes);

    // Write the modified buffer back to the disk
    if (write_cluster(fd, de->start, buffer) != 0) {
        printf("Failed to write back to file data cluster\n");
    } else {
        printf("Data written successfully\n");
    }

    // Write the updated directory entry back to the root cluster
    if (write_cluster(fd, root_cluster, buffer) != 0) {
        printf("Failed to update directory entry\n");
    }
}


int read_cluster(int fd, unsigned int cluster_num, void *buffer) {
    unsigned int start_sector = DATA_START_SECTOR + (cluster_num - 2) * SECTORS_PER_CLUSTER;
    off_t offset = start_sector * SECTOR_SIZE;

    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking to cluster");
        return -1;
    }

    for (int i = 0; i < SECTORS_PER_CLUSTER; i++) {
        if (read(fd, buffer + (i * SECTOR_SIZE), SECTOR_SIZE) != SECTOR_SIZE) {
            perror("Error reading cluster");
            return -1;
        }
    }
    return 0;
}

int write_cluster(int fd, unsigned int cluster_num, const void *buffer) {
    unsigned int start_sector = DATA_START_SECTOR + (cluster_num - 2) * SECTORS_PER_CLUSTER;
    off_t offset = start_sector * SECTOR_SIZE;

    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking to cluster");
        return -1;
    }

    for (int i = 0; i < SECTORS_PER_CLUSTER; i++) {
        if (write(fd, buffer + (i * SECTOR_SIZE), SECTOR_SIZE) != SECTOR_SIZE) {
            perror("Error writing to cluster");
            return -1;
        }
    }
    fsync(fd);  // Ensure the data is flushed to disk
    return 0;
}

// Function to open the disk image file and return the file descriptor
int open_disk_image(const char *path) {
    int fd = open(path, O_RDWR);  // Open the file for read/write
    if (fd == -1) {
        perror("Error opening disk image");
    }
    return fd;
}
