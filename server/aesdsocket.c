#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/time.h>  // For select()
#include <sys/stat.h>  // For mkdir()
#include <errno.h>     // For error codes like EEXIST

#define PORT "9000"
#define BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10

int server_fd = -1, client_fd = -1;
volatile sig_atomic_t stop = 0;

// Signal handler to catch SIGINT and SIGTERM
void handle_signal(int signo) {
    syslog(LOG_INFO, "Caught signal, exiting");
    stop = 1;
}

// Function to clean up resources when shutting down
void clean_up() {
    if (client_fd != -1) close(client_fd);
    if (server_fd != -1) close(server_fd);

    // Remove the file only if a signal was received
    if (stop) {
        remove(DATA_FILE);
        syslog(LOG_INFO, "Removed file %s", DATA_FILE);
    }

    syslog(LOG_INFO, "Cleaned up and exiting");
    closelog();
}

// Function to set up the server socket using getaddrinfo
int setup_server_socket() {
    struct addrinfo hints, * servinfo, * p;
    int status;
    int yes = 1;  // For setting socket options (SO_REUSEADDR)

    // Set up hints for getaddrinfo
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Get server info
    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo error: %s", gai_strerror(status));
        return -1;
    }

    // Loop through all results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_fd == -1) {
            continue;
        }

        // Set the SO_REUSEADDR option to avoid "Address already in use" error
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            syslog(LOG_ERR, "setsockopt error");
            close(server_fd);
            return -1;
        }

        // Bind to the port
        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            syslog(LOG_ERR, "Binding failed");
            close(server_fd);
            continue;
        }

        // Successfully bound the socket
        break;
    }

    if (p == NULL) {
        syslog(LOG_ERR, "Failed to bind socket");
        freeaddrinfo(servinfo);
        return -1;
    }

    freeaddrinfo(servinfo);

    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listening failed");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

// Daemonize the process
void daemonize() {
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char* argv[]) {
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    FILE* data_file;
    char client_ip[INET6_ADDRSTRLEN];
    bool daemon_mode = false;
    struct timeval tv;
    fd_set readfds;

    // Open syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Ensure /var/tmp exists
    if (mkdir("/var/tmp", 0777) == -1 && errno != EEXIST) {
        syslog(LOG_ERR, "Failed to create /var/tmp directory");
        exit(EXIT_FAILURE);
    }

    // Remove the file before each run to ensure it's cleared
    remove(DATA_FILE);
    syslog(LOG_INFO, "Removed file %s before starting", DATA_FILE);

    // Check for the -d argument to run in daemon mode
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    }

    // Set up signal handling for graceful exit
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Setup server socket using getaddrinfo
    if (setup_server_socket() == -1) {
        syslog(LOG_ERR, "Failed to set up server socket");
        exit(EXIT_FAILURE);
    }

    // Run the program as a daemon if -d flag is provided
    if (daemon_mode) {
        daemonize();
        syslog(LOG_INFO, "Running in daemon mode");
    }

    while (!stop) {
        // Use select() to add a timeout to the accept() call
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        // Timeout of 1 second
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (ret == -1) {
            syslog(LOG_ERR, "select error");
            break;
        }
        else if (ret == 0) {
            // Timeout occurred, check stop flag and continue
            if (stop) break;
            continue;
        }

        // Accept a client connection
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            syslog(LOG_ERR, "Accept failed");
            exit(EXIT_FAILURE);
        }

        // Get the client IP address
        if (client_addr.ss_family == AF_INET) {
            struct sockaddr_in* s = (struct sockaddr_in*)&client_addr;
            inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof client_ip);
        }
        else {
            struct sockaddr_in6* s = (struct sockaddr_in6*)&client_addr;
            inet_ntop(AF_INET6, &s->sin6_addr, client_ip, sizeof client_ip);
        }

        // Log accepted connection
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Open the file for appending data
        data_file = fopen(DATA_FILE, "a");
        if (data_file == NULL) {
            syslog(LOG_ERR, "Failed to open file for appending: %s", DATA_FILE);
            exit(EXIT_FAILURE);
        }

        // Read data from the client and write to the file
        while ((bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, bytes_read, data_file);

            // If newline is found, send back the contents of the file
            if (strchr(buffer, '\n')) {
                fclose(data_file);  // Close the file after writing
                data_file = fopen(DATA_FILE, "r");  // Reopen the file for reading
                if (data_file == NULL) {
                    syslog(LOG_ERR, "Failed to open file for reading: %s", DATA_FILE);
                    exit(EXIT_FAILURE);
                }

                // Send file contents back to the client
                while (fgets(buffer, BUFFER_SIZE, data_file) != NULL) {
                    send(client_fd, buffer, strlen(buffer), 0);
                }

                fclose(data_file);  // Close after reading
                break;  // Process the next client connection
            }
        }

        // Log closed connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(client_fd);
        client_fd = -1;
    }

    // Cleanup on exit
    clean_up();
    return 0;
}
