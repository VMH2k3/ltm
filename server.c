#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sqlite3.h>
#include <sys/select.h>

#define MAX_BUFFER 16384
#define DB_FILE "users.db"
#define MAX_SESSIONS 100

// Cấu trúc lưu thông tin phiên đăng nhập
typedef struct {
    int socket_fd;
    char username[50];
    char role[20];
    int user_id;
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
                "end_time ENDTIME,"
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
                "CREATE TABLE IF NOT EXISTS exam_questions ("
                "exam_question_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "room_id INTEGER NOT NULL,"
                "question_id INTEGER NOT NULL,"
                "FOREIGN KEY (room_id) REFERENCES exam_rooms(room_id),"
                "FOREIGN KEY (question_id) REFERENCES questions(question_id));"
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
    // Lấy user_id từ database
    char *get_user_sql = "SELECT id FROM users WHERE username = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, get_user_sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    
    int user_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (loggedInUsers[i].socket_fd == socket_fd) {
            loggedInUsers[i].user_id = user_id;  // Lưu user_id
            strncpy(loggedInUsers[i].username, username, sizeof(loggedInUsers[i].username) - 1);
            loggedInUsers[i].username[sizeof(loggedInUsers[i].username) - 1] = '\0';
            strncpy(loggedInUsers[i].role, role, sizeof(loggedInUsers[i].role) - 1);
            loggedInUsers[i].role[sizeof(loggedInUsers[i].role) - 1] = '\0';
            return 1;
        }
    }
    return 0;
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

void handle_join_exam(int socket_fd, int room_id) {
    // Kiểm tra xem phòng thi có tồn tại và đang diễn ra không
    char *check_room_sql = "SELECT room_id FROM exam_rooms "
                          "WHERE room_id = ? "
                          "AND datetime('now') BETWEEN start_time AND end_time;";
    
    sqlite3_stmt *check_stmt;
    int rc = sqlite3_prepare_v2(db, check_room_sql, -1, &check_stmt, 0);
    sqlite3_bind_int(check_stmt, 1, room_id);
    
    if (sqlite3_step(check_stmt) != SQLITE_ROW) {
        send(socket_fd, "ERROR:Phòng thi không tồn tại hoặc chưa bắt đầu", 48, 0);
        sqlite3_finalize(check_stmt);
        return;
    }
    sqlite3_finalize(check_stmt);

    // Lấy thông tin user từ session
    LoggedInUser user = get_session_by_socket(socket_fd);
    
    // Lấy user_id từ username
    char *get_user_id_sql = "SELECT id FROM users WHERE username = ?;";
    sqlite3_stmt *user_stmt;
    int user_id;
    
    rc = sqlite3_prepare_v2(db, get_user_id_sql, -1, &user_stmt, 0);
    sqlite3_bind_text(user_stmt, 1, user.username, -1, SQLITE_STATIC);
    
    if (sqlite3_step(user_stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(user_stmt, 0);
    } else {
        send(socket_fd, "ERROR:Không tìm thấy thông tin user", 35, 0);
        sqlite3_finalize(user_stmt);
        return;
    }
    sqlite3_finalize(user_stmt);

    // Kiểm tra xem user đã tham gia phòng thi này chưa
    char *check_participant_sql = "SELECT participant_id FROM exam_participants "
                                "WHERE room_id = ? AND user_id = ?;";
    sqlite3_stmt *part_stmt;
    rc = sqlite3_prepare_v2(db, check_participant_sql, -1, &part_stmt, 0);
    sqlite3_bind_int(part_stmt, 1, room_id);
    sqlite3_bind_int(part_stmt, 2, user_id);
    
    if (sqlite3_step(part_stmt) == SQLITE_ROW) {
        send(socket_fd, "ERROR:Bạn đã tham gia phòng thi này rồi", 40, 0);
        sqlite3_finalize(part_stmt);
        return;
    }
    sqlite3_finalize(part_stmt);

    // Thêm participant mới
    char *insert_participant_sql = "INSERT INTO exam_participants (room_id, user_id, start_time) "
                                 "VALUES (?, ?, datetime('now'));";
    sqlite3_stmt *insert_stmt;
    rc = sqlite3_prepare_v2(db, insert_participant_sql, -1, &insert_stmt, 0);
    sqlite3_bind_int(insert_stmt, 1, room_id);
    sqlite3_bind_int(insert_stmt, 2, user_id);
    
    if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
        send(socket_fd, "ERROR:Không thể tham gia phòng thi", 33, 0);
        sqlite3_finalize(insert_stmt);
        return;
    }
    sqlite3_finalize(insert_stmt);

    // Lấy danh sách câu hỏi cho phòng thi
    char *get_questions_sql = "SELECT q.question_id, q.question_text, "
                             "GROUP_CONCAT(c.choice_id || ';' || c.choice_text, ';') as choices "
                             "FROM exam_questions eq "
                             "JOIN questions q ON eq.question_id = q.question_id "
                             "JOIN choices c ON q.question_id = c.question_id "
                             "WHERE eq.room_id = ? "
                             "GROUP BY q.question_id, q.question_text;";
                            
    sqlite3_stmt *q_stmt;
    rc = sqlite3_prepare_v2(db, get_questions_sql, -1, &q_stmt, 0);
    sqlite3_bind_int(q_stmt, 1, room_id);
    
    char response[MAX_BUFFER] = "QUESTIONS:";
    char temp[256];
    int first = 1;
    
    while (sqlite3_step(q_stmt) == SQLITE_ROW) {
        int question_id = sqlite3_column_int(q_stmt, 0);
        const char *question_text = (const char*)sqlite3_column_text(q_stmt, 1);
        const char *choices = (const char*)sqlite3_column_text(q_stmt, 2);
        
        if (!first) {
            strcat(response, "|");
        }
        first = 0;
        
        snprintf(temp, sizeof(temp), "%d;%s;%s", 
                 question_id, question_text, choices);
        strcat(response, temp);
    }
    
    sqlite3_finalize(q_stmt);
    printf("%s", response);
    send(socket_fd, response, strlen(response), 0);
}

