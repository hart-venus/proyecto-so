#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_PROCESSES 4

void copyFile(char *srcPath, char *destPath);
void traverseDirectory(char *srcDir, char *destDir);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        return 1;
    }

    char *srcDir = argv[1];
    char *destDir = argv[2];

    // manejar caso en el que ocupamos crear el directorio destino
    struct stat st = {0};
    if (stat(destDir, &st) == -1) {
        mkdir(destDir, 0700);
    }

    traverseDirectory(srcDir, destDir);

    return 0;
}

void traverseDirectory(char *srcDir, char *destDir) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char srcPath[512];
    char destPath[512];
    pid_t childPids[MAX_PROCESSES];
    int numChildren = 0;

    dir = opendir(srcDir);
    if (dir == NULL) {
        printf("Error opening directory %s\n", srcDir);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(srcPath, sizeof(srcPath), "%s/%s", srcDir, entry->d_name);
        snprintf(destPath, sizeof(destPath), "%s/%s", destDir, entry->d_name);

        if (stat(srcPath, &fileStat) == 0 && S_ISDIR(fileStat.st_mode)) {
            mkdir(destPath, fileStat.st_mode & 0777);
            traverseDirectory(srcPath, destPath);
        } else {
            while (numChildren == MAX_PROCESSES)
                wait(NULL);

            pid_t pid = fork();
            if (pid == 0) {
                copyFile(srcPath, destPath);
                exit(0);
            } else {
                childPids[numChildren++] = pid;
            }
        }
    }

    closedir(dir);

    while (numChildren > 0)
        wait(NULL);
}

void copyFile(char *srcPath, char *destPath) {
    FILE *src, *dest;
    char buffer[4096];
    size_t bytesRead;
    struct stat fileStat;

    src = fopen(srcPath, "rb");
    if (src == NULL) {
        printf("Error opening file %s\n", srcPath);
        return;
    }

    dest = fopen(destPath, "wb");
    if (dest == NULL) {
        printf("Error creating file %s\n", destPath);
        fclose(src);
        return;
    }

    stat(srcPath, &fileStat);
    printf("Copying %s (%ld bytes)\n", srcPath, fileStat.st_size);

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0)
        fwrite(buffer, 1, bytesRead, dest);

    fclose(src);
    fclose(dest);
}