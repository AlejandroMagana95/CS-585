#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

// Hilo para recibir e imprimir mensajes provenientes del servidor
void *receive_messages(void *arg) {
    int sockfd = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            break;
        }
        printf("%s", buffer);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    char host[256] = {0};
    int port = 0;
    char username[256] = {0};
    char password[256] = {0};

    static struct option long_options[] = {
        {"host",     required_argument, 0, 'h'},
        {"port",     required_argument, 0, 'p'},
        {"username", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h': strncpy(host, optarg, sizeof(host) - 1); break;
            case 'p': port = atoi(optarg); break;
            case 'u': strncpy(username, optarg, sizeof(username) - 1); break;
            case 'w': strncpy(password, optarg, sizeof(password) - 1); break;
            default: break;
        }
    }

    if (strlen(host) == 0 || port <= 0 || strlen(username) == 0 || strlen(password) == 0) {
        fprintf(stderr, "Uso: %s --host <host> --port <port> --username <user> --password <pass>\n", argv[0]);
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect failed");
        close(sockfd);
        return 1;
    }

    // Enviar credenciales
    char auth_payload[BUFFER_SIZE];
    snprintf(auth_payload, sizeof(auth_payload), "%s %s\n", username, password);
    send(sockfd, auth_payload, strlen(auth_payload), 0);

    printf("Connected to %s on port %d\n", host, port);
    fflush(stdout);

    // Crear hilo para recibir mensajes del servidor
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sockfd);
    pthread_detach(recv_thread);

    // Bucle para leer de stdin y enviar al servidor
    char input_buffer[BUFFER_SIZE];
    while (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
        send(sockfd, input_buffer, strlen(input_buffer), 0);
        
        // Quitar salto de línea para verificar si es exit
        input_buffer[strcspn(input_buffer, "\r\n")] = 0;
        if (strcmp(input_buffer, "exit") == 0) {
            break;
        }
    }

    close(sockfd);
    return 0;
}