void handle_submit_exam(int socket_fd, char *request) {
    // Parse room_id từ request
    printf("%s\n", request);
    char *token = strtok(request, "|");
    int room_id = atoi(token);
    
    // Lấy user_id từ session
    LoggedInUser user = get_session_by_socket(socket_fd);
    
    char *check_participant_sql = "SELECT participant_id FROM exam_participants "
                                "WHERE room_id = ? AND user_id = ? "
                                "AND end_time IS NULL;";
    sqlite3_stmt *check_stmt;
    int rc = sqlite3_prepare_v2(db, check_participant_sql, -1, &check_stmt, 0);
    sqlite3_bind_int(check_stmt, 1, room_id);
    sqlite3_bind_int(check_stmt, 2, user.user_id);

    const char *expanded_sql = sqlite3_expanded_sql(check_stmt);
    if (expanded_sql) {
        printf("Expanded query: %s\n", expanded_sql);
        sqlite3_free((void *)expanded_sql);
    }
    
    if (sqlite3_step(check_stmt) != SQLITE_ROW) {
        send(socket_fd, "ERROR:Không tìm thấy thông tin bài thi", 38, 0);
        sqlite3_finalize(check_stmt);
        return;
    }
    int participant_id = sqlite3_column_int(check_stmt, 0);
    sqlite3_finalize(check_stmt);

    // Đếm số câu đúng
    int correct_count = 0;
    int total_questions = 0;

    // Kiểm tra từng câu trả lời
    token = strtok(NULL, "|");
    while (token != NULL) {
        int question_id, choice_id;
        sscanf(token, "%d,%d", &question_id, &choice_id);
        
        // Kiểm tra đáp án
        char *check_answer_sql = "SELECT 1 FROM questions q "
                               "JOIN choices c ON q.question_id = c.question_id "
                               "WHERE q.question_id = ? AND c.choice_id = ? "
                               "AND c.is_correct = 1;";
        sqlite3_stmt *ans_stmt;
        rc = sqlite3_prepare_v2(db, check_answer_sql, -1, &ans_stmt, 0);
        sqlite3_bind_int(ans_stmt, 1, question_id);
        sqlite3_bind_int(ans_stmt, 2, choice_id);
        
        const char *expanded_sql = sqlite3_expanded_sql(ans_stmt);
        if (expanded_sql) {
            printf("Expanded query: %s\n", expanded_sql);
            sqlite3_free((void *)expanded_sql);
        }

        if (sqlite3_step(ans_stmt) == SQLITE_ROW) {
            correct_count++;
        }
        total_questions++;
        token = strtok(NULL, "|");
    }

    // Cập nhật kết quả và thời gian kết thúc
    char *update_result_sql = "UPDATE exam_participants "
                            "SET score = ?, end_time = datetime('now') "
                            "WHERE participant_id = ?;";
    sqlite3_stmt *update_stmt;
    float score = (float)correct_count / total_questions * 10;
    
    rc = sqlite3_prepare_v2(db, update_result_sql, -1, &update_stmt, 0);
    sqlite3_bind_double(update_stmt, 1, score);
    sqlite3_bind_int(update_stmt, 2, participant_id);
    sqlite3_step(update_stmt);
    sqlite3_finalize(update_stmt);

    // Gửi kết quả về client
    char response[MAX_BUFFER];
    snprintf(response, sizeof(response), 
             "RESULT:Đúng %d/%d câu. Điểm của bạn: %.2f", 
             correct_count, total_questions, score);
    printf("Sending response: %s\n", response);
    ssize_t sent = send(socket_fd, response, strlen(response), 0);
    if (sent < 0) {
        perror("Error sending result");
    }
}

