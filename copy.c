#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_PROCESSES 10
#define MSG_KEY 1234
#define FILE_MSG_TYPE 1
#define TERMINATE_MSG_TYPE 2

struct msg_buf {
    long mtype;
    char filename[256];
};

void copy_file(const char *src, const char *dst) {
    int in_fd, out_fd, n_chars;
    char buf[1024];

    // Open the source file
    in_fd = open(src, O_RDONLY);
    if (in_fd == -1) {
        perror("Opening source file");
        return;
    }

    // Open/create the destination file
    out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        perror("Opening destination file");
        close(in_fd);
        return;
    }

    // Copy the file
    while ((n_chars = read(in_fd, buf, sizeof(buf))) > 0) {
        if (write(out_fd, buf, n_chars) != n_chars) {
            perror("Writing to destination file");
            break;
        }
    }

    if (n_chars == -1) {
        perror("Reading from source file");
    }

    // Close the files
    if (close(in_fd) == -1) {
        perror("Closing source file");
    }

    if (close(out_fd) == -1) {
        perror("Closing destination file");
    }
}

void process_dir(const char *src_dir, const char *dst_dir, int msg_id) {
    DIR *dir;
    struct dirent *entry;
    char src_path[512], dst_path[512];
    struct stat st;

    dir = opendir(src_dir);
    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip current and parent directories
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        if (lstat(src_path, &st) == -1) {
            perror("Failed to get file status");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // It's a directory: create it in the destination and process it
            if (mkdir(dst_path, st.st_mode) == -1 && errno != EEXIST) {
                perror("Failed to create directory");
                continue;
            }
            process_dir(src_path, dst_path, msg_id);
        } else {
            // It's a file: send message to message queue
            struct msg_buf msg;
            msg.mtype = FILE_MSG_TYPE; // File message type
            strncpy(msg.filename, src_path, sizeof(msg.filename) - 1);
            msg.filename[sizeof(msg.filename) - 1] = '\0';

            if (msgsnd(msg_id, &msg, sizeof(msg.filename), 0) == -1) {
                perror("Failed to send message");
                exit(EXIT_FAILURE);
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s source_dir dest_dir\n", argv[0]);
        return 1;
    }

    char *src_dir = argv[1];
    char *dst_dir = argv[2];

    // Create the message queue
    int msg_id = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msg_id == -1) {
        perror("Failed to create message queue");
        exit(EXIT_FAILURE);
    }

    // Fork child processes
    pid_t pids[MAX_PROCESSES];
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) { // Child process
            struct msg_buf msg;
            while (1) {
                if (msgrcv(msg_id, &msg, sizeof(msg.filename), FILE_MSG_TYPE, 0) == -1) {
                    perror("Failed to receive message");
                    exit(EXIT_FAILURE);
                }

                if (msg.mtype == TERMINATE_MSG_TYPE) {
                    break; // Termination signal received
                }

                char dst_path[512];
                snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, strrchr(msg.filename, '/') + 1);
                copy_file(msg.filename, dst_path);
            }
            exit(EXIT_SUCCESS);
        }
    }

    // Process the directories
    process_dir(src_dir, dst_dir, msg_id);

    // Send termination messages
    for (int i = 0; i < MAX_PROCESSES; i++) {
        struct msg_buf msg;
        msg.mtype = TERMINATE_MSG_TYPE;
        msgsnd(msg_id, &msg, sizeof(msg.filename), 0); // No need to check for error here
    }

    // Wait for child processes to finish
    for (int i = 0; i < MAX_PROCESSES; i++) {
        waitpid(pids[i], NULL, 0);
    }

    // Clean up message queue
    msgctl(msg_id, IPC_RMID, NULL);

    return 0;
}
