#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sqlite3.h>

#define PORT 8080
#define MAX_BUFFER 1024
#define DB_FILE "users.db"
#define MAX_SESSIONS 100

// Cấu trúc lưu thông tin phiên đăng nhập
typedef struct {
    int socket_fd;
    char username[50];
    char role[20];
} LoggedInUser;

LoggedInUser loggedInUsers[MAX_SESSIONS];

sqlite3 *db;

void init_database() {
    int rc = sqlite3_open(DB_FILE, &db);
    if (rc) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return;
    }

    char *sql = "CREATE TABLE IF NOT EXISTS users ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "username TEXT UNIQUE NOT NULL,"
                "password TEXT NOT NULL,"
                "role TEXT NOT NULL);"
                "CREATE TABLE IF NOT EXISTS exam_rooms ("
                "room_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "room_name TEXT NOT NULL,"
                "time_limit INTEGER NOT NULL,"
                "start_time DATETIME,"
                "end_time DATETIME);"
                "CREATE TABLE IF NOT EXISTS exam_participants ("
                "participant_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "room_id INTEGER,"
                "user_id INTEGER,"
                "score INTEGER DEFAULT 0,"
                "start_time DATETIME,"
                "FOREIGN KEY (room_id) REFERENCES exam_rooms(room_id),"
                "FOREIGN KEY (user_id) REFERENCES users(id));"
                "CREATE TABLE IF NOT EXISTS questions ("
                "question_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "question_text TEXT NOT NULL,"
                "difficulty INTEGER);"
                "CREATE TABLE IF NOT EXISTS choices ("
                "choice_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "question_id INTEGER NOT NULL,"
                "choice_text TEXT NOT NULL,"
                "is_correct BOOLEAN NOT NULL DEFAULT 0,"
                "FOREIGN KEY (question_id) REFERENCES questions(question_id));"
                "CREATE TABLE IF NOT EXISTS exam_answers ("
                "answer_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "room_id INTEGER,"
                "question_id INTEGER,"
                "user_id INTEGER,"
                "choice_id INTEGER,"
                "FOREIGN KEY (choice_id) REFERENCES choices(choices_id),"
                "FOREIGN KEY (room_id) REFERENCES exam_rooms(room_id),"
                "FOREIGN KEY (question_id) REFERENCES exam_questions(question_id),"
                "FOREIGN KEY (user_id) REFERENCES users(id));"
                ;
    
    char *err_msg = 0;
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
}

// Thêm session mới
int add_session(int socket_fd, const char *username, const char *role) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        // Tìm vị trí trống trong mảng (socket_fd = 0 là chưa được sử dụng)
        if (loggedInUsers[i].socket_fd == 0) {
            loggedInUsers[i].socket_fd = socket_fd;
            strncpy(loggedInUsers[i].username, username, sizeof(loggedInUsers[i].username) - 1);
            loggedInUsers[i].username[sizeof(loggedInUsers[i].username) - 1] = '\0';
            strncpy(loggedInUsers[i].role, role, sizeof(loggedInUsers[i].role) - 1);
            loggedInUsers[i].role[sizeof(loggedInUsers[i].role) - 1] = '\0';
            return 1; // Thêm thành công
        }
    }
    return 0; // Mảng đã đầy
}

// Tìm session theo socket
LoggedInUser get_session_by_socket(int socket_fd) {
    LoggedInUser emptyUser = {0}; // Tạo user rỗng để trả về khi không tìm thấy
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (loggedInUsers[i].socket_fd == socket_fd) {
            return loggedInUsers[i];  // Trả về bản sao của user
        }
    }
    return emptyUser;
}

// Xóa session
void remove_session(int socket_fd) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (loggedInUsers[i].socket_fd == socket_fd) {
            // Reset thông tin về 0/rỗng
            loggedInUsers[i].socket_fd = 0;
            memset(loggedInUsers[i].username, 0, sizeof(loggedInUsers[i].username));
            memset(loggedInUsers[i].role, 0, sizeof(loggedInUsers[i].role));
            break;
        }
    }
}