void handle_practice_questions(int socket_fd) {
    // Lấy danh sách câu hỏi ngẫu nhiên
    char *get_questions_sql = "WITH random_questions AS ("
                            "  SELECT question_id, question_text "
                            "  FROM questions "
                            "  ORDER BY RANDOM() "
                            "  LIMIT 20"
                            ") "
                            "SELECT rq.question_id, rq.question_text, "
                            "GROUP_CONCAT(c.choice_id || ';' || c.choice_text, ';') as choices "
                            "FROM random_questions rq "
                            "JOIN choices c ON rq.question_id = c.question_id "
                            "GROUP BY rq.question_id, rq.question_text;";
                            
    sqlite3_stmt *q_stmt;
    int rc = sqlite3_prepare_v2(db, get_questions_sql, -1, &q_stmt, 0);
    
    char response[MAX_BUFFER] = "QUESTIONS:";
    char temp[256];
    int first = 1;
    
    while (sqlite3_step(q_stmt) == SQLITE_ROW) {
        int question_id = sqlite3_column_int(q_stmt, 0);
        const char *question_text = (const char*)sqlite3_column_text(q_stmt, 1);
        const char *choices = (const char*)sqlite3_column_text(q_stmt, 2);
        
        if (!first) {
            strcat(response, "|");
        }
        first = 0;
        
        snprintf(temp, sizeof(temp), "%d;%s;%s", 
                 question_id, question_text, choices);
        strcat(response, temp);
    }
    const char *expanded_sql = sqlite3_expanded_sql(q_stmt);
    if (expanded_sql) {
        printf("Expanded query: %s\n", expanded_sql);
        sqlite3_free((void *)expanded_sql);
    }
    sqlite3_finalize(q_stmt);

    printf("response: %s\n", response);
    send(socket_fd, response, strlen(response), 0);
}

