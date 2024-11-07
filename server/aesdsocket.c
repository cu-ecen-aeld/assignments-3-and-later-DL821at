#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT 9000
#define BACKLOG 10
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_fd = -1, client_fd = -1;
FILE *file_ptr = NULL;

void cleanup() {
    if (file_ptr) {
        fclose(file_ptr);
        file_ptr = NULL;
    }
    if (client_fd != -1) {
        close(client_fd);
        client_fd = -1;
    }
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
    remove(FILE_PATH);
    closelog();
}

void signal_handler(int signal) {
    syslog(LOG_INFO, "Caught signal, exiting");
    cleanup();
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    int daemon_mode = 0;
    struct sockaddr_in server_addr, client_addr;
    char buffer[1024];
    ssize_t bytes_read;

    // Check if daemon mode is requested
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting server on port %d", PORT);

    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Open socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Daemonize after successful binding
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Error forking for daemon mode");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            exit(EXIT_SUCCESS); // Parent exits
        }
        // Child process continues as daemon
        if (setsid() == -1) {
            perror("Error creating new session");
            exit(EXIT_FAILURE);
        }

        // Redirect standard I/O to /dev/null for daemon mode
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_RDWR);
    }

    // Listen for incoming connections
    if (listen(server_fd, BACKLOG) == -1) {
        perror("Error listening on socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Accept connections in a loop
    while (1) {
        socklen_t addr_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd == -1) {
            perror("Error accepting connection");
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        // Open file for appending
        file_ptr = fopen(FILE_PATH, "a+");
        if (!file_ptr) {
            perror("Error opening file");
            close(client_fd);
            client_fd = -1;
            continue;
        }

        // Receive data and write to file
        while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes_read] = '\0';
            fputs(buffer, file_ptr);
            fflush(file_ptr);

            if (strchr(buffer, '\n')) {
                rewind(file_ptr);
                while (fgets(buffer, sizeof(buffer), file_ptr) != NULL) {
                    send(client_fd, buffer, strlen(buffer), 0);
                }
            }
        }

        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));

        fclose(file_ptr);
        file_ptr = NULL;
        close(client_fd);
        client_fd = -1;
    }

    cleanup();
    return 0;
}
