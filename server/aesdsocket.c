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
#include <sys/time.h>   // For select()
#include <sys/stat.h>   // For mkdir()
#include <errno.h>      // For error codes like EEXIST
#include <pthread.h>    // NEW for multi-threading
#include <time.h>       // NEW for timestamp and clock_gettime
#include <sys/queue.h>  // NEW for linked-list (optional, but helpful)

#define PORT "9000"
#define BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10

int server_fd = -1, client_fd = -1;
volatile sig_atomic_t stop = 0;

pthread_mutex_t file_mutex; // Protects read/write to /var/tmp/aesdsocketdata

// Keep track of active threads using a singly-linked list:
typedef struct thread_list_node {
    pthread_t thread_id;
    SLIST_ENTRY(thread_list_node) entries;
} thread_list_node_t;

// Create the list head
SLIST_HEAD(slisthead, thread_list_node) head = SLIST_HEAD_INITIALIZER(head);

// Structure to pass parameters to each client thread
typedef struct client_params {
    int thread_client_fd;                 // The connection-specific socket fd
    struct sockaddr_storage client_addr;  // The client's address
} client_params_t;

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
    // For testing purposes, we have commented this out so that the file persists for validation.
    // if (stop) {
    //     remove(DATA_FILE);
    //     syslog(LOG_INFO, "Removed file %s", DATA_FILE);
    // }

    syslog(LOG_INFO, "Cleaned up and exiting");
    closelog();
}

// Function to set up the server socket using getaddrinfo
int setup_server_socket() {
    struct addrinfo hints, *servinfo, *p;
    int status;
    int yes = 1;  // For setting socket options (SO_REUSEADDR)

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo error: %s", gai_strerror(status));
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_fd == -1) {
            continue;
        }
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            syslog(LOG_ERR, "setsockopt error");
            close(server_fd);
            return -1;
        }
        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            syslog(LOG_ERR, "Binding failed");
            close(server_fd);
            continue;
        }
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

// Helper function to append a timestamp to the data file
void append_timestamp(void) {
    char time_str[100];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", timeinfo);

    pthread_mutex_lock(&file_mutex);
    FILE *fp = fopen(DATA_FILE, "a");
    if (fp) {
        fputs(time_str, fp);
        fclose(fp);
    } else {
        syslog(LOG_ERR, "Failed to open file for timestamp append");
    }
    pthread_mutex_unlock(&file_mutex);
}

// Timer thread to append timestamps every 10 seconds using a polling loop on CLOCK_MONOTONIC
void* timer_thread_func(void* arg) {
    (void)arg; // unused
    struct timespec start, now;
    // Get the current monotonic time as the baseline
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Append a timestamp immediately
    append_timestamp();

    while (!stop) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        // Check if at least 10 seconds have elapsed
        if ((now.tv_sec - start.tv_sec) >= 10) {
            append_timestamp();
            // Reset start to current time after appending a timestamp
            start = now;
        } else {
            // Sleep for 100 milliseconds to avoid busy-waiting
            usleep(100000);
        }
    }
    pthread_exit(NULL);
    return NULL;
}

// Thread function to handle each client's connection
void* client_thread_func(void* arg) {
    client_params_t* params = (client_params_t*)arg;
    int local_fd = params->thread_client_fd;
    char client_ip[INET6_ADDRSTRLEN];

    // Convert client address to string for logging
    if (params->client_addr.ss_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)&params->client_addr;
        inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof client_ip);
    } else {
        struct sockaddr_in6* s = (struct sockaddr_in6*)&params->client_addr;
        inet_ntop(AF_INET6, &s->sin6_addr, client_ip, sizeof client_ip);
    }
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int newline_found = 0;
    // Read data from the client
    while ((bytes_read = recv(local_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        syslog(LOG_INFO, "Received %zd bytes from %s", bytes_read, client_ip);
        // Write to the file under mutex
        pthread_mutex_lock(&file_mutex);
        FILE* data_file_ptr = fopen(DATA_FILE, "a");
        if (!data_file_ptr) {
            syslog(LOG_ERR, "Failed to open file for appending: %s", DATA_FILE);
            pthread_mutex_unlock(&file_mutex);
            break;
        }
        size_t written = fwrite(buffer, 1, bytes_read, data_file_ptr);
        fflush(data_file_ptr); // Flush explicitly
        if (written < bytes_read) {
            syslog(LOG_ERR, "Incomplete write: wrote %zu of %zd bytes", written, bytes_read);
        }
        fclose(data_file_ptr);
        pthread_mutex_unlock(&file_mutex);

        // Check if a newline is present in this received chunk
        if (strchr(buffer, '\n')) {
            newline_found = 1;
        }
        // Continue reading until connection is closed to ensure full data is received
    }
    
    // After the connection is closed, if a newline was ever received,
    // send the entire file back to the client.
    if (newline_found) {
        pthread_mutex_lock(&file_mutex);
        FILE* data_file_ptr = fopen(DATA_FILE, "r");
        if (!data_file_ptr) {
            syslog(LOG_ERR, "Failed to open file for reading: %s", DATA_FILE);
            pthread_mutex_unlock(&file_mutex);
        } else {
            while (fgets(buffer, BUFFER_SIZE, data_file_ptr) != NULL) {
                send(local_fd, buffer, strlen(buffer), 0);
            }
            fclose(data_file_ptr);
            pthread_mutex_unlock(&file_mutex);
        }
    }

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    close(local_fd);
    free(params);
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char* argv[]) {
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
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

    // Initialize the file_mutex
    pthread_mutex_init(&file_mutex, NULL);

    // Create the timer thread for timestamp appending
    pthread_t timer_tid;
    pthread_create(&timer_tid, NULL, timer_thread_func, NULL);

    while (!stop) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        // Timeout of 1 second
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (ret == -1) {
            syslog(LOG_ERR, "select error");
            break;
        } else if (ret == 0) {
            if (stop) break;
            continue;
        }

        // Accept a client connection
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            syslog(LOG_ERR, "Accept failed");
            exit(EXIT_FAILURE);
        }

        client_params_t* cparams = (client_params_t*)malloc(sizeof(client_params_t));
        if (!cparams) {
            syslog(LOG_ERR, "Malloc failed for client_params");
            close(client_fd);
            client_fd = -1;
            continue;
        }
        cparams->thread_client_fd = client_fd;
        memcpy(&cparams->client_addr, &client_addr, sizeof(client_addr));

        pthread_t client_tid;
        if (pthread_create(&client_tid, NULL, client_thread_func, cparams) != 0) {
            syslog(LOG_ERR, "Failed to create client thread");
            free(cparams);
            close(client_fd);
            client_fd = -1;
            continue;
        }

        thread_list_node_t* node = malloc(sizeof(thread_list_node_t));
        if (!node) {
            syslog(LOG_ERR, "Malloc failed for thread_list_node");
        } else {
            node->thread_id = client_tid;
            SLIST_INSERT_HEAD(&head, node, entries);
        }

        client_fd = -1;
    }

    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }

    pthread_join(timer_tid, NULL);

    thread_list_node_t* curr = SLIST_FIRST(&head);
    while (curr != NULL) {
        thread_list_node_t* tmp = SLIST_NEXT(curr, entries);
        pthread_join(curr->thread_id, NULL);
        free(curr);
        curr = tmp;
    }

    pthread_mutex_destroy(&file_mutex);
    clean_up();
    return 0;
}