void handle_submit_practice(int socket_fd, char *request) {
    // Đếm số câu đúng
    int correct_count = 0;
    int total_questions = 0;

    // Kiểm tra từng câu trả lời
    char *token = strtok(request, "|");
    while (token != NULL) {
        int question_id, choice_id;
        sscanf(token, "%d,%d", &question_id, &choice_id);
        
        // Kiểm tra đáp án
        char *check_answer_sql = "SELECT 1 FROM questions q "
                               "JOIN choices c ON q.question_id = c.question_id "
                               "WHERE q.question_id = ? AND c.choice_id = ? "
                               "AND c.is_correct = 1;";
        sqlite3_stmt *ans_stmt;
        int rc = sqlite3_prepare_v2(db, check_answer_sql, -1, &ans_stmt, 0);
        
        if (rc != SQLITE_OK) {
            printf("SQL error: %s\n", sqlite3_errmsg(db));
            send(socket_fd, "ERROR:Lỗi kiểm tra đáp án", strlen("ERROR:Lỗi kiểm tra đáp án"), 0);
            return;
        }

        sqlite3_bind_int(ans_stmt, 1, question_id);
        sqlite3_bind_int(ans_stmt, 2, choice_id);

        if (sqlite3_step(ans_stmt) == SQLITE_ROW) {
            correct_count++;
        }
        total_questions++;
        
        sqlite3_finalize(ans_stmt);
        token = strtok(NULL, "|");
    }

    // Tính điểm và gửi kết quả về client
    float score = (float)correct_count / total_questions * 10;
    char response[MAX_BUFFER];
    snprintf(response, sizeof(response), 
             "RESULT:Kết quả luyện tập:\nĐúng %d/%d câu\nĐiểm: %.2f", 
             correct_count, total_questions, score);
             
    printf("Practice result: %s\n", response);
    send(socket_fd, response, strlen(response), 0);
}

void send_exam_history(int socket_fd) {
    // Lấy thông tin user từ session
    LoggedInUser user = get_session_by_socket(socket_fd);
    
    char *sql = "SELECT ep.score, ep.start_time, ep.end_time, er.room_name "
                "FROM exam_participants ep "
                "JOIN exam_rooms er ON ep.room_id = er.room_id "
                "WHERE ep.user_id = ? "
                "ORDER BY ep.start_time DESC;";
                
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, user.user_id);
    
    if (rc != SQLITE_OK) {
        send(socket_fd, "ERROR:Không thể lấy lịch sử thi", 31, 0);
        return;
    }

    char response[MAX_BUFFER] = "EXAM_HISTORY:";
    char temp[256];
    int first = 1;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        double score = sqlite3_column_double(stmt, 0);
        const char *start_time = (const char*)sqlite3_column_text(stmt, 1);
        const char *end_time = (const char*)sqlite3_column_text(stmt, 2);
        const char *room_name = (const char*)sqlite3_column_text(stmt, 3);
        
        if (!first) {
            strcat(response, "|");
        }
        first = 0;
        
        snprintf(temp, sizeof(temp), "%.2f,%s,%s,%s", 
                score, start_time, end_time, room_name);
        strcat(response, temp);
    }
    response[strlen(response)] = '\0';
    
    sqlite3_finalize(stmt);
    send(socket_fd, response, strlen(response), 0);
}

void handle_update_room_questions(int socket_fd, char *request) {
    int room_id;
    char *questions_data = strchr(request, '|');
    if (!questions_data) {
        send(socket_fd, "ERROR:Invalid request format", 27, 0);
        return;
    }

    // Bỏ qua ký tự '|'
    questions_data++;
    sscanf(request, "UPDATE_ROOM_QUESTIONS|%d", &room_id);

    // Bắt đầu transaction
    char *err_msg = 0;
    sqlite3_exec(db, "BEGIN TRANSACTION", 0, 0, &err_msg);

    // Xóa tất cả câu hỏi hiện tại của phòng thi
    char *delete_sql = "DELETE FROM exam_questions WHERE room_id = ?;";
    sqlite3_stmt *delete_stmt;
    int rc = sqlite3_prepare_v2(db, delete_sql, -1, &delete_stmt, 0);
    sqlite3_bind_int(delete_stmt, 1, room_id);

    if (sqlite3_step(delete_stmt) != SQLITE_DONE) {
        printf("Error deleting existing questions: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(delete_stmt);
        sqlite3_exec(db, "ROLLBACK", 0, 0, 0);
        send(socket_fd, "ERROR:Failed to delete existing questions", 39, 0);
        return;
    }
    sqlite3_finalize(delete_stmt);

    // Thêm các câu hỏi mới được chọn
    char *insert_sql = "INSERT INTO exam_questions (room_id, question_id) VALUES (?, ?);";
    sqlite3_stmt *insert_stmt;
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &insert_stmt, 0);

    char *question = strtok(questions_data, "|");
    while (question != NULL) {
        int question_id;
        int is_selected;
        sscanf(question, "%d,%d", &question_id, &is_selected);

        // Chỉ thêm câu hỏi nếu checkbox được chọn
        if (is_selected == 1) {
            sqlite3_bind_int(insert_stmt, 1, room_id);
            sqlite3_bind_int(insert_stmt, 2, question_id);

            if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
                printf("Error inserting question %d: %s\n", question_id, sqlite3_errmsg(db));
                sqlite3_finalize(insert_stmt);
                sqlite3_exec(db, "ROLLBACK", 0, 0, 0);
                send(socket_fd, "ERROR:Failed to insert questions", 31, 0);
                return;
            }
            sqlite3_reset(insert_stmt);
        }

        question = strtok(NULL, "|");
    }

    sqlite3_finalize(insert_stmt);

    // Commit transaction
    rc = sqlite3_exec(db, "COMMIT", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        printf("Error committing transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK", 0, 0, 0);
        send(socket_fd, "ERROR:Failed to commit changes", 29, 0);
        return;
    }

    // Gửi thông báo thành công
    send(socket_fd, "SUCCESS:Questions updated successfully", 36, 0);
}


