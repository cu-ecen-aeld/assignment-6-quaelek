#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 9000
#define DATA_BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

volatile sig_atomic_t stop_server = 0;
pthread_mutex_t lock;

typedef struct {
    int sockfd;
    struct sockaddr_in cli_addr;
} thread_args_t;

void signal_handler(int sig) {
    stop_server = 1;
}

void clean_up(int sockfd, FILE *fp) {
    if (sockfd >= 0) {
        close(sockfd);
    }
    if (fp) {
        fclose(fp);
        remove(DATA_FILE);
    }
    pthread_mutex_destroy(&lock);
    closelog();
}

void send_file_contents(int sockfd, FILE *fp) {
    char buffer[DATA_BUFFER_SIZE];
    size_t bytes_read;

    rewind(fp);
    while ((bytes_read = fread(buffer, 1, DATA_BUFFER_SIZE, fp)) > 0) {
        if (write(sockfd, buffer, bytes_read) < 0) {
            syslog(LOG_ERR, "Error sending file contents to socket: %s", strerror(errno));
            break;
        }
    }
}

void* handle_connection(void* args) {
    thread_args_t* connection = (thread_args_t*)args;
    int sockfd = connection->sockfd;
    char buffer[DATA_BUFFER_SIZE];
    ssize_t read_size;
    FILE *fp;

    syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(connection->cli_addr.sin_addr));

    pthread_mutex_lock(&lock);
    fp = fopen(DATA_FILE, "a+");
    pthread_mutex_unlock(&lock);

    if (fp == NULL) {
        syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
        close(sockfd);
        free(args);
        return NULL;
    }

    do {
        memset(buffer, 0, DATA_BUFFER_SIZE);
        read_size = read(sockfd, buffer, DATA_BUFFER_SIZE - 1);
        if (read_size <= 0) {
            syslog(LOG_ERR, "Error reading from socket: %s", strerror(errno));
            break;
        }

        pthread_mutex_lock(&lock);
        fwrite(buffer, sizeof(char), read_size, fp);
        fflush(fp);
        pthread_mutex_unlock(&lock);
    } while (buffer[read_size - 1] != '\n');

    send_file_contents(sockfd, fp);

    fclose(fp);
    close(sockfd);
    syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(connection->cli_addr.sin_addr));
    free(args);
    return NULL;
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    int daemon_mode = 0;
    pthread_t thread_id;

    // Argument parsing for daemon mode
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    pthread_mutex_init(&lock, NULL);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "Error opening socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        syslog(LOG_ERR, "Error on binding: %s", strerror(errno));
        clean_up(sockfd, NULL);
        exit(EXIT_FAILURE);
    }

    if (daemon_mode) {
        if (fork() > 0) exit(EXIT_SUCCESS);
        setsid();
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (!stop_server) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "Error on accept: %s", strerror(errno));
            break;
        }

        thread_args_t* args = malloc(sizeof(thread_args_t));
        if (!args) {
            close(newsockfd);
            continue;
        }

        args->sockfd = newsockfd;
        args->cli_addr = cli_addr;

        if (pthread_create(&thread_id, NULL, &handle_connection, (void*)args) != 0) {
            syslog(LOG_ERR, "Could not create thread: %s", strerror(errno));
            free(args);
            close(newsockfd);
        }
        // Detach the thread to handle its cleanup automatically
        pthread_detach(thread_id);
    }

    clean_up(sockfd, NULL);
    syslog(LOG_INFO, "Daemon exiting, received signal");
    return 0;
}