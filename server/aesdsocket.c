/**
 * @file aesdsocket.c
 * @brief Socket server for the AESD socket application.
 *
 * This application listens on port 9000 and reads/writes data.
 * For Assignment 8, if the build switch USE_AESD_CHAR_DEVICE is defined
 * (set to 1 by default), then all I/O is redirected to /dev/aesdchar,
 * timestamp printing is removed, and the driver endpoint is preserved.
 *
 * Author: Your Name Here
 */

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
 #include <sys/time.h>
 #include <sys/stat.h>
 #include <errno.h>
 #include <pthread.h>
 #include <time.h>
 #include <sys/queue.h>
 
 #define PORT "9000"
 #define BUFFER_SIZE 1024
 
 #ifdef USE_AESD_CHAR_DEVICE
 #define DATA_FILE "/dev/aesdchar"
 #else
 #define DATA_FILE "/var/tmp/aesdsocketdata"
 #endif
 
 #define BACKLOG 10
 
 int server_fd = -1, client_fd = -1;
 volatile sig_atomic_t stop = 0;
 
 pthread_mutex_t file_mutex; // Protects I/O to DATA_FILE
 
 /* Linked list structure to track threads */
 typedef struct thread_list_node {
     pthread_t thread_id;
     SLIST_ENTRY(thread_list_node) entries;
 } thread_list_node_t;
 
 SLIST_HEAD(slisthead, thread_list_node) head = SLIST_HEAD_INITIALIZER(head);
 
 /* Structure to pass parameters to client thread */
 typedef struct client_params {
     int thread_client_fd;
     struct sockaddr_storage client_addr;
 } client_params_t;
 
 /* Signal handler */
 void handle_signal(int signo) {
     syslog(LOG_INFO, "Caught signal, exiting");
     stop = 1;
 }
 
 /* Clean up resources */
 void clean_up() {
     if (client_fd != -1)
         close(client_fd);
     if (server_fd != -1)
         close(server_fd);
     syslog(LOG_INFO, "Cleaned up and exiting");
     closelog();
 }
 
 /* Set up the server socket */
 int setup_server_socket() {
     struct addrinfo hints, *servinfo, *p;
     int status;
     int yes = 1;
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
         if (server_fd == -1)
             continue;
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
 
 /* Daemonize the process */
 void daemonize() {
     pid_t pid;
     pid = fork();
     if (pid < 0) {
         syslog(LOG_ERR, "Fork failed");
         exit(EXIT_FAILURE);
     }
     if (pid > 0)
         exit(EXIT_SUCCESS);
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
     if (pid > 0)
         exit(EXIT_SUCCESS);
 }
 
 /* Note: Timestamp functionality is disabled when using the AESD char device */
 #ifndef USE_AESD_CHAR_DEVICE
 /* Append a timestamp to the file */
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
 /* Timer thread to append timestamps every 10 seconds */
 void* timer_thread_func(void* arg) {
     (void)arg;
     struct timespec start, now;
     clock_gettime(CLOCK_MONOTONIC, &start);
     append_timestamp();
     while (!stop) {
         clock_gettime(CLOCK_MONOTONIC, &now);
         if ((now.tv_sec - start.tv_sec) >= 10) {
             append_timestamp();
             start = now;
         } else {
             usleep(100000);
         }
     }
     pthread_exit(NULL);
     return NULL;
 }
 #endif
 
 /* Client thread function */
 void* client_thread_func(void* arg) {
     client_params_t* params = (client_params_t*)arg;
     int local_fd = params->thread_client_fd;
     char client_ip[INET6_ADDRSTRLEN];
 
     if (params->client_addr.ss_family == AF_INET) {
         struct sockaddr_in* s = (struct sockaddr_in*)&params->client_addr;
         inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof(client_ip));
     } else {
         struct sockaddr_in6* s = (struct sockaddr_in6*)&params->client_addr;
         inet_ntop(AF_INET6, &s->sin6_addr, client_ip, sizeof(client_ip));
     }
     syslog(LOG_INFO, "Accepted connection from %s", client_ip);
 
     char buffer[BUFFER_SIZE];
     ssize_t bytes_read;
     int newline_triggered = 0;
 
     while ((bytes_read = recv(local_fd, buffer, BUFFER_SIZE, 0)) > 0) {
         syslog(LOG_INFO, "Received %zd bytes from %s", bytes_read, client_ip);
         pthread_mutex_lock(&file_mutex);
         FILE* data_file_ptr = fopen(DATA_FILE, "a");
         if (!data_file_ptr) {
             syslog(LOG_ERR, "Failed to open file for appending: %s", DATA_FILE);
             pthread_mutex_unlock(&file_mutex);
             break;
         }
         fwrite(buffer, 1, bytes_read, data_file_ptr);
         fclose(data_file_ptr);
         pthread_mutex_unlock(&file_mutex);
 
         if (strchr(buffer, '\n')) {
             newline_triggered = 1;
             pthread_mutex_lock(&file_mutex);
             data_file_ptr = fopen(DATA_FILE, "r");
             if (!data_file_ptr) {
                 syslog(LOG_ERR, "Failed to open file for reading: %s", DATA_FILE);
                 pthread_mutex_unlock(&file_mutex);
                 break;
             }
             while (fgets(buffer, BUFFER_SIZE, data_file_ptr) != NULL) {
                 send(local_fd, buffer, strlen(buffer), 0);
             }
             fclose(data_file_ptr);
             pthread_mutex_unlock(&file_mutex);
             break;
         }
     }
     
     if (!newline_triggered && bytes_read == 0) {
         pthread_mutex_lock(&file_mutex);
         FILE* data_file_ptr = fopen(DATA_FILE, "r");
         if (data_file_ptr) {
             while (fgets(buffer, BUFFER_SIZE, data_file_ptr) != NULL) {
                 send(local_fd, buffer, strlen(buffer), 0);
             }
             fclose(data_file_ptr);
         } else {
             syslog(LOG_ERR, "Failed to open file for reading (post-loop): %s", DATA_FILE);
         }
         pthread_mutex_unlock(&file_mutex);
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
 
     openlog("aesdsocket", LOG_PID, LOG_USER);
 
     /* When using the driver, do not remove /dev/aesdchar. Otherwise, clear file */
 #ifndef USE_AESD_CHAR_DEVICE
     remove(DATA_FILE);
     syslog(LOG_INFO, "Removed file %s before starting", DATA_FILE);
 #endif
 
     if (argc > 1 && strcmp(argv[1], "-d") == 0) {
         daemon_mode = true;
     }
 
     signal(SIGINT, handle_signal);
     signal(SIGTERM, handle_signal);
 
     if (setup_server_socket() == -1) {
         syslog(LOG_ERR, "Failed to set up server socket");
         exit(EXIT_FAILURE);
     }
 
     if (daemon_mode) {
         daemonize();
         syslog(LOG_INFO, "Running in daemon mode");
     }
 
     pthread_mutex_init(&file_mutex, NULL);
 
 #ifndef USE_AESD_CHAR_DEVICE
     pthread_t timer_tid;
     pthread_create(&timer_tid, NULL, timer_thread_func, NULL);
 #endif
 
     while (!stop) {
         FD_ZERO(&readfds);
         FD_SET(server_fd, &readfds);
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
 
         client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
         if (client_fd < 0) {
             syslog(LOG_ERR, "Accept failed");
             exit(EXIT_FAILURE);
         }
 
         client_params_t* cparams = malloc(sizeof(client_params_t));
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
         if (node) {
             node->thread_id = client_tid;
             SLIST_INSERT_HEAD(&head, node, entries);
         } else {
             syslog(LOG_ERR, "Malloc failed for thread_list_node");
         }
 
         client_fd = -1;
     }
 
     if (server_fd != -1) {
         close(server_fd);
         server_fd = -1;
     }
 
 #ifndef USE_AESD_CHAR_DEVICE
     pthread_join(timer_tid, NULL);
 #endif
 
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
 