void handle_get_questions(int socket_fd) {
    char *sql = "SELECT question_id, question_text, difficulty "
                "FROM questions "
                "ORDER BY question_id;";
                
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    char response[MAX_BUFFER] = "QUESTIONS:";
    char temp[1024];
    int first = 1;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) strcat(response, "|");
        first = 0;
        
        int id = sqlite3_column_int(stmt, 0);
        const char *text = (const char*)sqlite3_column_text(stmt, 1);
        int difficulty = sqlite3_column_int(stmt, 2);
        
        snprintf(temp, sizeof(temp), "%d,%s,%d", id, text, difficulty);
        strcat(response, temp);
    }
    
    sqlite3_finalize(stmt);
    send(socket_fd, response, strlen(response), 0);
}

void handle_get_question(int socket_fd, int question_id) {
    char *sql = "SELECT q.question_id, q.question_text, q.difficulty, "
                "c.choice_id, c.choice_text, c.is_correct "
                "FROM questions q "
                "LEFT JOIN choices c ON q.question_id = c.question_id "
                "WHERE q.question_id = ? "
                "ORDER BY c.choice_id;";
                
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, question_id);
    
    char response[MAX_BUFFER] = "QUESTION:";
    char temp[1024];
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        snprintf(temp, sizeof(temp), "%d;%s;%d",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_int(stmt, 2));
        strcat(response, temp);
        
        do {
            snprintf(temp, sizeof(temp), ";%d;%s;%d",
                    sqlite3_column_int(stmt, 3),
                    sqlite3_column_text(stmt, 4),
                    sqlite3_column_int(stmt, 5));
            strcat(response, temp);
        } while (sqlite3_step(stmt) == SQLITE_ROW);
        
        send(socket_fd, response, strlen(response), 0);
    } else {
        send(socket_fd, "ERROR:Không tìm thấy câu hỏi", 30, 0);
    }
    
    sqlite3_finalize(stmt);
}

void handle_delete_question(int socket_fd, int question_id) {
    // Delete choices
    char *sql = "DELETE FROM choices WHERE question_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, question_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Delete question
    sql = "DELETE FROM questions WHERE question_id = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, question_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE) {
        send(socket_fd, "SUCCESS:Xóa câu hỏi thành công", 36, 0);
    } else {
        send(socket_fd, "ERROR:Không thể xóa câu hỏi", 28, 0);
    }
}

