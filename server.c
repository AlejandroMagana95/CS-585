#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int socket_fd;
    int slot_index;
    char username[32];
    int active;
} client_t;

static client_t *clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static int active_clients_count = 0;
static char server_password[256] = {0};

void send_to_client(int socket_fd, const char *msg) {
    send(socket_fd, msg, strlen(msg), 0);
}

void broadcast_message(const char *msg, int sender_slot) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->active && i != sender_slot) {
            send_to_client(clients[i]->socket_fd, msg);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = recv(cli->socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            break; // Cliente desconectado
        }

        // Eliminar salto de línea si viene al final
        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strlen(buffer) == 0) continue;

        // Si el cliente envía exit
        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        // Formatear mensaje para broadcast: "username: message\n"
        char formatted_msg[BUFFER_SIZE + 64];
        snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s\n", cli->username, buffer);

        // Retransmitir a los demás clientes
        broadcast_message(formatted_msg, cli->slot_index);
    }

    // Notificación de salida
    char leave_msg[128];
    snprintf(leave_msg, sizeof(leave_msg), "%s left the chatroom\n", cli->username);
    
    // Imprimir en servidor
    printf("%s", leave_msg);
    fflush(stdout);

    // Notificar a los demás clientes
    broadcast_message(leave_msg, cli->slot_index);

    // Limpieza al desconectarse
    close(cli->socket_fd);
    pthread_mutex_lock(&clients_mutex);
    clients[cli->slot_index] = NULL;
    active_clients_count--;
    pthread_mutex_unlock(&clients_mutex);
    free(cli);

    return NULL;
}

int main(int argc, char *argv[]) {
    int port = 0;

    static struct option long_options[] = {
        {"port",     required_argument, 0, 'p'},
        {"password", required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'w':
                strncpy(server_password, optarg, sizeof(server_password) - 1);
                break;
            default:
                break;
        }
    }

    if (port <= 0 || strlen(server_password) == 0) {
        fprintf(stderr, "Uso: %s --port <port> --password <password>\n", argv[0]);
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }

    printf("Server started on port %d. Accepting connections\n", port);
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int newsockfd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (newsockfd < 0) continue;

        pthread_mutex_lock(&clients_mutex);
        if (active_clients_count >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            send_to_client(newsockfd, "Server full\n");
            close(newsockfd);
            continue;
        }

        int free_slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == NULL) {
                free_slot = i;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        char auth_buf[BUFFER_SIZE] = {0};
        int bytes = recv(newsockfd, auth_buf, sizeof(auth_buf) - 1, 0);
        if (bytes <= 0) {
            close(newsockfd);
            continue;
        }

        char user[64] = {0}, pass[64] = {0};
        sscanf(auth_buf, "%s %s", user, pass);

        if (strcmp(pass, server_password) != 0) {
            send_to_client(newsockfd, "Incorrect password\n");
            close(newsockfd);
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->socket_fd = newsockfd;
        cli->slot_index = free_slot;
        strncpy(cli->username, user, sizeof(cli->username) - 1);
        cli->active = 1;

        pthread_mutex_lock(&clients_mutex);
        clients[free_slot] = cli;
        active_clients_count++;
        pthread_mutex_unlock(&clients_mutex);

        char join_msg[128];
        snprintf(join_msg, sizeof(join_msg), "%s joined the chatroom\n", user);
        printf("%s", join_msg);
        fflush(stdout);

        broadcast_message(join_msg, free_slot);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)cli);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}