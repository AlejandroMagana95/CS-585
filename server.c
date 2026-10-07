#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define USERNAME_MAX 8
#define PASSWORD_MAX 5

typedef struct {
    int socket_fd;
    char username[USERNAME_MAX + 1];
    int active;
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
char server_password[PASSWORD_MAX + 1];
int port = 0;

void send_string(int fd, const char *str) {
    send(fd, str, strlen(str), 0);
}

void replace_emoticons(const char *src, char *dest, size_t dest_size) {
    dest[0] = '\0';
    size_t i = 0;
    while (src[i] != '\0' && strlen(dest) < dest_size - 1) {
        if (src[i] == ':' && src[i+1] == ')') {
            strncat(dest, "[feeling happy]", dest_size - strlen(dest) - 1);
            i += 2;
        } else if (src[i] == ':' && src[i+1] == '(') {
            strncat(dest, "[feeling sad]", dest_size - strlen(dest) - 1);
            i += 2;
        } else {
            size_t len = strlen(dest);
            if (len < dest_size - 1) {
                dest[len] = src[i];
                dest[len + 1] = '\0';
            }
            i++;
        }
    }
}

void get_formatted_time(int add_hour, char *out_buf, size_t buf_size) {
    time_t rawtime;
    time(&rawtime);
    if (add_hour) {
        rawtime += 3600;
    }
    struct tm *timeinfo = localtime(&rawtime);
    strftime(out_buf, buf_size, "%a %b %d %H:%M:%S %Y", timeinfo);
}

void broadcast_message(const char *msg, int sender_fd, int include_sender) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->active) {
            if (include_sender || clients[i]->socket_fd != sender_fd) {
                send_string(clients[i]->socket_fd, msg);
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void handle_users_command(int client_fd, const char *username) {
    char response[BUFFER_SIZE];
    char log_buf[BUFFER_SIZE + 64];
    
    strcpy(response, "Active Users: ");
    pthread_mutex_lock(&clients_mutex);
    int first = 1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->active) {
            if (!first) {
                strcat(response, ", ");
            }
            strcat(response, clients[i]->username);
            first = 0;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    strcat(response, "\n");
    
    send_string(client_fd, response);

    snprintf(log_buf, sizeof(log_buf), "%s: searched up active users\n", username);
    printf("%s", log_buf);
    fflush(stdout);
}

void handle_private_message(int sender_fd, const char *sender_user, const char *target_user, const char *msg) {
    char target_msg[BUFFER_SIZE + 64];
    char log_buf[BUFFER_SIZE + 64];
    int found = 0;

    snprintf(target_msg, sizeof(target_msg), "[Message from %s]: %s\n", sender_user, msg);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->active && strcmp(clients[i]->username, target_user) == 0) {
            send_string(clients[i]->socket_fd, target_msg);
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (found) {
        snprintf(log_buf, sizeof(log_buf), "%s: sent message to %s\n", sender_user, target_user);
        printf("%s", log_buf);
        fflush(stdout);
    } else {
        snprintf(target_msg, sizeof(target_msg), "User %s not found\n", target_user);
        send_string(sender_fd, target_msg);
    }
}

void process_line(client_t *cli, const char *line) {
    char log_buf[BUFFER_SIZE + 64];
    char out_buf[BUFFER_SIZE + 64];

    if (strcmp(line, ":Exit") == 0) {
        return;
    }

    if (strcmp(line, ":Users") == 0) {
        handle_users_command(cli->socket_fd, cli->username);
        return;
    }

    if (strncmp(line, ":Msg ", 5) == 0) {
        const char *p = line + 5;
        char target_user[USERNAME_MAX + 1];
        int idx = 0;
        while (*p != '\0' && *p != ' ' && idx < USERNAME_MAX) {
            target_user[idx++] = *p++;
        }
        target_user[idx] = '\0';
        if (*p == ' ') p++;
        handle_private_message(cli->socket_fd, cli->username, target_user, p);
        return;
    }

    if (strcmp(line, ":mytime") == 0 || strcmp(line, ":+1hr") == 0) {
        char time_str[128];
        int add_hr = (strcmp(line, ":+1hr") == 0) ? 1 : 0;
        get_formatted_time(add_hr, time_str, sizeof(time_str));

        snprintf(out_buf, sizeof(out_buf), "%s: %s\n", cli->username, time_str);
        printf("%s", out_buf);
        fflush(stdout);

        broadcast_message(out_buf, cli->socket_fd, 1);
        return;
    }

    char parsed_msg[BUFFER_SIZE];
    replace_emoticons(line, parsed_msg, sizeof(parsed_msg));

    snprintf(log_buf, sizeof(log_buf), "%s: %s\n", cli->username, parsed_msg);
    printf("%s", log_buf);
    fflush(stdout);

    broadcast_message(log_buf, cli->socket_fd, 0);
}

void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    char rx_buffer[BUFFER_SIZE];
    char stream_buffer[BUFFER_SIZE * 2];
    int stream_len = 0;
    stream_buffer[0] = '\0';

    char log_buf[BUFFER_SIZE + 64];
    snprintf(log_buf, sizeof(log_buf), "%s joined the chatroom\n", cli->username);
    printf("%s", log_buf);
    fflush(stdout);
    broadcast_message(log_buf, cli->socket_fd, 0);

    while (1) {
        int bytes = recv(cli->socket_fd, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (bytes <= 0) {
            break;
        }
        rx_buffer[bytes] = '\0';

        if (stream_len + bytes < (int)sizeof(stream_buffer) - 1) {
            strcat(stream_buffer, rx_buffer);
            stream_len += bytes;
        }

        char *newline_ptr;
        while ((newline_ptr = strchr(stream_buffer, '\n')) != NULL) {
            *newline_ptr = '\0';

            if (strcmp(stream_buffer, ":Exit") == 0) {
                goto client_cleanup;
            }

            process_line(cli, stream_buffer);

            char temp[BUFFER_SIZE * 2];
            strcpy(temp, newline_ptr + 1);
            strcpy(stream_buffer, temp);
            stream_len = strlen(stream_buffer);
        }
    }

client_cleanup:
    snprintf(log_buf, sizeof(log_buf), "%s left the chatroom\n", cli->username);
    printf("%s", log_buf);
    fflush(stdout);
    broadcast_message(log_buf, cli->socket_fd, 0);

    close(cli->socket_fd);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == cli) {
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    free(cli);
    return NULL;
}

int main(int argc, char *argv[]) {
    int opt;
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"password", required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p': port = atoi(optarg); break;
            case 'w': strncpy(server_password, optarg, PASSWORD_MAX); server_password[PASSWORD_MAX] = '\0'; break;
            default: exit(EXIT_FAILURE);
        }
    }

    if (port == 0 || strlen(server_password) == 0) {
        fprintf(stderr, "Usage: %s --port <port> --password <pass>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(port);

    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, 10);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

        pthread_mutex_lock(&clients_mutex);
        int count = 0;
        int free_slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] != NULL) {
                count++;
            } else if (free_slot == -1) {
                free_slot = i;
            }
        }

        if (count >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            send_string(client_fd, "Server full\n");
            close(client_fd);
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->socket_fd = client_fd;
        cli->active = 0;
        clients[free_slot] = cli;
        pthread_mutex_unlock(&clients_mutex);

        char auth_buf[BUFFER_SIZE];
        int bytes = recv(client_fd, auth_buf, sizeof(auth_buf) - 1, 0);
        if (bytes <= 0) {
            close(client_fd);
            pthread_mutex_lock(&clients_mutex);
            clients[free_slot] = NULL;
            pthread_mutex_unlock(&clients_mutex);
            free(cli);
            continue;
        }
        auth_buf[bytes] = '\0';

        char user[USERNAME_MAX + 1], pass[PASSWORD_MAX + 1];
        if (sscanf(auth_buf, "%8s %5s", user, pass) == 2) {
            if (strcmp(pass, server_password) == 0) {
                strncpy(cli->username, user, USERNAME_MAX);
                cli->username[USERNAME_MAX] = '\0';
                cli->active = 1;

                send_string(client_fd, "OK\n");

                pthread_t tid;
                pthread_create(&tid, NULL, handle_client, (void *)cli);
                pthread_detach(tid);
            } else {
                send_string(client_fd, "Incorrect password\n");
                close(client_fd);
                pthread_mutex_lock(&clients_mutex);
                clients[free_slot] = NULL;
                pthread_mutex_unlock(&clients_mutex);
                free(cli);
            }
        } else {
            send_string(client_fd, "Invalid format\n");
            close(client_fd);
            pthread_mutex_lock(&clients_mutex);
            clients[free_slot] = NULL;
            pthread_mutex_unlock(&clients_mutex);
            free(cli);
        }
    }

    close(server_fd);
    return 0;
}