int validate_login(const char *username, const char *password, int socket_fd) {
    char *sql = "SELECT role FROM users WHERE username = ? AND password = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    const char *expanded_sql = sqlite3_expanded_sql(stmt);
    if (expanded_sql) {
        printf("Expanded query: %s\n", expanded_sql);
        sqlite3_free((void *)expanded_sql);
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* role = (const char*)sqlite3_column_text(stmt, 0);
        add_session(socket_fd, username, role);
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int register_user(const char *username, const char *password) {
    char *sql = "INSERT INTO users (username, password, role) VALUES (?, ?, 'student');";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

void init_logged_in_users() {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        loggedInUsers[i].socket_fd = 0;
        memset(loggedInUsers[i].username, 0, sizeof(loggedInUsers[i].username));
        memset(loggedInUsers[i].role, 0, sizeof(loggedInUsers[i].role));
    }
}

void send_exam_rooms(int socket_fd) {
    char *sql = "SELECT room_id, room_name, "
                "CASE "
                "  WHEN start_time > datetime('now') THEN 'Sắp diễn ra' "
                "  WHEN datetime('now') BETWEEN start_time AND end_time THEN 'Đang diễn ra' "
                "  ELSE 'Đã kết thúc' "
                "END as status, "
                "time_limit, "
                "start_time, "
                "end_time "
                "FROM exam_rooms;";
                
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Buffer cho response cuối cùng
    char response[MAX_BUFFER] = {0};
    // Thêm message header
    strcat(response, "EXAM_ROOMS_LIST:");
    
    char temp[256];
    int first = 1;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int room_id = sqlite3_column_int(stmt, 0);
        const char *room_name = (const char*)sqlite3_column_text(stmt, 1);
        const char *status = (const char*)sqlite3_column_text(stmt, 2);
        int time_limit = sqlite3_column_int(stmt, 3);
        const char *start_time = (const char*)sqlite3_column_text(stmt, 4);
        const char *end_time = (const char*)sqlite3_column_text(stmt, 5);

        // Thêm dấu | giữa các phòng, trừ phòng đầu tiên
        if (!first) {
            strcat(response, "|");
        }
        first = 0;

        // Format: room_id,room_name,status,time_limit,start_time,end_time
        snprintf(temp, sizeof(temp), "%d,%s,%s,%d,%s,%s", 
                room_id, room_name, status, time_limit, 
                start_time ? start_time : "N/A", 
                end_time ? end_time : "N/A");
        strcat(response, temp);
    }

    sqlite3_finalize(stmt);
    
    // Gửi response về client
    send(socket_fd, response, strlen(response), 0);
}

void handle_student_server(int socket_fd, char *buffer) {
    if (strncmp(buffer, "GET_EXAM_ROOMS", 14) == 0) {
        send_exam_rooms(socket_fd);
    }
}

void handle_teacher_server() {

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

    init_logged_in_users();
    // Khởi tạo database
    init_database();

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("New client connected\n");
        LoggedInUser currentSocketUser;

        while (1) {
            memset(buffer, 0, MAX_BUFFER);
            int valread = read(new_socket, buffer, MAX_BUFFER);
            if (valread <= 0) break;

            if (strncmp(buffer, "LOGIN|", 6) == 0) {
                char username[50], password[50], role[20];
                sscanf(buffer + 6, "%[^|]|%s", username, password);
                if (validate_login(username, password, new_socket)) {
                    char response[MAX_BUFFER];
                    currentSocketUser = get_session_by_socket(new_socket);
                    snprintf(response, sizeof(response), "SUCCESS:%s", currentSocketUser.role);
                    send(new_socket, response, strlen(response), 0);
                } else {
                    printf("Fail: %s %s", currentSocketUser.username, currentSocketUser.role);
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
            } else if (strcmp(currentSocketUser.role, "student") == 0) {
                handle_student_server(new_socket, buffer);
            } else if (strcmp(currentSocketUser.role, "teacher") == 0) {
                handle_teacher_server();
            }
            else {
                printf("Unknown message: %s\n", buffer);
            }
        }

        close(new_socket);
        printf("Client disconnected\n");
    }

    // Đóng database
    sqlite3_close(db);
    return 0;
}