void handle_add_question(int socket_fd, char *data) {
    char question_text[512];
    int difficulty;
    char choices[4][256];
    int is_correct[4];
    
    // Parse dữ liệu
    char *token = strtok(data, "|");
    strcpy(question_text, token);
    
    token = strtok(NULL, "|");
    difficulty = atoi(token);
    difficulty += 1;
    
    for (int i = 0; i < 4; i++) {
        token = strtok(NULL, "|");
        strcpy(choices[i], token);
        
        token = strtok(NULL, "|");
        is_correct[i] = atoi(token);
    }
    
    // Thêm câu hỏi vào database
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
    
    char *sql = "INSERT INTO questions (question_text, difficulty) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, question_text, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, difficulty);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE) {
        int question_id = sqlite3_last_insert_rowid(db);
        
        // Thêm các đáp án
        sql = "INSERT INTO choices (question_id, choice_text, is_correct) VALUES (?, ?, ?);";
        for (int i = 0; i < 4; i++) {
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
            sqlite3_bind_int(stmt, 1, question_id);
            sqlite3_bind_text(stmt, 2, choices[i], -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, is_correct[i]);
            
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc != SQLITE_DONE) {
                sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
                send(socket_fd, "ERROR:Không thể thêm đáp án", 28, 0);
                return;
            }
        }
        
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        send(socket_fd, "SUCCESS:Thêm câu hỏi thành công", 37, 0);
    } else {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        send(socket_fd, "ERROR:Không thể thêm câu hỏi", 28, 0);
    }
}

void handle_update_question(int socket_fd, char *data) {
    int question_id;
    char question_text[512];
    int difficulty;
    char choices[4][256];
    int is_correct[4];
    
    // Parse dữ liệu
    char *token = strtok(data, "|");
    question_id = atoi(token);
    
    token = strtok(NULL, "|");
    strcpy(question_text, token);
    
    token = strtok(NULL, "|");
    difficulty = atoi(token);
    difficulty += 1;
    
    // Cập nhật câu hỏi
    char *sql = "UPDATE questions SET question_text = ?, difficulty = ? WHERE question_id = ?";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, question_text, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, difficulty);
    sqlite3_bind_int(stmt, 3, question_id);
    
    rc = sqlite3_step(stmt);
    char *expanded_sql = sqlite3_expanded_sql(stmt);
    if (expanded_sql) {
        printf("Expanded query: %s\n", expanded_sql);
        sqlite3_free((void *)expanded_sql);
    }
    sqlite3_finalize(stmt);

    printf("rc: %d\n", rc);
    
    if (rc == SQLITE_DONE) {
        // Cập nhật các lựa chọn
        for (int i = 0; i < 4; i++) {
            token = strtok(NULL, "|");
            int choice_id = atoi(token);
            
            token = strtok(NULL, "|");
            strcpy(choices[i], token);
            
            token = strtok(NULL, "|");
            is_correct[i] = atoi(token);
            
            sql = "UPDATE choices SET choice_text = ?, is_correct = ? WHERE choice_id = ?";
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, choices[i], -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, is_correct[i]);
            sqlite3_bind_int(stmt, 3, choice_id);
            
            rc = sqlite3_step(stmt);
            char *expanded_choice_sql = sqlite3_expanded_sql(stmt);
            if (expanded_choice_sql) {
                printf("Expanded query: %s\n", expanded_choice_sql);
                sqlite3_free((void *)expanded_choice_sql);
            }
            sqlite3_finalize(stmt);
            
            if (rc != SQLITE_DONE) {
                send(socket_fd, "ERROR:Không thể cập nhật đáp án", 41, 0);
                return;
            }
        }
        
        send(socket_fd, "SUCCESS:Cập nhật câu hỏi thành công, mở lại để xem sự thay đổi", 83, 0);
    } else {
        send(socket_fd, "ERROR:Không thể cập nhật câu hỏi", 42, 0);
    }
}

void handle_get_room_questions(int socket_fd, int room_id) {
    char *sql = "SELECT q.question_id, q.question_text, q.difficulty, "
                "CASE WHEN eq.room_id IS NOT NULL THEN 1 ELSE 0 END as is_selected "
                "FROM questions q "
                "LEFT JOIN exam_questions eq ON q.question_id = eq.question_id AND eq.room_id = ? "
                "ORDER BY q.question_id;";
                
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, room_id);
    
    char response[MAX_BUFFER];
    memset(response, 0, MAX_BUFFER);
    strcpy(response, "QUESTIONS:");
    
    char temp[1024];
    int first = 1;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) {
            strcat(response, "|");
        }
        
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *text = sqlite3_column_text(stmt, 1);
        int difficulty = sqlite3_column_int(stmt, 2);
        int is_selected = sqlite3_column_int(stmt, 3);
        
        if (text) {
            snprintf(temp, sizeof(temp), "%d,%s,%d,%d", 
                    id, text, difficulty, is_selected);
            if (strlen(response) + strlen(temp) + 2 < MAX_BUFFER) {
                strcat(response, temp);
                first = 0;
            }
        }
    }
    
    sqlite3_finalize(stmt);
    send(socket_fd, response, strlen(response), 0);
}

