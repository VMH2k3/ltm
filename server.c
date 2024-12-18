#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MAX_BUFFER 1024
#define USER_FILE "users.txt"

typedef struct {
    char username[50];
    char password[50];
    char role[20];
} User;

int validate_login(const char *username, const char *password, char *role) {
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) {
        return 0; // File not found, assume no users
    }

    char line[MAX_BUFFER];
    while (fgets(line, sizeof(line), file)) {
        User user;
        sscanf(line, "%[^,],%[^,],%s", user.username, user.password, user.role);
        if (strcmp(username, user.username) == 0 &&
            strcmp(password, user.password) == 0) {
            strcpy(role, user.role);
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0; // User not found
}

int register_user(const char *username, const char *password) {
    FILE *file = fopen(USER_FILE, "a");
    if (file == NULL) {
        return 0; // Unable to open file
    }

    // Check if username already exists
    rewind(file);
    char line[MAX_BUFFER];
    while (fgets(line, sizeof(line), file)) {
        User user;
        sscanf(line, "%[^,],%[^,],%s", user.username, user.password, user.role);
        if (strcmp(username, user.username) == 0) {
            fclose(file);
            return 0; // Username already exists
        }
    }

    // Add new user to the file
    fprintf(file, "%s,%s,student\n", username, password);
    fclose(file);
    return 1; // Registration successful
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[MAX_BUFFER] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d...\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("New client connected\n");

        while (1) {
            memset(buffer, 0, MAX_BUFFER);
            int valread = read(new_socket, buffer, MAX_BUFFER);
            if (valread <= 0) break;

            // Check if the message is a login or registration request
            if (strncmp(buffer, "LOGIN|", 6) == 0) {
                char username[50], password[50], role[20];
                sscanf(buffer + 6, "%[^|]|%s", username, password);
                if (validate_login(username, password, role)) {
                    char response[MAX_BUFFER];
                    snprintf(response, sizeof(response), "SUCCESS:%s", role);
                    send(new_socket, response, strlen(response), 0);
                } else {
                    send(new_socket, "FAIL:Invalid username or password", 32, 0);
                }
            } else if (strncmp(buffer, "REGISTER|", 9) == 0) {
                char username[50], password[50];
                sscanf(buffer + 9, "%[^|]|%s", username, password);
                if (register_user(username, password)) {
                    send(new_socket, "SUCCESS:Registration successful", 29, 0);
                } else {
                    send(new_socket, "FAIL:Username already exists", 26, 0);
                }
            } else {
                printf("Unknown message: %s\n", buffer);
            }
        }

        close(new_socket);
        printf("Client disconnected\n");
    }

    return 0;
}
