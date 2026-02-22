#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "fetcher.h"

#define BUFFER_SIZE 4096

char* fetch_html(const char *hostname, const char *port, const char *path, size_t *out_size) {
    char request[1024];

    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: Mozilla/5.0 (X11; Linux x86_64) C-Browser/1.0\r\n"
             "Accept: text/html, */*\r\n"
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

    int is_https = (strcmp(port, "443") == 0);
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;

    if (is_https) {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();

        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            perror("ssl context failed");
            close(sockfd);
            return NULL;
        }

        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sockfd);
        SSL_set_tlsext_host_name(ssl, hostname);

        if (SSL_connect(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sockfd);
            return NULL;
        }
    }

    printf("sending http request...\n");
    if (is_https) {
        if (SSL_write(ssl, request, strlen(request)) <= 0) {
            perror("ssl write failed");
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sockfd);
            return NULL;
        }
    } else {
        if (send(sockfd, request, strlen(request), 0) == -1) {
            perror("error sending request");
            close(sockfd);
            return NULL;
        }
    }

    printf("receiving data...\n");

    size_t total_size = 0;
    size_t capacity = 8192;
    char *response = malloc(capacity);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while (1) {
        if (is_https) {
            bytes_received = SSL_read(ssl, buffer, BUFFER_SIZE);
        } else {
            bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
        }

        if (bytes_received <= 0) {
            break;
        }

        if (total_size + bytes_received + 1 > capacity) {
            capacity *= 2;
            response = realloc(response, capacity);
        }
        memcpy(response + total_size, buffer, bytes_received);
        total_size += bytes_received;
    }

    if (is_https) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
    }
    close(sockfd);

    if (bytes_received < 0) {
        perror("error receiving data");
        free(response);
        return NULL;
    }

    response[total_size] = '\0';
    *out_size = total_size;

    return response;
}
