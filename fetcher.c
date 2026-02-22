#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include "fetcher.h"

#define BUFFER_SIZE 4096

char* fetch_html(const char *hostname, const char *port, const char *path, size_t *out_size) {
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n\r\n", path, hostname);

    int sockfd;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    printf("resolving %s...\n", hostname);
    if (getaddrinfo(hostname, port, &hints, &res) != 0) {
        perror("error resolving hostname");
        return NULL;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("error creating socket");
        freeaddrinfo(res);
        return NULL;
    }

    printf("connecting to server...\n");
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("error connecting");
        close(sockfd);
        freeaddrinfo(res);
        return NULL;
    }
    freeaddrinfo(res);

    printf("sending http request...\n");
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("error sending request");
        close(sockfd);
        return NULL;
    }

    printf("receiving data...\n");

    size_t total_size = 0;
    size_t capacity = 8192;
    char *response = malloc(capacity);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        // +1 ensures space for the null terminator
        if (total_size + bytes_received + 1 > capacity) {
            capacity *= 2;
            response = realloc(response, capacity);
        }
        memcpy(response + total_size, buffer, bytes_received);
        total_size += bytes_received;
    }

    close(sockfd);

    if (bytes_received == -1) {
        perror("error receiving data");
        free(response);
        return NULL;
    }

    // null-terminate the buffer for safe string processing
    response[total_size] = '\0';
    *out_size = total_size;

    return response;
}
