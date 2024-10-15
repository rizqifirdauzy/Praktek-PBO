#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define FOLDER_DOCUMENT "DOKUMEN/" // Path ke file resource
#define CGI_PATH "CGI/cgi" // Path ke program CGI

void parse_request_line(
    char *request,
    char *method,
    char *uri,
    char *http_version,
    char *query_string,
    char *post_data) {
    char request_message[BUFFER_SIZE];
    char request_line[BUFFER_SIZE];
    char *words[3];

    // Baca baris pertama dari rangkaian data request
    strcpy(request_message, request);
    char *line = strtok(request_message, "\r\n");
    if (line == NULL) {
        fprintf(stderr, "Error: Invalid request line\n");
        return;
    }
    strcpy(request_line, line);

    // Pilah request line berdasarkan spasi
    int i = 0;
    char *token = strtok(request_line, " ");
    while (token != NULL && i < 3) {
        words[i++] = token;
        token = strtok(NULL, " ");
    }

    // Pastikan ada 3 komponen dalam request line
    if (i < 3) {
        fprintf(stderr, "Error: Request line tidak lengkap\n");
        return;
    }

    // kata 1 : Method, kata 2 : URI, kata 3 : versi HTTP
    strcpy(method, words[0]);
    strcpy(uri, words[1]);
    strcpy(http_version, words[2]);

    // Hapus tanda / pada URI
    if (uri[0] == '/') {
        memmove(uri, uri + 1, strlen(uri));
    }

    // Cek apakah ada query string
    char *query_start = strchr(uri, '?');
    if (query_start != NULL) {
        // Pisahkan query string dari URI
        *query_start = '\0';
        // Salin query string
        strcpy(query_string, query_start + 1);
    } else {
        // Tidak ada query string
        query_string[0] = '\0';
    }

    // Cek apakah ada body data
    char *body_start = strstr(request, "\r\n\r\n");
    if (body_start != NULL) {
        // Pindahkan pointer ke awal body data
        body_start += 4; // Melewati CRLF CRLF
        // Salin data POST dari body
        strcpy(post_data, body_start);
    } else {
        post_data[0] = '\0'; // Tidak ada body data
    }

    // Jika URI kosong, maka isi URI dengan resource default yaitu index.html
    if (strlen(uri) == 0) {
        strcpy(uri, "index.html");
    }
}

void get_mime_type(char *file, char *mime) {
    const char *dot = strrchr(file, '.');
    if (dot == NULL) {
        strcpy(mime, "text/html");
    } else if (strcmp(dot, ".html") == 0) {
        strcpy(mime, "text/html");
    } else if (strcmp(dot, ".css") == 0) {
        strcpy(mime, "text/css");
    } else if (strcmp(dot, ".js") == 0) {
        strcpy(mime, "application/javascript");
    } else if (strcmp(dot, ".jpg") == 0) {
        strcpy(mime, "image/jpeg");
    } else if (strcmp(dot, ".png") == 0) {
        strcpy(mime, "image/png");
    } else if (strcmp(dot, ".gif") == 0) {
        strcpy(mime, "image/gif");
    } else if (strcmp(dot, ".ico") == 0) {
        strcpy(mime, "image/x-icon");
    } else {
        strcpy(mime, "text/html");
    }
}

void handle_method(
    char **response,
    int *response_size,
    char *method,
    char *uri,
    char *http_version,
    char *query_string,
    char *post_data) {
    
    char fileURL[256];
    snprintf(fileURL, sizeof(fileURL), "%s%s", FOLDER_DOCUMENT, uri);

    FILE *file = fopen(fileURL, "rb");

    if (!file) {
        char not_found[BUFFER_SIZE];
        snprintf(not_found, sizeof(not_found),
                 "%s 404 Not Found\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %lu\r\n"
                 "\r\n"
                 "<h1>404 Not Found</h1>",
                 http_version,
                 strlen("<h1>404 Not Found</h1>"));

        *response = (char *)malloc(strlen(not_found) + 1);
        strcpy(*response, not_found);
        *response_size = strlen(not_found);
    } else {
        char *content;
        size_t content_length;

        const char *extension = strrchr(uri, '.');
        if (extension && strcmp(extension, ".php") == 0) {
            char command[BUFFER_SIZE];
            FILE *fp;

            snprintf(command, sizeof(command),
                "%s --target=%s --method=%s --data_query_string=\"%s\" --data_post=\"%s\"",
                CGI_PATH, fileURL, method, query_string, post_data);
            
            fp = popen(command, "r");
            if (fp == NULL) {
                perror("popen");
                exit(EXIT_FAILURE);
            }

            char cgi_output[BUFFER_SIZE];
            size_t output_len = 0;
            while (fgets(cgi_output + output_len, sizeof(cgi_output) - output_len, fp) != NULL) {
                output_len += strlen(cgi_output + output_len);
            }

            pclose(fp);

            char response_header[BUFFER_SIZE];
            snprintf(response_header, sizeof(response_header),
                     "%s 200 OK\r\n"
                     "Content-Type: text/html\r\n"
                     "Content-Length: %lu\r\n\r\n",
                     http_version, output_len);

            *response_size = strlen(response_header) + output_len;
            *response = (char *)malloc(*response_size + 1);
            strcpy(*response, response_header);
            strcat(*response, cgi_output);
        } else {
            char response_line[BUFFER_SIZE];
            char mimeType[32];
            get_mime_type(uri, mimeType);

            snprintf(response_line, sizeof(response_line),
                "%s 200 OK\r\n"
                "Content-Type: %s\r\n\r\n", http_version, mimeType);

            fseek(file, 0, SEEK_END);
            long fsize = ftell(file);
            fseek(file, 0, SEEK_SET);

            *response = (char *)malloc(fsize + strlen(response_line) + 1);
            strcpy(*response, response_line);
            *response_size = fsize + strlen(response_line);

            char *msg_body = *response + strlen(response_line);
            fread(msg_body, fsize, 1, file);
        }
        fclose(file);
    }
}

void handle_client(int sock_client) {
    char request[BUFFER_SIZE];
    char *response = NULL;
    int request_size;
    int response_size = 0;
    char method[16], uri[256], query_string[512], post_data[512], http_version[16];

    request_size = read(sock_client, request, BUFFER_SIZE - 1);
    if (request_size < 0) {
        perror("Proses baca dari client gagal");
        close(sock_client);
        return;
    }

    request[request_size] = '\0';
    printf("\n-----------------------------------------------\n");
    printf("Request dari browser:\r\n\r\n%s", request);
    parse_request_line(request, method, uri, http_version, query_string, post_data);
    handle_method(&response, &response_size, method, uri, http_version, query_string, post_data);
    printf("\n-----------------------------------------------\n");
    printf("Method: %s URI: %s\n", method, uri);
    printf("Response ke browser:\n%s\n", response);

    if (response != NULL) {
        write(sock_client, response, response_size);
        free(response);
    }
    close(sock_client);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket gagal dibuat");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind gagal");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen gagal");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server berjalan di port %d...\n", PORT);

    while (1) {
        client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Proses koneksi client gagal");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}