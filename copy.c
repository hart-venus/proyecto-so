#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>

#define MAX_THREADS 4

typedef struct {
    char srcPath[512];
    char destPath[512];
} CopyTask;

typedef struct Node {
    CopyTask task;
    struct Node *next;
} Node;

Node *head = NULL;
Node *tail = NULL;
pthread_mutex_t lock;

void enqueue(CopyTask task);
CopyTask dequeue();
void *workerThread(void *arg);
void copyFile(char *srcPath, char *destPath);
void traverseDirectory(char *srcDir, char *destDir);
void cleanupQueue();

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        return 1;
    }

    pthread_mutex_init(&lock, NULL);
    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&threads[i], NULL, workerThread, NULL);
    }

    traverseDirectory(argv[1], argv[2]);

    for (int i = 0; i < MAX_THREADS; i++) {
        enqueue((CopyTask){ .srcPath = "", .destPath = "" });
    }

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    cleanupQueue();
    pthread_mutex_destroy(&lock);
    return 0;
}

void enqueue(CopyTask task) {
    pthread_mutex_lock(&lock);
    Node *newNode = malloc(sizeof(Node));
    newNode->task = task;
    newNode->next = NULL;
    if (tail == NULL) {
        head = tail = newNode;
    } else {
        tail->next = newNode;
        tail = newNode;
    }
    pthread_mutex_unlock(&lock);
}

CopyTask dequeue() {
    pthread_mutex_lock(&lock);
    if (head == NULL) {
        pthread_mutex_unlock(&lock);
        return (CopyTask){ .srcPath = "", .destPath = "" };
    }
    Node *temp = head;
    CopyTask task = temp->task;
    head = head->next;
    if (head == NULL) {
        tail = NULL;
    }
    free(temp);
    pthread_mutex_unlock(&lock);
    return task;
}

void *workerThread(void *arg) {
    while (1) {
        CopyTask task = dequeue();
        if (task.srcPath[0] == '\0' && task.destPath[0] == '\0') {
            break;
        }
        copyFile(task.srcPath, task.destPath);
    }
    return NULL;
}

void copyFile(char *srcPath, char *destPath) {
    FILE *src, *dest;
    char buffer[4096];
    size_t bytesRead;
    struct stat fileStat;
    char destDir[512];
    char *lastSlash;
    strcpy(destDir, destPath);
    lastSlash = strrchr(destDir, '/');
    if (lastSlash != NULL) {
        *lastSlash = '\0';  // Temporarily truncate the string to get the directory path
        struct stat st = {0};
        if (stat(destDir, &st) == -1) {
            mkdir(destDir, 0700);  // Create the directory if it doesn't exist
        }
    }

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

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytesRead, dest);
    }

    fclose(src);
    fclose(dest);
}


void traverseDirectory(char *srcDir, char *destDir) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char srcPath[512];
    char destPath[512];

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
            CopyTask task;
            strncpy(task.srcPath, srcPath, sizeof(task.srcPath));
            strncpy(task.destPath, destPath, sizeof(task.destPath));
            enqueue(task);
        }
    }

    closedir(dir);
}

void cleanupQueue() {
    while (head != NULL) {
        Node *temp = head;
        head = head->next;
        free(temp);
    }
}
