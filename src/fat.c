#include <stddef.h>
#include "fat.h"
#include "ide.h" // See this file for function prototype of ata_lba_read()

#define SECTOR_SIZE 512

struct boot_sector *bs;
char bootSector[512]; // Allocate a global array to store boot sector
char fat_table[8*SECTOR_SIZE];
unsigned int root_sector;

int findLen(char *s) {
    int l = 0;

    while (s[l] && s[l] != ' ') l++;

    return l;
}

// Returns true is they're the same, otherwise false
int stringCompare(char str1[], char str2[]) {
    int str1Size = findLen(str1);
    int str2Size = findLen(str2);

    if (str1Size != str2Size) return false;

    for (int i = 0; i < str1Size; i++) {
        if (str1[i] != str2[i]) {
            return false;
        }
    }
    return true;
}

int fatInit() {
    if (ata_lba_read(2048, bootSector, 1) != 0){ // Read sector 0 from disk drive into bootSector array
        return -1;
    }
    bs = (struct boot_sector *)bootSector; // Point boot_sector struct to the boot sector so we can read fields
  
    // Validate the boot signature = 0xaa55
    if (bs->boot_signature != 0xaa55) {
        return -2;
    }

    // Validate fs_type = "FAT16" using string comparison
    const char CORRECT_FS_TYPE[] = "FAT16";
    if (!stringCompare(bs->fs_type, CORRECT_FS_TYPE)) {
        return -3;
    }

    // Read FAT table from the SD card into array fat_table
    if (ata_lba_read(2048 + bs->num_reserved_sectors, fat_table, 8) != 0) {
        return -4;
    }
    // Compute root_sector as:
    root_sector = 2048 + bs->num_fat_tables * bs->num_sectors_per_fat + bs->num_reserved_sectors + bs->num_hidden_sectors;

    return 0;
}

// helper for fatOpen
void extract_filename(struct root_directory_entry *rde, char *fname) {
    int out = 0;

    // Base name (up to 8 chars, stop at space padding)
    for (int i = 0; i < 8 && rde->file_name[i] != ' '; i++) {
        fname[out++] = rde->file_name[i];
    }

    // Check if extension exists (not all spaces)
    int has_ext = 0;
    for (int i = 0; i < 3; i++) {
        if (rde->file_extension[i] != ' ') { 
            has_ext = 1; 
            break; 
        }
    }

    // If there is an extension lets add it
    if (has_ext) {
        fname[out++] = '.';
        for (int i = 0; i < 3 && rde->file_extension[i] != ' '; i++) {
            fname[out++] = rde->file_extension[i];
        }
    }

    fname[out] = '\0';
}

// Helper for fatOpen
static inline char to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

// Case-insensitive compare | Helper for fatOpen
static int str_case_cmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = to_upper(*a);
        char cb = to_upper(*b);
        if (ca != cb) {
            return (unsigned char)ca - (unsigned char)cb;
        }
        a++; 
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}


struct file *fatOpen(const char *filename) {
    uint8_t buffer[SECTOR_SIZE];
    struct root_directory_entry *entry;

    // Compute root directory layout
    uint32_t root_dir_sectors =
        ((bs->num_root_dir_entries * 32) + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector;
    uint32_t root_dir_start = root_sector;

    // Convert target filename to uppercase (FAT stores in uppercase)
    char target[32];
    int i = 0;
    while (filename[i] && i < (int)sizeof(target) - 1) {
        target[i] = to_upper(filename[i]);
        i++;
    }
    target[i] = '\0';

    // Loop through root directory entries sector by sector
    for (uint32_t sector = 0; sector < root_dir_sectors; sector++) {
        ata_lba_read(root_dir_start + sector, buffer, 1);
        entry = (struct root_directory_entry *)buffer;

        for (int j = 0; j < bs->bytes_per_sector / sizeof(struct root_directory_entry); j++) {
            // Empty entry marks end
            if (entry[j].file_name[0] == 0x00)
                return NULL;

            // Skip deleted or non-file entries
            if (entry[j].file_name[0] == 0xE5)
                continue;
            if (entry[j].attribute & FILE_ATTRIBUTE_SUBDIRECTORY)
                continue;

            // Extract full filename from entry
            char extracted[16];
            extract_filename(&entry[j], extracted);

            // Compare filenames
            if (str_case_cmp(extracted, target) == 0) {
                static struct file f; 
                f.rde = entry[j];
                f.start_cluster = entry[j].cluster;
                f.next = NULL;
                f.prev = NULL;

                return &f;
            }
        }
    }

    return NULL;
}

int fatRead(struct file *f, uint8_t *buf, uint32_t len) {
    uint32_t bytes_read = 0;
    uint32_t cluster = f->start_cluster;

    // Calculate where data area starts
    uint32_t root_dir_sectors = ((bs->num_root_dir_entries * 32) + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector;
    uint32_t data_start = 2048 + bs->num_reserved_sectors + (bs->num_fat_tables * bs->num_sectors_per_fat) + root_dir_sectors;

    // Start reading the file, following the FAT chain
    while (cluster < 0xFFF8 && bytes_read < len) {
        // Compute LBA of this cluster
        uint32_t first_sector = data_start + (cluster - 2) * bs->num_sectors_per_cluster;

        // Read each sector in this cluster
        for (uint32_t s = 0; s < bs->num_sectors_per_cluster && bytes_read < len; s++) {
            ata_lba_read(first_sector + s, buf + bytes_read, 1);
            bytes_read += bs->bytes_per_sector;
        }

        // Follow FAT chain
        cluster = ((uint16_t*)fat_table)[cluster];
    }

    // If the buffer wasn't big enough, return the correct amount read
    return (bytes_read > len) ? len : bytes_read;
}