void send_score_list(int socket_fd, int room_id) {
    char *sql = "SELECT u.username, ep.score, ep.start_time, ep.end_time "
                "FROM exam_participants ep "
                "JOIN users u ON ep.user_id = u.id "
                "WHERE ep.room_id = ? "
                "ORDER BY ep.score DESC";
                
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        send(socket_fd, "ERROR:Database error", 19, 0);
        return;
    }

    sqlite3_bind_int(stmt, 1, room_id);
    
    char response[MAX_BUFFER] = "SCORE_LIST:";
    char temp[MAX_BUFFER];
    int first = 1;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *username = (const char *)sqlite3_column_text(stmt, 0);
        int score = sqlite3_column_int(stmt, 1);
        const char *start_time = (const char *)sqlite3_column_text(stmt, 2);
        const char *end_time = (const char *)sqlite3_column_text(stmt, 3);
        
        if (!first) {
            strcat(response, "|");
        }
        
        snprintf(temp, sizeof(temp), "%s,%d,%s,%s",
                username, score, start_time, end_time);
        
        strcat(response, temp);
        first = 0;
    }
    
    sqlite3_finalize(stmt);
    send(socket_fd, response, strlen(response), 0);
}

int add_exam_room(const char *room_name, int time_limit, const char *start_time, const char *end_time) {
    char *sql = "INSERT INTO exam_rooms (room_name, time_limit, start_time, end_time) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, room_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, time_limit);
    sqlite3_bind_text(stmt, 3, start_time, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, end_time, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    char *expanded_sql = sqlite3_expanded_sql(stmt);
    if (expanded_sql) {
        printf("Expanded query: %s\n", expanded_sql);
        sqlite3_free((void *)expanded_sql);
    }
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

void handle_server(int socket_fd, char *buffer) {
    if (strncmp(buffer, "GET_QUESTIONS", 13) == 0) {
        handle_get_questions(socket_fd);
    } else if (strncmp(buffer, "GET_QUESTION|", 13) == 0) {
        int question_id;
        sscanf(buffer + 13, "%d", &question_id);
        handle_get_question(socket_fd, question_id);
    } else if (strncmp(buffer, "DELETE_QUESTION|", 16) == 0) {
        int question_id;
        sscanf(buffer + 16, "%d", &question_id);
        handle_delete_question(socket_fd, question_id);
    } else if (strncmp(buffer, "ADD_QUESTION|", 13) == 0) {
        handle_add_question(socket_fd, buffer + 13);
    } else if (strncmp(buffer, "UPDATE_QUESTION|", 16) == 0) {
        handle_update_question(socket_fd, buffer + 16);
    } else if (strncmp(buffer, "LOGOUT", 6) == 0) {
        handle_logout(socket_fd);
    } else if (strncmp(buffer, "GET_EXAM_ROOMS_TEACHER", 22) == 0) {
        send_exam_rooms(socket_fd);
    } else if (strncmp(buffer, "GET_SCORE_LIST", 14) == 0) {
        int room_id;
        sscanf(buffer + 15, "%d", &room_id);
        send_score_list(socket_fd, room_id);
    } else if (strncmp(buffer, "GET_EXAM_ROOMS", 14) == 0) {
        send_exam_rooms(socket_fd);
    } else if (strncmp(buffer, "ADD_EXAM_ROOM|", 14) == 0) {
        char room_name[100], start_time[50], end_time[50];
        int time_limit;
        sscanf(buffer + 14, "%[^|]|%d|%[^|]|%[^\n]", room_name, &time_limit, start_time, end_time);
        
        if (add_exam_room(room_name, time_limit, start_time, end_time)) {
            send(socket_fd, "SUCCESS:Room added successfully", 31, 0);
        }
    } else  if (strncmp(buffer, "GET_ROOM_QUESTIONS|", 19) == 0) {
        int room_id;
        sscanf(buffer + 19, "%d", &room_id);
        handle_get_room_questions(socket_fd, room_id);
    } else if (strncmp(buffer, "UPDATE_ROOM_QUESTIONS|", 21) == 0) {
        handle_update_room_questions(socket_fd, buffer);
    } else if (strncmp(buffer, "JOIN_EXAM|", 10) == 0) {
        int room_id;
        sscanf(buffer + 10, "%d", &room_id);
        handle_join_exam(socket_fd, room_id);
    } else if (strncmp(buffer, "SUBMIT_EXAM|", 12) == 0) {
        handle_submit_exam(socket_fd, buffer + 12);
    } else if (strncmp(buffer, "GET_PRACTICE_QUESTIONS", 22) == 0) {
        handle_practice_questions(socket_fd);
    } else if (strncmp(buffer, "SUBMIT_PRACTICE|", 16) == 0) {
        handle_submit_practice(socket_fd, buffer + 16);
    } else if (strncmp(buffer, "GET_EXAM_HISTORY", 16) == 0) {
        send_exam_history(socket_fd);
    } else {
        printf("Unknown message: %s\n", buffer);
    }
}

void handle_logout(int socket_fd) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (loggedInUsers[i].socket_fd == socket_fd) {
            loggedInUsers[i].socket_fd = 0;
            loggedInUsers[i].user_id = 0;
            memset(loggedInUsers[i].username, 0, sizeof(loggedInUsers[i].username));
            memset(loggedInUsers[i].role, 0, sizeof(loggedInUsers[i].role));
            printf("User logged out: %s\n", loggedInUsers[i].username);
            break;
        }
    }

}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Sử dụng: %s <port>\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        printf("Port không hợp lệ. Vui lòng sử dụng port từ 1-65535\n");
        return 1;
    }

    // Xóa define PORT cũ và sử dụng port từ argument
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    printf("Server đang chạy trên port %d...\n", port);
    
    int server_fd, new_socket, max_sd, sd, activity, valread;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[MAX_BUFFER] = {0};
    fd_set readfds;

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

    init_logged_in_users();
    // Khởi tạo database
    init_database();

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;
        
        for (int i = 0; i < MAX_SESSIONS; i++) {
            sd = loggedInUsers[i].socket_fd;
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }

            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            for (int i = 0; i < MAX_SESSIONS; i++) {
                if (loggedInUsers[i].socket_fd == 0) {
                    loggedInUsers[i].socket_fd = new_socket;
                    printf("New client connected: %d\n", new_socket);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_SESSIONS; i++) {
            sd = loggedInUsers[i].socket_fd;
            if (FD_ISSET(sd, &readfds)) {
                memset(buffer, 0, MAX_BUFFER);
                int valread = read(sd, buffer, MAX_BUFFER);
                if (valread <= 0) break;
                LoggedInUser currentSocketUser;

                if (strncmp(buffer, "LOGIN|", 6) == 0) {
                    char username[50], password[50], role[20];
                    sscanf(buffer + 6, "%[^|]|%s", username, password);
                    if (validate_login(username, password, sd)) {
                        char response[MAX_BUFFER];
                        currentSocketUser = get_session_by_socket(sd);
                        snprintf(response, sizeof(response), "SUCCESS:%s", currentSocketUser.role);
                        send(sd, response, strlen(response), 0);
                    } else {
                        printf("Fail: %s %s", currentSocketUser.username, currentSocketUser.role);
                        send(sd, "FAIL:Invalid username or password", 32, 0);
                    }
                } else if (strncmp(buffer, "REGISTER|", 9) == 0) {
                    char username[50], password[50];
                    sscanf(buffer + 9, "%[^|]|%s", username, password);
                    if (register_user(username, password)) {
                        send(sd, "SUCCESS:Registration successful", 29, 0);
                    } else {
                        send(sd, "FAIL:Username already exists", 26, 0);
                    }
                } else {
                    handle_server(sd, buffer);
                }
            }
        }
    }

    // Đóng database
    sqlite3_close(db);
    return 0;
}
