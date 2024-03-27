#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>

#define MAX_PROCESSES 4
#define MAX_PATH_LEN 512

struct message {
    long mtype;
    char srcPath[MAX_PATH_LEN];
    char destPath[MAX_PATH_LEN];
};

void copyFile(char *srcPath, char *destPath);
void traverseDirectory(char *srcDir, char *destDir, int qid);
void childProcess(int qid);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        return 1;
    }

    char *srcDir = argv[1];
    char *destDir = argv[2];

    struct stat st = {0};
    if (stat(destDir, &st) == -1) {
        mkdir(destDir, 0700);
    }

    // Create the message queue
    key_t key = ftok(".", 'q');
    int qid = msgget(key, IPC_CREAT | 0666);

    // Fork child processes and place them in a pool
    pid_t childPids[MAX_PROCESSES];
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            childProcess(qid);
            exit(0);
        } else if (pid < 0) {
            printf("Failed to fork child process\n");
            return 1;
        } else {
            childPids[i] = pid;
        }
    }

    traverseDirectory(srcDir, destDir, qid);

    // Send termination messages to child processes
    struct message msg;
    msg.mtype = 1;
    strcpy(msg.srcPath, "");
    strcpy(msg.destPath, "");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        msgsnd(qid, &msg, sizeof(struct message) - sizeof(long), 0);
    }

    // Wait for child processes to exit
    for (int i = 0; i < MAX_PROCESSES; i++) {
        waitpid(childPids[i], NULL, 0);
    }

    // Remove the message queue
    msgctl(qid, IPC_RMID, NULL);

    return 0;
}

void traverseDirectory(char *srcDir, char *destDir, int qid) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char srcPath[MAX_PATH_LEN];
    char destPath[MAX_PATH_LEN];

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
            traverseDirectory(srcPath, destPath, qid);
        } else {
            struct message msg;
            msg.mtype = 1;
            strcpy(msg.srcPath, srcPath);
            strcpy(msg.destPath, destPath);
            msgsnd(qid, &msg, sizeof(struct message) - sizeof(long), 0);
        }
    }

    closedir(dir);
}

void childProcess(int qid) {
    struct message msg;
    while (1) {
        if (msgrcv(qid, &msg, sizeof(struct message) - sizeof(long), 1, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }

        if (strlen(msg.srcPath) == 0 && strlen(msg.destPath) == 0) {
            break; // Termination message received
        }

        copyFile(msg.srcPath, msg.destPath);
    }
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
    printf("copiando %s (%ld bytes)\n", srcPath, fileStat.st_size);

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0)
        fwrite(buffer, 1, bytesRead, dest);

    fclose(src);
    fclose(dest);
}