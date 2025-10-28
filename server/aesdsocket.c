#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 9000
#define BACKLOG 10
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

volatile sig_atomic_t signal_received = 0;

void signal_handler(int sig) {
    signal_received = 1;
    syslog(LOG_INFO, "Caught signal, exiting");
}

int main(int argc, char *argv[]) {
    int daemon_mode = 0;

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Remove the data file to start fresh
    unlink(DATA_FILE);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Failed to create socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Failed to bind socket");
        close(sockfd);
        return -1;
    }

    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Failed to fork");
            close(sockfd);
            return -1;
        }
        if (pid > 0) {
            // Parent exits
            exit(0);
        }
        // Child continues
        setsid();
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Failed to listen");
        close(sockfd);
        return -1;
    }

    while (!signal_received) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (clientfd == -1) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "Failed to accept connection");
            continue;
        }

        char *client_ip = inet_ntoa(client_addr.sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char buffer[BUFFER_SIZE];
        char *packet = NULL;
        size_t packet_size = 0;

        while (!signal_received) {
            ssize_t bytes_received = recv(clientfd, buffer, BUFFER_SIZE, 0);
            if (bytes_received == -1) {
                syslog(LOG_ERR, "Failed to receive data");
                break;
            }
            if (bytes_received == 0) {
                // Connection closed
                break;
            }

            // Append to packet
            packet = realloc(packet, packet_size + bytes_received);
            if (!packet) {
                syslog(LOG_ERR, "Failed to allocate memory");
                break;
            }
            memcpy(packet + packet_size, buffer, bytes_received);
            packet_size += bytes_received;

            // Check for newline
            char *newline = memchr(packet, '\n', packet_size);
            if (newline) {
                // Write up to newline
                size_t write_size = newline - packet + 1;
                int fd = open(DATA_FILE, O_CREAT | O_APPEND | O_WRONLY, 0644);
                if (fd == -1) {
                    syslog(LOG_ERR, "Failed to open data file for writing");
                    free(packet);
                    packet = NULL;
                    packet_size = 0;
                    break;
                }
                if (write(fd, packet, write_size) == -1) {
                    syslog(LOG_ERR, "Failed to write to data file");
                    close(fd);
                    free(packet);
                    packet = NULL;
                    packet_size = 0;
                    break;
                }
                close(fd);

                // Send full file content
                fd = open(DATA_FILE, O_RDONLY);
                if (fd == -1) {
                    syslog(LOG_ERR, "Failed to open data file for reading");
                    free(packet);
                    packet = NULL;
                    packet_size = 0;
                    break;
                }
                off_t file_size = lseek(fd, 0, SEEK_END);
                lseek(fd, 0, SEEK_SET);
                char *file_content = malloc(file_size);
                if (!file_content) {
                    syslog(LOG_ERR, "Failed to allocate memory for file content");
                    close(fd);
                    free(packet);
                    packet = NULL;
                    packet_size = 0;
                    break;
                }
                if (read(fd, file_content, file_size) != file_size) {
                    syslog(LOG_ERR, "Failed to read data file");
                    free(file_content);
                    close(fd);
                    free(packet);
                    packet = NULL;
                    packet_size = 0;
                    break;
                }
                close(fd);

                // Send to client
                if (send(clientfd, file_content, file_size, 0) == -1) {
                    syslog(LOG_ERR, "Failed to send data to client");
                }
                free(file_content);

                // Reset packet for next
                size_t remaining = packet_size - write_size;
                memmove(packet, packet + write_size, remaining);
                packet_size = remaining;
            }
        }

        free(packet);
        close(clientfd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    close(sockfd);
    unlink(DATA_FILE);
    closelog();
    return 0;
}