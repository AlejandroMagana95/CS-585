#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define USERNAME_MAX 8
#define PASSWORD_MAX 5

int port = 0;
char username[USERNAME_MAX + 1];
char password[PASSWORD_MAX + 1];

void *receive_messages(void *arg) {
    int sock_fd = *(int *)arg;
    char rx_buf[BUFFER_SIZE];

    while (1) {
        int bytes = recv(sock_fd, rx_buf, sizeof(rx_buf) - 1, 0);
        if (bytes <= 0) {
            break;
        }
        rx_buf[bytes] = '\0';
        printf("%s", rx_buf);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int opt;
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"username", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p': port = atoi(optarg); break;
            case 'u': strncpy(username, optarg, USERNAME_MAX); username[USERNAME_MAX] = '\0'; break;
            case 'w': strncpy(password, optarg, PASSWORD_MAX); password[PASSWORD_MAX] = '\0'; break;
            default: exit(EXIT_FAILURE);
        }
    }

    if (port == 0 || strlen(username) == 0 || strlen(password) == 0) {
        fprintf(stderr, "Usage: %s --port <port> --username <user> --password <pass>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(port);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }

    char auth_msg[BUFFER_SIZE];
    snprintf(auth_msg, sizeof(auth_msg), "%s %s\n", username, password);
    send(sock_fd, auth_msg, strlen(auth_msg), 0);

    char response[BUFFER_SIZE];
    int bytes = recv(sock_fd, response, sizeof(response) - 1, 0);
    if (bytes <= 0) {
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    response[bytes] = '\0';

    if (strcmp(response, "OK\n") != 0) {
        printf("%s", response);
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sock_fd);

    char send_buf[BUFFER_SIZE];
    while (fgets(send_buf, sizeof(send_buf), stdin) != NULL) {
        size_t len = strlen(send_buf);
        if (len > 0 && send_buf[len - 1] == '\n') {
            send_buf[len - 1] = '\0';
        }

        char msg_to_send[BUFFER_SIZE + 64];
        snprintf(msg_to_send, sizeof(msg_to_send), "%s\n", send_buf);
        send(sock_fd, msg_to_send, strlen(msg_to_send), 0);

        if (strcmp(send_buf, ":Exit") == 0) {
            break;
        }
    }

    close(sock_fd);
    return 0;
}