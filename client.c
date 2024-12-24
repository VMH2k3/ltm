#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_BUFFER 16384

typedef struct {
    char server_ip[16]; 
    int server_port;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *status_label;
    GtkWidget *window;
    GtkWidget *main_window;
    int sock;
} AppData;

typedef struct {
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *confirm_entry;
    GtkWidget *status_label;
    GtkWidget *window;
    int sock;
} RegisterData;
typedef struct {
    GtkWidget *name_entry;
    GtkWidget *time_limit_entry;
    GtkWidget *start_time_entry;
    GtkWidget *end_time_entry;
    GtkWidget *status_label;
    GtkWidget *window;
    int sock;
} AddRoomData;

typedef struct {
    GtkLabel *label;
    int minutes;
    int seconds;
} TimerData;

typedef struct {
    int question_id;
    int selected_choice;
} Answer;

typedef struct {
    GtkWidget *dialog;
    int room_id;
    Answer *answers;
    int answer_count;
    int sock;
} ExamData;

// Tạo struct để truyền dữ liệu qua callback
typedef struct {
    int question_id;
    AppData *app_data;
    GtkWidget *window;
    int sock;
} DeleteUpdateData;

// Cấu trúc cho dữ liệu câu hỏi
typedef struct {
    int question_id;
    GtkWidget *question_text;
    GtkWidget *difficulty;
    struct {
        int choice_id;
        GtkWidget *choice_text;
        GtkWidget *is_correct;
    } choices[4];
    GtkWidget *window;
    AppData *app_data;
    int sock;
} Question;

// Khai báo các hàm
static void handle_question_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);
static void handle_delete_response(GtkDialog *dialog, gint response_id, gpointer user_data);
static void show_question_dialog(GtkButton *button, gpointer user_data);
static void edit_question(GtkButton *button, gpointer user_data);
static void delete_question(GtkButton *button, gpointer user_data);
static void load_questions(GtkListBox *list_box, AppData *data);
static void show_update_question(GtkButton *button, gpointer user_data);

static void logout(GtkWidget *button, AppData *data) {
    // Gửi thông báo logout đến server
    send(data->sock, "LOGOUT", 6, 0);
    
    // Đóng socket hiện tại
    close(data->sock);
    
    // Tạo socket mới
    data->sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(data->server_port);
    if (inet_pton(AF_INET, data->server_ip, &serv_addr.sin_addr) <= 0) {
        printf("Địa chỉ IP không hợp lệ\n");
        return;
    }
    
    if (connect(data->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Không thể kết nối lại đến server\n");
        return;
    }
    
    // Đóng cửa sổ chính và hiển thị cửa sổ đăng nhập
    gtk_window_destroy(GTK_WINDOW(data->main_window));
    gtk_widget_show(data->window);
}

static void dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static gboolean show_success_dialog(const char *message, AppData *data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Thành công",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_OK",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(message + 8); 
    gtk_box_append(GTK_BOX(content_area), label);

    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
    return TRUE;
}

static gboolean show_error_dialog(const char *message, AppData *data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Lỗi",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_OK",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(message + 6); 
    gtk_box_append(GTK_BOX(content_area), label);
    
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
    
    return TRUE; // Trả về TRUE để dừng các callback tiếp theo
}

static gboolean update_timer(gpointer user_data) {
    TimerData *data = (TimerData *)user_data;
    
    // Kiểm tra con trỏ
    if (!data || !data->label) {
        return G_SOURCE_REMOVE;
    }

    // Kiểm tra widget có còn tồn tại và visible
    if (!GTK_IS_WIDGET(data->label) || !gtk_widget_get_visible(GTK_WIDGET(data->label))) {
        g_free(data);
        return G_SOURCE_REMOVE;
    }
    
    // Giảm thời gian
    if (data->seconds > 0) {
        data->seconds--;
    } else if (data->minutes > 0) {
        data->minutes--;
        data->seconds = 59;
    } else {
        // Hết giờ
        gtk_label_set_text(data->label, "Hết giờ!");
        g_free(data);  // Giải phóng bộ nhớ
        return G_SOURCE_REMOVE;
    }

    // Cập nhật label
    char new_text[50];
    snprintf(new_text, sizeof(new_text), "Thời gian còn lại: %02d:%02d", 
             data->minutes, data->seconds);
    gtk_label_set_text(data->label, new_text);

    return G_SOURCE_CONTINUE;
}

static void on_answer_selected(GtkCheckButton *button, gpointer user_data) {
    if (gtk_check_button_get_active(button)) {
        Answer *answer = (Answer *)user_data;
        answer->selected_choice = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "choice_id"));
    }
}

static void handle_exam_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    ExamData *exam_data = (ExamData *)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        // Tạo chuỗi chứa kết quả
        char request[MAX_BUFFER] = "SUBMIT_EXAM|";
        char temp[32];
        snprintf(temp, sizeof(temp), "%d", exam_data->room_id);
        strcat(request, temp);
        
        for (int i = 0; i < exam_data->answer_count; i++) {
            snprintf(temp, sizeof(temp), "|%d,%d", 
                    exam_data->answers[i].question_id,
                    exam_data->answers[i].selected_choice);
            strcat(request, temp);
        }
        
        // Gửi lên server
        send(exam_data->sock, request, strlen(request), 0);
        
        // Nhận response
        char response[MAX_BUFFER] = {0};  // Khởi tạo buffer với 0
        ssize_t received = recv(exam_data->sock, response, MAX_BUFFER, 0);        
        if (received < 0) {
            perror("Error receiving response");
            return;
        }
        
        if (strncmp(response, "ERROR:", 6) == 0) {
            AppData temp_data = {
                .main_window = GTK_WIDGET(dialog)
            };
            show_error_dialog(response, &temp_data);
        } else if (strncmp(response, "RESULT:", 7) == 0) {
            // Lấy kết quả từ phản hồi
            GtkWidget *result_dialog = gtk_dialog_new_with_buttons(
                "Kết quả bài thi",
                GTK_WINDOW(dialog),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                "_OK",
                GTK_RESPONSE_ACCEPT,
                NULL
            );
            
            GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(result_dialog));
            gtk_widget_set_margin_start(content_area, 20);
            gtk_widget_set_margin_end(content_area, 20);
            gtk_widget_set_margin_top(content_area, 20);
            gtk_widget_set_margin_bottom(content_area, 20);

            // Tạo label mới cho kết quả
            GtkWidget *result_label = gtk_label_new(NULL);
            gtk_label_set_text(GTK_LABEL(result_label), response + 7);
            gtk_box_append(GTK_BOX(content_area), result_label);
            
            // Kết nối tín hiệu để đóng cả hai dialog
            g_signal_connect_swapped(result_dialog, "response", 
                                     G_CALLBACK(gtk_window_destroy), dialog);
            g_signal_connect(result_dialog, "response", 
                             G_CALLBACK(gtk_window_destroy), result_dialog);

            // Hiển thị dialog kết quả
            gtk_window_present(GTK_WINDOW(result_dialog));
        }
    }
    
    // Giải phóng bộ nhớ và đóng dialog thi
    g_free(exam_data->answers);
    g_free(exam_data);
}

static void show_exam_interface(AppData *data, const char *exam_data, int room_id, int time_limit) {
    // Tạo dialog thay vì window
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Làm bài thi",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Nộp bài",
                                                   GTK_RESPONSE_ACCEPT,
                                                   "_Hủy",
                                                   GTK_RESPONSE_CANCEL,
                                                   NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1000, 800);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content_area, 20);
    gtk_widget_set_margin_end(content_area, 20);
    gtk_widget_set_margin_top(content_area, 20);
    gtk_widget_set_margin_bottom(content_area, 20);

    // Header hiển thị thời gian
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    char initial_time[50];
    snprintf(initial_time, sizeof(initial_time), "Thời gian còn lại: %02d:00", time_limit);
    GtkWidget *time_label = gtk_label_new(initial_time);
    gtk_box_append(GTK_BOX(header_box), time_label);
    
    // Cấp phát động cho timer data
    TimerData *timer_data = g_new(TimerData, 1);
    timer_data->label = GTK_LABEL(time_label);
    timer_data->minutes = time_limit;
    timer_data->seconds = 0;
    
    // Thiết lập timer
    guint timer_id = g_timeout_add_seconds(1, update_timer, timer_data);

    // Scrolled window cho danh sách câu hỏi
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 600);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);

    // Box chứa các câu hỏi
    GtkWidget *questions_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);

    // Phân tích dữ liệu câu hỏi
    char *question = strtok((char *)exam_data, "|");
    int q_num = 1;
    
    // Khởi tạo ExamData
    ExamData *exam_send_data = g_new(ExamData, 1);
    exam_send_data->room_id = room_id;
    exam_send_data->answers = g_new(Answer, 20);
    exam_send_data->answer_count = 0;
    exam_send_data->sock = data->sock;

    while (question != NULL) {
        // Frame cho mỗi câu hỏi
        GtkWidget *q_frame = gtk_frame_new(NULL);
        GtkWidget *q_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_start(q_box, 10);
        gtk_widget_set_margin_end(q_box, 10);
        gtk_widget_set_margin_top(q_box, 10);
        gtk_widget_set_margin_bottom(q_box, 10);

        // Phân tích thông tin câu hỏi
        char q_text[256], a1[100], a2[100], a3[100], a4[100];
        int q_id, a1_id, a2_id, a3_id, a4_id;
        sscanf(question, "%d;%[^;];%d;%[^;];%d;%[^;];%d;%[^;];%d;%[^;]", 
               &q_id, q_text, &a1_id, a1, &a2_id, a2, &a3_id, a3, &a4_id, a4);

        // Text câu hỏi
        char q_label_text[300];
        snprintf(q_label_text, sizeof(q_label_text), "Câu %d: %s", q_num, q_text);
        GtkWidget *q_label = gtk_label_new(q_label_text);
        gtk_label_set_wrap(GTK_LABEL(q_label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(q_label), 0);
        gtk_box_append(GTK_BOX(q_box), q_label);

        // Radio buttons cho các đáp án (sử dụng check button với group)
        GtkWidget *group = gtk_check_button_new_with_label(a1);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(group), NULL);
        
        GtkWidget *radio2 = gtk_check_button_new_with_label(a2);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(radio2), GTK_CHECK_BUTTON(group));
        
        GtkWidget *radio3 = gtk_check_button_new_with_label(a3);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(radio3), GTK_CHECK_BUTTON(group));
        
        GtkWidget *radio4 = gtk_check_button_new_with_label(a4);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(radio4), GTK_CHECK_BUTTON(group));

        // Lưu ID đáp án vào button
        g_object_set_data(G_OBJECT(group), "choice_id", GINT_TO_POINTER(a1_id));
        g_object_set_data(G_OBJECT(radio2), "choice_id", GINT_TO_POINTER(a2_id));
        g_object_set_data(G_OBJECT(radio3), "choice_id", GINT_TO_POINTER(a3_id));
        g_object_set_data(G_OBJECT(radio4), "choice_id", GINT_TO_POINTER(a4_id));

        // Thêm các đáp án vào box
        gtk_box_append(GTK_BOX(q_box), group);
        gtk_box_append(GTK_BOX(q_box), radio2);
        gtk_box_append(GTK_BOX(q_box), radio3);
        gtk_box_append(GTK_BOX(q_box), radio4);

        gtk_frame_set_child(GTK_FRAME(q_frame), q_box);
        gtk_box_append(GTK_BOX(questions_box), q_frame);

        // Lưu question_id vào answers array
        exam_send_data->answers[exam_send_data->answer_count].question_id = q_id;
        exam_send_data->answers[exam_send_data->answer_count].selected_choice = -1;
        exam_send_data->answer_count++;

        // Kết nối signal cho radio buttons
        g_signal_connect(group, "toggled", G_CALLBACK(on_answer_selected), &exam_send_data->answers[exam_send_data->answer_count-1]);
        g_signal_connect(radio2, "toggled", G_CALLBACK(on_answer_selected), &exam_send_data->answers[exam_send_data->answer_count-1]);
        g_signal_connect(radio3, "toggled", G_CALLBACK(on_answer_selected), &exam_send_data->answers[exam_send_data->answer_count-1]);
        g_signal_connect(radio4, "toggled", G_CALLBACK(on_answer_selected), &exam_send_data->answers[exam_send_data->answer_count-1]);

        question = strtok(NULL, "|");
        q_num++;
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), questions_box);

    // Thêm vào content area
    gtk_box_append(GTK_BOX(content_area), header_box);
    gtk_box_append(GTK_BOX(content_area), scrolled);

    exam_send_data->dialog = dialog;
    g_signal_connect(dialog, "response", G_CALLBACK(handle_exam_response), exam_send_data);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

static void join_exam_room(GtkButton *button, AppData *data) {
    int room_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "room_id"));
    int time_limit = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "time_limit"));
    
    // Gửi yêu cầu tham gia phòng thi
    char request[MAX_BUFFER];
    snprintf(request, sizeof(request), "JOIN_EXAM|%d", room_id);
    send(data->sock, request, strlen(request), 0);
    
    // Nhận danh sách câu hỏi
    char response[MAX_BUFFER];
    recv(data->sock, response, MAX_BUFFER, 0);
    
    // Kiểm tra nếu có lỗi
    if (strncmp(response, "ERROR:", 6) == 0) {
        show_error_dialog(response, data);
        return;
    }
    
    // Kiểm tra và bỏ prefix QUESTIONS:
    char *exam_data = response;
    if (strncmp(response, "QUESTIONS:", 10) == 0) {
        exam_data = response + 10;
    }
    
    // Hiển thị giao diện làm bài thi với time_limit
    show_exam_interface(data, exam_data, room_id, time_limit);
}

static void show_exam_list(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Tạo cửa sổ mới
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Danh sách phòng thi",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Đóng",
                                                   GTK_RESPONSE_CLOSE,
                                                   NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1000, 600);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Tạo list box để hiển thị danh sách phòng
    GtkListBox *list_box = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_set_margin_start(GTK_WIDGET(list_box), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(list_box), 10);
    gtk_widget_set_margin_top(GTK_WIDGET(list_box), 10);
    gtk_widget_set_margin_bottom(GTK_WIDGET(list_box), 10);
    
    // Gửi yêu cầu lấy danh sách phòng từ server
    char request[MAX_BUFFER] = "GET_EXAM_ROOMS";
    send(data->sock, request, strlen(request), 0);
    
    char response[MAX_BUFFER];
    recv(data->sock, response, MAX_BUFFER, 0);
    
    // Kiểm tra message header
    if (strncmp(response, "EXAM_ROOMS_LIST:", 16) != 0) {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Lỗi khi lấy danh sách phòng thi");
        return;
    }
    
    // Bỏ qua header để lấy dữ liệu
    char *rooms_data = response + 16;
    
    // Phân tích response và tạo các row cho list box
    char *room = strtok(rooms_data, "|");
    while (room != NULL) {
        int room_id;
        char room_name[100];
        char status[20];
        int time_limit;
        char start_time[50];
        char end_time[50];
        
        sscanf(room, "%d,%[^,],%[^,],%d,%[^,],%[^,]", 
               &room_id, room_name, status, &time_limit, start_time, end_time);
        
        // Tạo row cho mỗi phòng thi
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
        gtk_widget_set_margin_start(box, 10);
        gtk_widget_set_margin_end(box, 10);
        gtk_widget_set_margin_top(box, 5);
        gtk_widget_set_margin_bottom(box, 5);
        
        // Tạo và định dạng các label
        GtkWidget *name_label = gtk_label_new(room_name);
        gtk_widget_set_size_request(name_label, 200, -1);
        
        GtkWidget *status_label = gtk_label_new(status);
        gtk_widget_set_size_request(status_label, 100, -1);
        
        // Sửa lỗi gtk_label_new_printf
        char time_text[32];
        snprintf(time_text, sizeof(time_text), "%d phút", time_limit);
        GtkWidget *time_label = gtk_label_new(time_text);
        gtk_widget_set_size_request(time_label, 80, -1);
        
        GtkWidget *start_label = gtk_label_new(start_time);
        gtk_widget_set_size_request(start_label, 150, -1);
        
        GtkWidget *end_label = gtk_label_new(end_time);
        gtk_widget_set_size_request(end_label, 150, -1);
        
        GtkWidget *join_button = gtk_button_new_with_label("Tham gia");
        
        // Thêm các widget vào box
        gtk_box_append(GTK_BOX(box), name_label);
        gtk_box_append(GTK_BOX(box), status_label);
        gtk_box_append(GTK_BOX(box), time_label);
        gtk_box_append(GTK_BOX(box), start_label);
        gtk_box_append(GTK_BOX(box), end_label);
        gtk_box_append(GTK_BOX(box), join_button);
        
        // Lưu room_id vào button để sử dụng khi click
        g_object_set_data(G_OBJECT(join_button), "room_id", GINT_TO_POINTER(room_id));
        g_object_set_data(G_OBJECT(join_button), "time_limit", GINT_TO_POINTER(time_limit));
        g_signal_connect(join_button, "clicked", G_CALLBACK(join_exam_room), data);
        
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        gtk_list_box_append(list_box, GTK_LIST_BOX_ROW(row));
        
        room = strtok(NULL, "|");
    }
    
    gtk_box_append(GTK_BOX(content_area), GTK_WIDGET(list_box));
    
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void show_history(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Tạo dialog mới
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Lịch sử thi",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Đóng",
                                                   GTK_RESPONSE_CLOSE,
                                                   NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 400);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content_area, 20);
    gtk_widget_set_margin_end(content_area, 20);
    gtk_widget_set_margin_top(content_area, 20);
    gtk_widget_set_margin_bottom(content_area, 20);
    
    // Tạo list box để hiển thị lịch sử
    GtkListBox *list_box = GTK_LIST_BOX(gtk_list_box_new());
    
    // Gửi yêu cầu lấy lịch sử thi
    send(data->sock, "GET_EXAM_HISTORY", 16, 0);
    
    char response[MAX_BUFFER];
    recv(data->sock, response, MAX_BUFFER, 0);
    
    if (strncmp(response, "ERROR:", 6) == 0) {
        GtkWidget *error_label = gtk_label_new(response + 6);
        gtk_box_append(GTK_BOX(content_area), error_label);
    } else if (strncmp(response, "EXAM_HISTORY:", 13) == 0) {
        char *history = response + 13;
        printf("history: %s\n", history);
        char *record = strtok(history, "|");
        
        // Header row
        GtkWidget *header_row = gtk_list_box_row_new();
        GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
        gtk_widget_set_margin_start(header_box, 10);
        gtk_widget_set_margin_end(header_box, 10);
        gtk_widget_set_margin_top(header_box, 5);
        gtk_widget_set_margin_bottom(header_box, 5);
        
        GtkWidget *room_header = gtk_label_new("Phòng thi");
        GtkWidget *score_header = gtk_label_new("Điểm");
        GtkWidget *start_header = gtk_label_new("Thời gian bắt đầu");
        GtkWidget *end_header = gtk_label_new("Thời gian kết thúc");
        
        gtk_widget_set_size_request(room_header, 200, -1);
        gtk_widget_set_size_request(score_header, 100, -1);
        gtk_widget_set_size_request(start_header, 200, -1);
        gtk_widget_set_size_request(end_header, 200, -1);
        
        gtk_box_append(GTK_BOX(header_box), room_header);
        gtk_box_append(GTK_BOX(header_box), score_header);
        gtk_box_append(GTK_BOX(header_box), start_header);
        gtk_box_append(GTK_BOX(header_box), end_header);
        
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(header_row), header_box);
        gtk_list_box_append(list_box, GTK_LIST_BOX_ROW(header_row));
        
        while (record != NULL) {
            printf("record: %s\n", record);
            char score_str[20], start_time[50], end_time[50], room_name[100];
            memset(start_time, 0, sizeof(start_time));
            memset(end_time, 0, sizeof(end_time));
            memset(room_name, 0, sizeof(room_name));
            sscanf(record, "%[^,],%[^,],%[^,],%[^\n]", score_str, start_time, end_time, room_name);
            
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
            gtk_widget_set_margin_start(box, 10);
            gtk_widget_set_margin_end(box, 10);
            gtk_widget_set_margin_top(box, 5);
            gtk_widget_set_margin_bottom(box, 5);

            printf("room_name: %s, score: %s, start_time: %s, end_time: %s\n", room_name, score_str, start_time, end_time);
            
            GtkWidget *room_label = gtk_label_new(room_name);
            GtkWidget *score_label = gtk_label_new(score_str);
            GtkWidget *start_label = gtk_label_new(start_time);
            GtkWidget *end_label = gtk_label_new(end_time);
            
            gtk_widget_set_size_request(room_label, 200, -1);
            gtk_widget_set_size_request(score_label, 100, -1);
            gtk_widget_set_size_request(start_label, 200, -1);
            gtk_widget_set_size_request(end_label, 200, -1);
            
            gtk_box_append(GTK_BOX(box), room_label);
            gtk_box_append(GTK_BOX(box), score_label);
            gtk_box_append(GTK_BOX(box), start_label);
            gtk_box_append(GTK_BOX(box), end_label);
            
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
            gtk_list_box_append(list_box, GTK_LIST_BOX_ROW(row));
            
            record = strtok(NULL, "|");
        }
    }
    
    gtk_box_append(GTK_BOX(content_area), GTK_WIDGET(list_box));
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void handle_practice_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    ExamData *practice_data = (ExamData *)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        // Tạo chuỗi chứa kết quả
        char request[MAX_BUFFER] = "SUBMIT_PRACTICE|";
        
        for (int i = 0; i < practice_data->answer_count; i++) {
            char temp[32];
            snprintf(temp, sizeof(temp), "%d,%d|", 
                    practice_data->answers[i].question_id,
                    practice_data->answers[i].selected_choice);
            strcat(request, temp);
        }
        
        // Gửi lên server
        send(practice_data->sock, request, strlen(request), 0);
        
        // Nhận response
        char response[MAX_BUFFER];
        recv(practice_data->sock, response, MAX_BUFFER, 0);
        
        if (strncmp(response, "ERROR:", 6) == 0) {
            AppData temp_data = {
                .main_window = GTK_WIDGET(dialog)
            };
            show_error_dialog(response, &temp_data);
        } else if (strncmp(response, "RESULT:", 7) == 0) {
            // Hiển thị kết quả
            GtkWidget *result_dialog = gtk_dialog_new_with_buttons(
                "Kết quả luyện tập",
                GTK_WINDOW(dialog),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                "_OK",
                GTK_RESPONSE_ACCEPT,
                NULL
            );
            
            GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(result_dialog));
            gtk_widget_set_margin_start(content_area, 20);
            gtk_widget_set_margin_end(content_area, 20);
            gtk_widget_set_margin_top(content_area, 20);
            gtk_widget_set_margin_bottom(content_area, 20);

            GtkWidget *result_label = gtk_label_new(NULL);
            gtk_label_set_text(GTK_LABEL(result_label), response + 7);
            gtk_box_append(GTK_BOX(content_area), result_label);
            
            g_signal_connect(result_dialog, "response", 
                           G_CALLBACK(gtk_window_destroy), result_dialog);
            g_signal_connect_swapped(result_dialog, "response", 
                                   G_CALLBACK(gtk_window_destroy), dialog);

            gtk_window_present(GTK_WINDOW(result_dialog));
        }
    }
    
    // Giải phóng bộ nhớ
    g_free(practice_data->answers);
    g_free(practice_data);
}

static void show_practice_interface(AppData *data, const char *practice_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Luyện tập",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Nộp bài",
                                                   GTK_RESPONSE_ACCEPT,
                                                   "_Hủy",
                                                   GTK_RESPONSE_CANCEL,
                                                   NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1000, 800);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content_area, 20);
    gtk_widget_set_margin_end(content_area, 20);
    gtk_widget_set_margin_top(content_area, 20);
    gtk_widget_set_margin_bottom(content_area, 20);

    // Header hiển thị thời gian
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *time_label = gtk_label_new("Thời gian còn lại: 30:00");
    gtk_box_append(GTK_BOX(header_box), time_label);
    
    // Timer data
    TimerData *timer_data = g_new(TimerData, 1);
    timer_data->label = GTK_LABEL(time_label);
    timer_data->minutes = 30;
    timer_data->seconds = 0;
    
    // Thiết lập timer
    guint timer_id = g_timeout_add_seconds(1, update_timer, timer_data);

    // Scrolled window cho danh sách câu hỏi
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 600);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);

    // Box chứa các câu hỏi
    GtkWidget *questions_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);

    // Phân tích dữ liệu câu hỏi
    char *question = strtok((char *)practice_data, "|");
    int q_num = 1;
    
    // Khởi tạo ExamData cho practice
    ExamData *practice_data_struct = g_new(ExamData, 1);
    practice_data_struct->room_id = 0; // 0 để đánh dấu đây là practice
    practice_data_struct->answers = g_new(Answer, 20);
    practice_data_struct->answer_count = 0;
    practice_data_struct->sock = data->sock;
    practice_data_struct->dialog = dialog;

    while (question != NULL) {
        // Frame cho mỗi câu hỏi
        GtkWidget *q_frame = gtk_frame_new(NULL);
        GtkWidget *q_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_start(q_box, 10);
        gtk_widget_set_margin_end(q_box, 10);
        gtk_widget_set_margin_top(q_box, 10);
        gtk_widget_set_margin_bottom(q_box, 10);

        // Phân tích thông tin câu hỏi
        char q_text[256], a1[100], a2[100], a3[100], a4[100];
        int q_id, a1_id, a2_id, a3_id, a4_id;
        sscanf(question, "%d;%[^;];%d;%[^;];%d;%[^;];%d;%[^;];%d;%[^;]", 
               &q_id, q_text, &a1_id, a1, &a2_id, a2, &a3_id, a3, &a4_id, a4);

        // Text câu hỏi
        char q_label_text[300];
        snprintf(q_label_text, sizeof(q_label_text), "Câu %d: %s", q_num, q_text);
        GtkWidget *q_label = gtk_label_new(q_label_text);
        gtk_label_set_wrap(GTK_LABEL(q_label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(q_label), 0);
        gtk_box_append(GTK_BOX(q_box), q_label);

        // Radio buttons cho các đáp án
        GtkWidget *group = gtk_check_button_new_with_label(a1);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(group), NULL);
        
        GtkWidget *radio2 = gtk_check_button_new_with_label(a2);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(radio2), GTK_CHECK_BUTTON(group));
        
        GtkWidget *radio3 = gtk_check_button_new_with_label(a3);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(radio3), GTK_CHECK_BUTTON(group));
        
        GtkWidget *radio4 = gtk_check_button_new_with_label(a4);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(radio4), GTK_CHECK_BUTTON(group));

        // Lưu ID đáp án
        g_object_set_data(G_OBJECT(group), "choice_id", GINT_TO_POINTER(a1_id));
        g_object_set_data(G_OBJECT(radio2), "choice_id", GINT_TO_POINTER(a2_id));
        g_object_set_data(G_OBJECT(radio3), "choice_id", GINT_TO_POINTER(a3_id));
        g_object_set_data(G_OBJECT(radio4), "choice_id", GINT_TO_POINTER(a4_id));

        // Thêm các đáp án vào box
        gtk_box_append(GTK_BOX(q_box), group);
        gtk_box_append(GTK_BOX(q_box), radio2);
        gtk_box_append(GTK_BOX(q_box), radio3);
        gtk_box_append(GTK_BOX(q_box), radio4);

        gtk_frame_set_child(GTK_FRAME(q_frame), q_box);
        gtk_box_append(GTK_BOX(questions_box), q_frame);

        // Lưu question_id vào answers array
        practice_data_struct->answers[practice_data_struct->answer_count].question_id = q_id;
        practice_data_struct->answers[practice_data_struct->answer_count].selected_choice = -1;
        practice_data_struct->answer_count++;

        // Kết nối signal cho radio buttons
        g_signal_connect(group, "toggled", G_CALLBACK(on_answer_selected), 
                        &practice_data_struct->answers[practice_data_struct->answer_count-1]);
        g_signal_connect(radio2, "toggled", G_CALLBACK(on_answer_selected), 
                        &practice_data_struct->answers[practice_data_struct->answer_count-1]);
        g_signal_connect(radio3, "toggled", G_CALLBACK(on_answer_selected), 
                        &practice_data_struct->answers[practice_data_struct->answer_count-1]);
        g_signal_connect(radio4, "toggled", G_CALLBACK(on_answer_selected), 
                        &practice_data_struct->answers[practice_data_struct->answer_count-1]);

        question = strtok(NULL, "|");
        q_num++;
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), questions_box);

    // Thêm vào content area
    gtk_box_append(GTK_BOX(content_area), header_box);
    gtk_box_append(GTK_BOX(content_area), scrolled);

    g_signal_connect(dialog, "response", G_CALLBACK(handle_practice_response), practice_data_struct);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

static void start_practice(GtkButton *button, AppData *data) {
    // Gửi yêu cầu lấy câu hỏi luyện tập
    char request[MAX_BUFFER] = "GET_PRACTICE_QUESTIONS";
    send(data->sock, request, strlen(request), 0);
    
    // Nhận danh sách câu hỏi
    char response[MAX_BUFFER];
    recv(data->sock, response, MAX_BUFFER, 0);
    
    // Kiểm tra nếu có lỗi
    if (strncmp(response, "ERROR:", 6) == 0) {
        show_error_dialog(response, data);
        return;
    }
    
    // Kiểm tra và bỏ prefix QUESTIONS:
    char *practice_data = response;
    if (strncmp(response, "QUESTIONS:", 10) == 0) {
        practice_data = response + 10;
    }

    printf("practice_data: %s\n", practice_data);
    // Hiển thị giao diện làm bài với time_limit cố định 30 phút
    show_practice_interface(data, practice_data);
}

static void show_practice(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Chế độ luyện tập",
                                                  GTK_WINDOW(data->main_window),
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content_area, 20);
    gtk_widget_set_margin_end(content_area, 20);
    gtk_widget_set_margin_top(content_area, 20);
    gtk_widget_set_margin_bottom(content_area, 20);

    GtkWidget *message = gtk_label_new("Ấn bắt đầu để bắt đầu chế độ luyện tập.\nChế độ này sẽ có 20 câu hỏi ngẫu nhiên trong 30 phút");
    gtk_box_append(GTK_BOX(content_area), message);

    GtkWidget *start_button = gtk_button_new_with_label("Bắt đầu");
    gtk_box_append(GTK_BOX(content_area), start_button);
    
    g_signal_connect(start_button, "clicked", G_CALLBACK(start_practice), data);
    g_signal_connect_swapped(start_button, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    
    gtk_window_present(GTK_WINDOW(dialog));
}
static void show_score_list(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Lấy room_id từ button data
    int room_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "room_id"));
    
    // Tạo cửa sổ mới
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Danh sách điểm thi",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Đóng",
                                                   GTK_RESPONSE_CLOSE,
                                                   NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1000, 600);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Tạo box chính để chứa tất cả các thành phần
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 10);
    gtk_widget_set_margin_end(main_box, 10);
    gtk_widget_set_margin_top(main_box, 10);
    gtk_widget_set_margin_bottom(main_box, 10);
    
    // Tạo list box để hiển thị danh sách điểm
    GtkWidget *list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box), GTK_SELECTION_NONE);
    
    // Tạo header
    GtkWidget *header_row = gtk_list_box_row_new();
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(header_box, 10);
    gtk_widget_set_margin_end(header_box, 10);
    gtk_widget_set_margin_top(header_box, 5);
    gtk_widget_set_margin_bottom(header_box, 5);
    
    const char *headers[] = {"Username", "Score", "Start Time", "End Time"};
    int widths[] = {200, 100, 200, 200};
    
    for (int i = 0; i < 4; i++) {
        GtkWidget *label = gtk_label_new(headers[i]);
        gtk_widget_set_size_request(label, widths[i], -1);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_box_append(GTK_BOX(header_box), label);
    }
    
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(header_row), header_box);
    gtk_list_box_append(list_box, header_row);
    
    // Gửi request lấy danh sách điểm với room_id
    char request[MAX_BUFFER];
    snprintf(request, sizeof(request), "GET_SCORE_LIST|%d", room_id);
    send(data->sock, request, strlen(request), 0);
    
    char response[MAX_BUFFER];
    recv(data->sock, response, MAX_BUFFER, 0);
    
    if (strncmp(response, "SCORE_LIST:", 11) == 0) {
        char *score_list = response + 11;
        char *score = strtok(score_list, "|");
        
        while (score != NULL) {
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_set_margin_start(box, 10);
            gtk_widget_set_margin_end(box, 10);
            gtk_widget_set_margin_top(box, 5);
            gtk_widget_set_margin_bottom(box, 5);
            
            char username[50];
            char score_str[20];
            char start_time[50];
            char end_time[50];
            
            sscanf(score, "%[^,],%[^,],%[^,],%[^\n]", username, score_str, start_time, end_time);
            
            GtkWidget *username_label = gtk_label_new(username);
            GtkWidget *score_label = gtk_label_new(score_str);
            GtkWidget *start_label = gtk_label_new(start_time);
            GtkWidget *end_label = gtk_label_new(end_time);
            
            gtk_widget_set_size_request(username_label, widths[0], -1);
            gtk_widget_set_size_request(score_label, widths[1], -1);
            gtk_widget_set_size_request(start_label, widths[2], -1);
            gtk_widget_set_size_request(end_label, widths[3], -1);
            
            gtk_label_set_xalign(GTK_LABEL(username_label), 0);
            gtk_label_set_xalign(GTK_LABEL(score_label), 0);
            gtk_label_set_xalign(GTK_LABEL(start_label), 0);
            gtk_label_set_xalign(GTK_LABEL(end_label), 0);
            
            gtk_box_append(GTK_BOX(box), username_label);
            gtk_box_append(GTK_BOX(box), score_label);
            gtk_box_append(GTK_BOX(box), start_label);
            gtk_box_append(GTK_BOX(box), end_label);
            
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
            gtk_list_box_append(list_box, row);
            
            score = strtok(NULL, "|");
        }
    }
    
    // Tạo scrolled window và thêm list box vào
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list_box);
    gtk_widget_set_vexpand(scrolled, TRUE);
    
    gtk_box_append(GTK_BOX(main_box), scrolled);
    gtk_box_append(GTK_BOX(content_area), main_box);
    
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}
typedef struct {
    GtkWidget *dialog;
    GtkWidget *list_box;
    int room_id;
    int sock;
    AppData *app_data;
} QuestionsDialogData;

static void save_questions(GtkButton *button, gpointer user_data) {
    QuestionsDialogData *dialog_data = (QuestionsDialogData *)user_data;
    
    // Tạo buffer cho request
    char request[MAX_BUFFER];
    snprintf(request, sizeof(request), "UPDATE_ROOM_QUESTIONS|%d", dialog_data->room_id);
    
    // Lặp qua tất cả các row trong list box
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(dialog_data->list_box));
    while (child != NULL) {
        if (GTK_IS_LIST_BOX_ROW(child)) {
            GtkWidget *box = gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(child));
            GtkWidget *check_button = gtk_widget_get_first_child(box);
            
            // Lấy question_id và trạng thái của checkbox
            int question_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(check_button), "question_id"));
            int is_selected = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_button));
            
            // Thêm vào request
            char temp[32];
            snprintf(temp, sizeof(temp), "|%d,%d", question_id, is_selected);
            strcat(request, temp);
        }
        child = gtk_widget_get_next_sibling(child);
    }
    
    // Gửi request lên server
    send(dialog_data->sock, request, strlen(request), 0);
    
    // Nhận response
    char response[MAX_BUFFER];
    recv(dialog_data->sock, response, MAX_BUFFER, 0);
    
    // Hiển thị thông báo kết quả
    GtkWidget *message_dialog;
    if (strncmp(response, "SUCCESS:",8) == 0) {
        message_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog_data->dialog),
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_INFO,
                                              GTK_BUTTONS_OK,
                                              "Lưu thành công!");
    } else {
        message_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog_data->dialog),
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_OK,
                                              "Lỗi: %s", response + 6);
    }
    
    gtk_window_present(GTK_WINDOW(message_dialog));
    g_signal_connect(message_dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
}

static void handle_questions_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    QuestionsDialogData *dialog_data = (QuestionsDialogData *)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        save_questions(NULL, dialog_data);
    }
    
    g_free(dialog_data);
    gtk_window_destroy(GTK_WINDOW(dialog));
}


static void show_questions_list(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    int room_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "room_id"));
    
    // Tạo dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Danh sách câu hỏi",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Lưu",
                                                   GTK_RESPONSE_ACCEPT,
                                                   "_Đóng",
                                                   GTK_RESPONSE_CLOSE,
                                                   NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 600);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content_area, 10);
    gtk_widget_set_margin_end(content_area, 10);
    gtk_widget_set_margin_top(content_area, 10);
    gtk_widget_set_margin_bottom(content_area, 10);
    
    // Tạo list box
    GtkWidget *list_box = gtk_list_box_new();
    
    // Tạo QuestionsDialogData
    QuestionsDialogData *dialog_data = g_new(QuestionsDialogData, 1);
    dialog_data->dialog = dialog;
    dialog_data->list_box = list_box;
    dialog_data->room_id = room_id;
    dialog_data->sock = data->sock;
    dialog_data->app_data = data;
    
    // Lấy danh sách câu hỏi từ server
    char request[MAX_BUFFER];
    snprintf(request, sizeof(request), "GET_ROOM_QUESTIONS|%d", room_id);
    send(data->sock, request, strlen(request), 0);
    
    char response[MAX_BUFFER];
    recv(data->sock, response, MAX_BUFFER, 0);
    
    if (strncmp(response, "QUESTIONS:", 10) == 0) {
        char *questions = response + 10;
        char *question = strtok(questions, "|");
        
        while (question != NULL) {
            int id, difficulty, is_selected;
            char q_text[256];
            sscanf(question, "%d,%[^,],%d,%d", &id, q_text, &difficulty, &is_selected);
            
            // Tạo row mới
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            
            // Tạo checkbox
            GtkWidget *check_button = gtk_check_button_new();
            g_object_set_data(G_OBJECT(check_button), "question_id", GINT_TO_POINTER(id));
            gtk_check_button_set_active(GTK_CHECK_BUTTON(check_button), is_selected);
            
            // Tạo label cho câu hỏi
            char label_text[512];
            snprintf(label_text, sizeof(label_text), "%s (Độ khó: %d)", q_text, difficulty);
            GtkWidget *label = gtk_label_new(label_text);
            gtk_label_set_wrap(GTK_LABEL(label), TRUE);
            gtk_widget_set_hexpand(label, TRUE);
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            
            // Thêm widgets vào box
            gtk_box_append(GTK_BOX(box), check_button);
            gtk_box_append(GTK_BOX(box), label);
            
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
            gtk_list_box_append(GTK_LIST_BOX(list_box), row);
            
            question = strtok(NULL, "|");
        }
    }
    
    // Thêm list box vào scrolled window
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list_box);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 400);
    
    // Thêm scrolled window vào content area
    gtk_box_append(GTK_BOX(content_area), scrolled);
    
    // Kết nối signals
    g_signal_connect(dialog, "response", G_CALLBACK(handle_questions_dialog_response), dialog_data);
    
    gtk_window_present(GTK_WINDOW(dialog));
}


static void send_add_room_data(GtkButton *button, gpointer user_data) {
    AddRoomData *data = (AddRoomData *)user_data;
    
    const char *room_name = gtk_editable_get_text(GTK_EDITABLE(data->name_entry));
    const char *time_limit_str = gtk_editable_get_text(GTK_EDITABLE(data->time_limit_entry));
    const char *start_time = gtk_editable_get_text(GTK_EDITABLE(data->start_time_entry));
    const char *end_time = gtk_editable_get_text(GTK_EDITABLE(data->end_time_entry));
    
    // Kiểm tra dữ liệu đầu vào
    if (strlen(room_name) == 0 || strlen(time_limit_str) == 0 || 
        strlen(start_time) == 0 || strlen(end_time) == 0) {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Vui lòng điền đầy đủ thông tin!");
        return;
    }
    
    int time_limit = atoi(time_limit_str);
    if (time_limit <= 0) {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Thời gian không hợp lệ!");
        return;
    }
    
    // Chuẩn bị message
    char message[MAX_BUFFER];
    snprintf(message, sizeof(message), "ADD_EXAM_ROOM|%s|%d|%s|%s", 
             room_name, time_limit, start_time, end_time);
    
    // Gửi đến server
    send(data->sock, message, strlen(message), 0);
    
    // Nhận response
    char response[MAX_BUFFER];
    recv(data->sock, response, sizeof(response), 0);
    
    // Xử lý response
    if (strstr(response, "SUCCESS") != NULL) {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Thêm phòng thi thành công!");
        // Đóng cửa sổ sau 1.5 giây
        g_timeout_add(1500, (GSourceFunc)gtk_window_destroy, data->window);
    } else {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Thêm phòng thi thất bại!");
    }
}
// Hàm mở cửa sổ thêm phòng thi
// Sửa lại hàm add_new_room

static void handle_add_room_response(GtkDialog *dialog, int response_id, gpointer user_data) {
    AddRoomData *data = (AddRoomData *)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        // Xử lý thêm phòng thi
        const char *room_name = gtk_editable_get_text(GTK_EDITABLE(data->name_entry));
        const char *time_limit_str = gtk_editable_get_text(GTK_EDITABLE(data->time_limit_entry));
        const char *start_time = gtk_editable_get_text(GTK_EDITABLE(data->start_time_entry));
        const char *end_time = gtk_editable_get_text(GTK_EDITABLE(data->end_time_entry));
        
        // Kiểm tra dữ liệu đầu vào
        if (strlen(room_name) == 0 || strlen(time_limit_str) == 0 || 
            strlen(start_time) == 0 || strlen(end_time) == 0) {
            gtk_label_set_text(GTK_LABEL(data->status_label), "Vui lòng điền đầy đủ thông tin!");
            return;
        }
        
        int time_limit = atoi(time_limit_str);
        if (time_limit <= 0) {
            gtk_label_set_text(GTK_LABEL(data->status_label), "Thời gian không hợp lệ!");
            return;
        }
        
        // Chuẩn bị message
        char message[MAX_BUFFER];
        snprintf(message, sizeof(message), "ADD_EXAM_ROOM|%s|%d|%s|%s", 
                 room_name, time_limit, start_time, end_time);
        
        // Gửi đến server
        send(data->sock, message, strlen(message), 0);
        
        // Nhận response
        char response[MAX_BUFFER];
        recv(data->sock, response, sizeof(response), 0);
        
        // Xử lý response
        if (strstr(response, "SUCCESS") != NULL) {
            gtk_window_destroy(GTK_WINDOW(dialog));
        } else {
            gtk_label_set_text(GTK_LABEL(data->status_label), "Thêm phòng thi thất bại!");
        }
    }
    
    if (response_id == GTK_RESPONSE_CANCEL) {
        gtk_window_destroy(GTK_WINDOW(dialog));
    }
    
    g_free(data);
}


static void add_new_room(GtkButton *button, AppData *data) {
    // Tạo dialog mới
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Thêm phòng thi mới",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Hủy",
                                                   GTK_RESPONSE_CANCEL,
                                                   "_Thêm",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 500);
    
    // Lấy content area của dialog
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Tạo layout box
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 200);
    gtk_widget_set_margin_end(box, 200);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    
    // Tạo các widget form
    GtkWidget *name_label = gtk_label_new("Tên phòng thi:");
    gtk_widget_set_halign(name_label, GTK_ALIGN_START);
    GtkWidget *name_entry = gtk_entry_new();
    
    GtkWidget *time_limit_label = gtk_label_new("Thời gian làm bài (phút):");
    gtk_widget_set_halign(time_limit_label, GTK_ALIGN_START);
    GtkWidget *time_limit_entry = gtk_entry_new();
    
    GtkWidget *start_time_label = gtk_label_new("Thời gian bắt đầu (YYYY-MM-DD HH:MM:SS):");
    gtk_widget_set_halign(start_time_label, GTK_ALIGN_START);
    GtkWidget *start_time_entry = gtk_entry_new();
    
    GtkWidget *end_time_label = gtk_label_new("Thời gian kết thúc (YYYY-MM-DD HH:MM:SS):");
    gtk_widget_set_halign(end_time_label, GTK_ALIGN_START);
    GtkWidget *end_time_entry = gtk_entry_new();
    
    GtkWidget *status_label = gtk_label_new("");
    
    // Tạo AddRoomData structure
    AddRoomData *room_data = g_new(AddRoomData, 1);
    room_data->name_entry = name_entry;
    room_data->time_limit_entry = time_limit_entry;
    room_data->start_time_entry = start_time_entry;
    room_data->end_time_entry = end_time_entry;
    room_data->status_label = status_label;
    room_data->sock = data->sock;
    room_data->window = dialog;
    
    // Thêm widgets vào box
    gtk_box_append(GTK_BOX(box), name_label);
    gtk_box_append(GTK_BOX(box), name_entry);
    gtk_box_append(GTK_BOX(box), time_limit_label);
    gtk_box_append(GTK_BOX(box), time_limit_entry);
    gtk_box_append(GTK_BOX(box), start_time_label);
    gtk_box_append(GTK_BOX(box), start_time_entry);
    gtk_box_append(GTK_BOX(box), end_time_label);
    gtk_box_append(GTK_BOX(box), end_time_entry);
    gtk_box_append(GTK_BOX(box), status_label);
    
    // Thêm box vào content area
    gtk_box_append(GTK_BOX(content_area), box);
    
    // Kết nối signal cho dialog response
    g_signal_connect(dialog, "response", G_CALLBACK(handle_add_room_response), room_data);
    
    // Hiển thị dialog
    gtk_window_present(GTK_WINDOW(dialog));
}

static void show_room_list(GtkButton *button, gpointer user_data) {
   AppData *data = (AppData *)user_data;
    
    // Tạo cửa sổ mới
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Danh sách phòng thi",
                                                   GTK_WINDOW(data->main_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Đóng",
                                                   GTK_RESPONSE_CLOSE,
                                                   NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1000, 600);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Tạo box chính để chứa tất cả các thành phần
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 10);
    gtk_widget_set_margin_end(main_box, 10);
    gtk_widget_set_margin_top(main_box, 10);
    gtk_widget_set_margin_bottom(main_box, 10);
    
    // Tạo list box để hiển thị danh sách phòng
    GtkListBox *list_box = GTK_LIST_BOX(gtk_list_box_new());
    
    // Gửi yêu cầu lấy danh sách phòng từ server
    char request[MAX_BUFFER] = "GET_EXAM_ROOMS_TEACHER";
    send(data->sock, request, strlen(request), 0);
    
    char response[MAX_BUFFER];
    recv(data->sock, response, MAX_BUFFER, 0);
    
    // Kiểm tra message header
    if (strncmp(response, "EXAM_ROOMS_LIST:", 16) != 0) {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Lỗi khi lấy danh sách phòng thi");
        return;
    }
    
    // Bỏ qua header để lấy dữ liệu
    char *rooms_data = response + 16;
    
    // Phân tích response và tạo các row cho list box
    char *room = strtok(rooms_data, "|");
    while (room != NULL) {
        int room_id;
        char room_name[100];
        char status[20];
        int time_limit;
        char start_time[50];
        char end_time[50];
        
        sscanf(room, "%d,%[^,],%[^,],%d,%[^,],%[^,]", 
               &room_id, room_name, status, &time_limit, start_time, end_time);
        
        // Tạo row cho mỗi phòng thi
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
            gtk_widget_set_margin_start(box, 10);
            gtk_widget_set_margin_end(box, 10);
            gtk_widget_set_margin_top(box, 5);
            gtk_widget_set_margin_bottom(box, 5);

        // Tạo và định dạng các label
        GtkWidget *name_label = gtk_label_new(room_name);
        gtk_widget_set_size_request(name_label, 200, -1);
        
        GtkWidget *status_label = gtk_label_new(status);
        gtk_widget_set_size_request(status_label, 100, -1);
        
        // Sửa lỗi gtk_label_new_printf
        char time_text[32];
        snprintf(time_text, sizeof(time_text), "%d phút", time_limit);
        GtkWidget *time_label = gtk_label_new(time_text);
        gtk_widget_set_size_request(time_label, 80, -1);
        
            GtkWidget *start_label = gtk_label_new(start_time);
        gtk_widget_set_size_request(start_label, 150, -1);
        
            GtkWidget *end_label = gtk_label_new(end_time);
        gtk_widget_set_size_request(end_label, 150, -1);
        
        GtkWidget *show_score_button = gtk_button_new_with_label("See Score");
        GtkWidget *show_question_button = gtk_button_new_with_label("Question List");
        
        // Thêm các widget vào box
        gtk_box_append(GTK_BOX(box), name_label);
        gtk_box_append(GTK_BOX(box), status_label);
        gtk_box_append(GTK_BOX(box), time_label);
            gtk_box_append(GTK_BOX(box), start_label);
        gtk_box_append(GTK_BOX(box), end_label);
        gtk_box_append(GTK_BOX(box), show_score_button);
        gtk_box_append(GTK_BOX(box), show_question_button);
        
        // Lưu room_id vào button để sử dụng khi click
        g_object_set_data(G_OBJECT(show_score_button), "room_id", GINT_TO_POINTER(room_id));
        g_signal_connect(show_score_button, "clicked", G_CALLBACK(show_score_list), data);
        
        // Thêm signal handler cho nút Question List
        g_object_set_data(G_OBJECT(show_question_button), "room_id", GINT_TO_POINTER(room_id));
        g_signal_connect(show_question_button, "clicked", G_CALLBACK(show_questions_list), data);
        
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        gtk_list_box_append(list_box, GTK_LIST_BOX_ROW(row));
        
        room = strtok(NULL, "|");
    }
    
    // Thêm list box vào main_box
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(list_box));
    
    // Tạo box cho nút thêm phòng thi
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(button_box, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    
    // Tạo nút thêm phòng thi
    GtkWidget *add_room_button = gtk_button_new_with_label("Thêm phòng thi mới");
    gtk_widget_set_size_request(add_room_button, 200, 40);
    g_signal_connect(add_room_button, "clicked", G_CALLBACK(add_new_room),data);
    
    // Thêm nút vào button_box
    gtk_box_append(GTK_BOX(button_box), add_room_button);
    
    // Thêm button_box vào main_box
    gtk_box_append(GTK_BOX(main_box), button_box);
    
    // Thêm main_box vào content_area
    gtk_box_append(GTK_BOX(content_area), main_box);
    
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

// Hàm để load danh sách câu hỏi từ server
static void load_questions(GtkListBox *list_box, AppData *data) {
    send(data->sock, "GET_QUESTIONS", strlen("GET_QUESTIONS"), 0);
    
    char response[MAX_BUFFER];
    recv(data->sock, response, MAX_BUFFER, 0);
    
    // Xóa tất cả các row hiện tại
    GtkWidget *child;
    while ((child = gtk_list_box_get_row_at_index(list_box, 0)) != NULL) {
        gtk_list_box_remove(list_box, child);
    }
    
    if (strncmp(response, "QUESTIONS:", 10) == 0) {
        char *questions_data = response + 10;
        char *question = strtok(questions_data, "|");
        
        while (question != NULL) {
            // Parse thông tin câu hỏi
            int id;
            char text[512];
            int difficulty;
            sscanf(question, "%d,%[^,],%d", &id, text, &difficulty);
            
            // Tạo box chứa thông tin câu hỏi
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_set_margin_start(hbox, 10);
            gtk_widget_set_margin_end(hbox, 10);
            gtk_widget_set_margin_top(hbox, 5);
            gtk_widget_set_margin_bottom(hbox, 5);
            
            // Label hiển thị thông tin
            char display_text[600];
            snprintf(display_text, sizeof(display_text), "ID: %d - %s (Độ khó: %d)", 
                    id, text, difficulty);
            GtkWidget *label = gtk_label_new(display_text);
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_widget_set_hexpand(label, TRUE);
            
            // Nút sửa và xóa
            GtkWidget *edit_btn = gtk_button_new_with_label("Sửa");
            GtkWidget *delete_btn = gtk_button_new_with_label("Xóa");
            
            // Thêm các widget vào hbox
            gtk_box_append(GTK_BOX(hbox), label);
            gtk_box_append(GTK_BOX(hbox), edit_btn);
            gtk_box_append(GTK_BOX(hbox), delete_btn);
            
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
            gtk_list_box_append(list_box, GTK_LIST_BOX_ROW(row));
            DeleteUpdateData *del_update_data = g_new(DeleteUpdateData, 1);
            del_update_data->window = data->main_window;
            del_update_data->sock = data->sock;
            del_update_data->app_data = data;
            del_update_data->question_id = id;
            
            // Kết nối signals cho các nút
            g_signal_connect(edit_btn, "clicked", G_CALLBACK(show_update_question), 
                           del_update_data);
            g_signal_connect(delete_btn, "clicked", G_CALLBACK(delete_question), 
                           del_update_data);
            
            question = strtok(NULL, "|");
        }
    }
}

static void handle_update_question_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        Question *question = (Question *)user_data;
        const char* question_text = gtk_editable_get_text(GTK_EDITABLE(question->question_text));
        int difficulty = gtk_combo_box_get_active(GTK_COMBO_BOX(question->difficulty));
        
        char request[MAX_BUFFER];
        snprintf(request, sizeof(request), "UPDATE_QUESTION|%d|%s|%d", 
                question->question_id, question_text, difficulty);

        for (int i = 0; i < 4; i++) {
            const char* choice_text = gtk_editable_get_text(GTK_EDITABLE(question->choices[i].choice_text));
            int is_correct = gtk_check_button_get_active(GTK_CHECK_BUTTON(question->choices[i].is_correct));
            char choice_data[256];
            snprintf(choice_data, sizeof(choice_data), "|%d|%s|%d", 
                    question->choices[i].choice_id, choice_text, is_correct);
            strcat(request, choice_data);
        }

        send(question->sock, request, strlen(request), 0);
        printf("request: %s\n", request);
        
        char response[MAX_BUFFER];
        recv(question->sock, response, MAX_BUFFER, 0);
        
        if (strncmp(response, "ERROR:", 6) == 0) {
            show_error_dialog(response, question->app_data);
        } else if (strncmp(response, "SUCCESS:", 8) == 0) {
            show_success_dialog(response, question->app_data);
        }
    }
    
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_free(user_data);
}

static void show_update_question(GtkButton *button, gpointer user_data) {
    DeleteUpdateData *update_data = (DeleteUpdateData *)user_data;
    
    // Gửi yêu cầu lấy thông tin câu hỏi
    char request[32];
    snprintf(request, sizeof(request), "GET_QUESTION|%d", update_data->question_id);
    send(update_data->sock, request, strlen(request), 0);
    
    char response[MAX_BUFFER];
    recv(update_data->sock, response, MAX_BUFFER, 0);
    
    if (strncmp(response, "ERROR:", 6) == 0) {
        show_error_dialog(response, update_data->app_data);
        return;
    }
    
    if (strncmp(response, "QUESTION:", 9) == 0) {
        printf("response: %s\n", response);
        char *q_data = response + 9;
        int question_id = atoi(strtok(q_data, ";"));
        char *question_text = strtok(NULL, ";");
        char *difficulty = atoi(strtok(NULL, ";"));
        int choice_id[4];
        char *choices[4];
        int *is_correct[4];
        for (int i = 0; i < 4; i++) {
            choice_id[i] = atoi(strtok(NULL, ";"));
            choices[i] = strtok(NULL, ";");
            is_correct[i] = atoi(strtok(NULL, ";"));
        }
        
        // Tạo dialog
        GtkWidget *dialog = gtk_dialog_new_with_buttons(
            "Cập nhật câu hỏi",
            GTK_WINDOW(update_data->window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "_Hủy",
            GTK_RESPONSE_CANCEL,
            "_Lưu",
            GTK_RESPONSE_ACCEPT,
            NULL);

        // Tạo container chính
        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        gtk_widget_set_margin_start(content, 20);
        gtk_widget_set_margin_end(content, 20);
        gtk_widget_set_margin_top(content, 20);
        gtk_widget_set_margin_bottom(content, 20);

        // Box cho câu hỏi
        GtkWidget *question_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        GtkWidget *q_label = gtk_label_new("Câu hỏi:");
        GtkWidget *q_entry = gtk_entry_new();
        gtk_editable_set_text(GTK_EDITABLE(q_entry), (const char*)question_text);

        // Box cho độ khó
        GtkWidget *diff_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        GtkWidget *diff_label = gtk_label_new("Độ khó:");
        GtkWidget *diff_combo = gtk_combo_box_text_new();
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "Dễ");
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "Trung bình");
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "Khó");
        gtk_combo_box_set_active(GTK_COMBO_BOX(diff_combo), difficulty - 1);

        // Box cho các đáp án
        GtkWidget *choices_label = gtk_label_new("Các đáp án:");
        GtkWidget *choices_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

        // Tạo entries và checkboxes cho 4 đáp án
        GtkWidget *choice_entries[4];
        GtkWidget *correct_checks[4];

        for (int i = 0; i < 4; i++) {
            GtkWidget *choice_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
            
            choice_entries[i] = gtk_entry_new();
            gtk_editable_set_text(GTK_EDITABLE(choice_entries[i]), 
                            (const char*)choices[i]);
            
            correct_checks[i] = gtk_check_button_new_with_label("Đáp án đúng");
            gtk_check_button_set_active(GTK_CHECK_BUTTON(correct_checks[i]), 
                                    is_correct[i]);
            
            gtk_box_append(GTK_BOX(choice_box), choice_entries[i]);
            gtk_box_append(GTK_BOX(choice_box), correct_checks[i]);
            gtk_box_append(GTK_BOX(choices_box), choice_box);
        }

        // Thêm các widget vào dialog
        gtk_box_append(GTK_BOX(question_box), q_label);
        gtk_box_append(GTK_BOX(question_box), q_entry);
        gtk_box_append(GTK_BOX(diff_box), diff_label);
        gtk_box_append(GTK_BOX(diff_box), diff_combo);
        gtk_box_append(GTK_BOX(content), question_box);
        gtk_box_append(GTK_BOX(content), diff_box);
        gtk_box_append(GTK_BOX(content), choices_label);
        gtk_box_append(GTK_BOX(content), choices_box);

        gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 600);

        // Tạo struct để lưu thông tin câu hỏi
        Question *updated_question = g_new(Question, 1);
        updated_question->question_id = question_id;
        updated_question->question_text = q_entry;
        updated_question->difficulty = diff_combo;

        for (int i = 0; i < 4; i++) {
            updated_question->choices[i].choice_id = choice_id[i];
            updated_question->choices[i].choice_text = choice_entries[i];
            updated_question->choices[i].is_correct = correct_checks[i];
        }

        updated_question->window = update_data->window;
        updated_question->sock = update_data->sock;
        updated_question->app_data = update_data->app_data;

        // Kết nối signal cho dialog
        g_signal_connect(dialog, "response", G_CALLBACK(handle_update_question_response), updated_question);

        gtk_window_present(GTK_WINDOW(dialog));
    }
}

// Hàm hiển thị dialog thêm/sửa câu hỏi
static void show_question_dialog(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Thêm câu hỏi mới",
        GTK_WINDOW(data->main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Lưu",
        GTK_RESPONSE_ACCEPT,
        "_Hủy",
        GTK_RESPONSE_CANCEL,
        NULL);
        
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content, 20);
    gtk_widget_set_margin_end(content, 20);
    gtk_widget_set_margin_top(content, 20);
    gtk_widget_set_margin_bottom(content, 20);
    
    // Box cho câu hỏi
    GtkWidget *question_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *q_label = gtk_label_new("Câu hỏi:");
    gtk_label_set_xalign(GTK_LABEL(q_label), 0);

    GtkWidget *q_entry = gtk_entry_new();

    // Combobox độ khó
    GtkWidget *diff_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *diff_label = gtk_label_new("Độ khó:");
    GtkWidget *diff_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "Dễ");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "Trung bình");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "Khó");
    gtk_combo_box_set_active(GTK_COMBO_BOX(diff_combo), 0);

    // Box cho các đáp án
    GtkWidget *choices_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *choices_label = gtk_label_new("Các đáp án:");
    gtk_label_set_xalign(GTK_LABEL(choices_label), 0);
    
    GtkWidget *choice_entries[4];
    GtkWidget *correct_checks[4];
    
    for (int i = 0; i < 4; i++) {
        GtkWidget *choice_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        choice_entries[i] = gtk_entry_new();
        correct_checks[i] = gtk_check_button_new_with_label("Đáp án đúng");
        gtk_box_append(GTK_BOX(choice_box), choice_entries[i]);
        gtk_box_append(GTK_BOX(choice_box), correct_checks[i]);
        gtk_box_append(GTK_BOX(choices_box), choice_box);
    }
    
    // Thêm tất cả vào dialog
    gtk_box_append(GTK_BOX(question_box), q_label);
    gtk_box_append(GTK_BOX(question_box), q_entry);
    gtk_box_append(GTK_BOX(diff_box), diff_label);
    gtk_box_append(GTK_BOX(diff_box), diff_combo);
    gtk_box_append(GTK_BOX(content), question_box);
    gtk_box_append(GTK_BOX(content), diff_box);
    gtk_box_append(GTK_BOX(content), choices_label);
    gtk_box_append(GTK_BOX(content), choices_box);
    
    gtk_box_append(GTK_BOX(content), question_box);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1200, 800);

    Question *question = g_new(Question, 1);
    // đưa hết data đọc được vào question
    question->question_id = -1;
    question->question_text = q_entry;
    question->difficulty = diff_combo;

    for (int i = 0; i < 4; i++) {
        question->choices[i].choice_id = -1;
        question->choices[i].choice_text = choice_entries[i];
        question->choices[i].is_correct = correct_checks[i];
    }

    question->window = GTK_WINDOW(dialog);
    question->sock = data->sock;
    question->app_data = data;

    g_signal_connect(dialog, "response", G_CALLBACK(handle_question_dialog_response), question);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

// Hàm xử lý khi nhấn nút xóa câu hỏi
static void delete_question(GtkButton *button, gpointer user_data) {
    DeleteUpdateData *del_data = (DeleteUpdateData *)user_data;
    
    // Hiển thị dialog xác nhận
    GtkWidget *confirm_dialog = gtk_dialog_new_with_buttons(
        "Xác nhận xóa",
        GTK_WINDOW(del_data->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Hủy",
        GTK_RESPONSE_CANCEL,
        "_Xóa",
        GTK_RESPONSE_ACCEPT,
        NULL);
        
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(confirm_dialog));
    GtkWidget *label = gtk_label_new("Bạn có chắc chắn muốn xóa câu hỏi này?");
    gtk_box_append(GTK_BOX(content), label);
        
    g_signal_connect(confirm_dialog, "response", G_CALLBACK(handle_delete_response), del_data);
    
    gtk_window_present(GTK_WINDOW(confirm_dialog));
}

// Thêm hàm callback mới để xử lý response của dialog xóa
static void handle_delete_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    DeleteUpdateData *del_data = (DeleteUpdateData *)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        // Gửi yêu cầu xóa câu hỏi
        char request[32];
        snprintf(request, sizeof(request), "DELETE_QUESTION|%d", del_data->question_id);
        send(del_data->app_data->sock, request, strlen(request), 0);
        
        char response[MAX_BUFFER];
        recv(del_data->app_data->sock, response, MAX_BUFFER, 0);
        
        if (strncmp(response, "ERROR:", 6) == 0) {
            show_error_dialog(response, del_data->app_data);
        } else if (strncmp(response, "SUCCESS:", 8) == 0) {
            show_success_dialog(response, del_data->app_data);
        }
    }
    
    g_free(del_data);
    gtk_window_destroy(GTK_WINDOW(dialog));
}

// Hàm xử lý khi nhấn nút Lưu trong dialog thêm/sửa câu hỏi
static void handle_question_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        Question *question = (Question *)user_data;
        const char* question_text = gtk_editable_get_text(GTK_EDITABLE(question->question_text));
        int difficulty = gtk_combo_box_get_active(GTK_COMBO_BOX(question->difficulty));
        char request[MAX_BUFFER];
        snprintf(request, sizeof(request), "ADD_QUESTION|%s|%d", question_text, difficulty);

        for (int i = 0; i < 4; i++) {
            const char* choice_text = gtk_editable_get_text(GTK_EDITABLE(question->choices[i].choice_text));
            int is_correct = gtk_check_button_get_active(GTK_CHECK_BUTTON(question->choices[i].is_correct));
            const char* choice_data[256];
            snprintf(choice_data, sizeof(choice_data), "|%s|%d", choice_text, is_correct);
            strcat(request, choice_data);
        }
        
        // Gửi yêu cầu lên server
        send(question->sock, request, strlen(request), 0);
        printf("Send request: %s to server %d\n", request, question->sock);
        
        char response[MAX_BUFFER];
        recv(question->sock, response, MAX_BUFFER, 0);
        
        if (strncmp(response, "ERROR:", 6) == 0) {
            show_error_dialog(response, question->app_data);
        } else if (strncmp(response, "SUCCESS:", 8) == 0) {
            show_success_dialog(response, question->app_data);
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

// Hàm chính để hiển thị CRUD câu hỏi
static void show_crud_question(GtkButton *button, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Quản lý câu hỏi",
        GTK_WINDOW(data->main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Đóng",
        GTK_RESPONSE_CLOSE,
        NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1600, 800);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content, 20);
    gtk_widget_set_margin_end(content, 20);
    gtk_widget_set_margin_top(content, 20);
    gtk_widget_set_margin_bottom(content, 20);
    
    // Header với nút thêm mới
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *add_btn = gtk_button_new_with_label("Thêm câu hỏi mới");
    gtk_widget_set_halign(add_btn, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(header_box), add_btn);
    
    // Danh sách câu hỏi
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 680);
    GtkWidget *list_box = gtk_list_box_new();
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list_box);
    
    // Thêm vào dialog
    gtk_box_append(GTK_BOX(content), header_box);
    gtk_box_append(GTK_BOX(content), scrolled);
    
    // Load danh sách câu hỏi
    load_questions(GTK_LIST_BOX(list_box), data);
    
    // Kết nối signals
    g_signal_connect(add_btn, "clicked", G_CALLBACK(show_question_dialog), data);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

static void create_student_interface(AppData *data) {
    // Hide login window
    gtk_widget_hide(data->window);
    
    // Create new window for student interface
    GtkWidget *window = gtk_application_window_new(gtk_window_get_application(GTK_WINDOW(data->window)));
    data->main_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Student Dashboard");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
    
    // Create main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    
    // Create header with logout button
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    GtkWidget *logout_button = gtk_button_new_with_label("Đăng xuất");
    gtk_box_append(GTK_BOX(header_box), spacer);
    gtk_box_append(GTK_BOX(header_box), logout_button);
    
    // Create grid for main options
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    
    // Create three main option buttons
    GtkWidget *exam_button = gtk_button_new_with_label("Danh sách phòng thi");
    GtkWidget *history_button = gtk_button_new_with_label("Thống kê lịch sử thi");
    GtkWidget *practice_button = gtk_button_new_with_label("Chế độ luyện tập");

    // Set size for buttons
    gtk_widget_set_size_request(exam_button, 1000, 200);
    gtk_widget_set_size_request(history_button, 1000, 200);
    gtk_widget_set_size_request(practice_button, 1000, 200);
    
    // Add buttons to grid
    gtk_grid_attach(GTK_GRID(grid), exam_button, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), history_button, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), practice_button, 0, 2, 1, 1);
    
    // Connect signals
    g_signal_connect(logout_button, "clicked", G_CALLBACK(logout), data);
    g_signal_connect(exam_button, "clicked", G_CALLBACK(show_exam_list), data);
    g_signal_connect(history_button, "clicked", G_CALLBACK(show_history), data);
    g_signal_connect(practice_button, "clicked", G_CALLBACK(show_practice), data);
    
    // Add all components to main box
    gtk_box_append(GTK_BOX(main_box), header_box);
    gtk_box_append(GTK_BOX(main_box), grid);
    
    // Set up window
    gtk_window_set_child(GTK_WINDOW(window), main_box);
    gtk_widget_show(window);
}



static void create_teacher_interface(AppData *data) {
    // Hide login window
    gtk_widget_hide(data->window);
    
    // Create new window for student interface
    GtkWidget *window = gtk_window_new();
    data->main_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Teacher Dashboard");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
    
    // Create main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    
    // Create header with logout button
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    GtkWidget *logout_button = gtk_button_new_with_label("Đăng xuất");
    gtk_box_append(GTK_BOX(header_box), spacer);
    gtk_box_append(GTK_BOX(header_box), logout_button);
    
    // Create grid for main options
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    
    // Create three main option buttons
    GtkWidget *room_button = gtk_button_new_with_label("Quản lý phòng thi");
    GtkWidget *grade_button = gtk_button_new_with_label("Quản lý câu hỏi");
    
    // Set size for buttons
    gtk_widget_set_size_request(room_button, 400, 400);
    gtk_widget_set_size_request(grade_button, 400, 400);
    
    // Add buttons to grid
    gtk_grid_attach(GTK_GRID(grid), room_button, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), grade_button, 1, 0, 1, 1);
    
    // Connect signals
    g_signal_connect(logout_button, "clicked", G_CALLBACK(logout), data);
    g_signal_connect(room_button, "clicked", G_CALLBACK(show_room_list), data);
    g_signal_connect(grade_button, "clicked", G_CALLBACK(show_crud_question), data);

    
    // Add all components to main box
    gtk_box_append(GTK_BOX(main_box), header_box);
    gtk_box_append(GTK_BOX(main_box), grid);
    
    // Set up window
    gtk_window_set_child(GTK_WINDOW(window), main_box);
    gtk_widget_show(window);
}

static void send_login_data(GtkWidget *button, AppData *data) {
    const char *username = gtk_editable_get_text(GTK_EDITABLE(data->username_entry));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(data->password_entry));
    
    char message[MAX_BUFFER];
    snprintf(message, sizeof(message), "LOGIN|%s|%s", username, password);
    
    send(data->sock, message, strlen(message), 0);
    
    char response[MAX_BUFFER] = {0};
    recv(data->sock, response, MAX_BUFFER, 0);
    
    char status[10], role[20];
    sscanf(response, "%[^:]:%s", status, role);
    
    if (strcmp(status, "SUCCESS") == 0) {
        if (strcmp(role, "student") == 0) {
            create_student_interface(data);
        } else {
            create_teacher_interface(data);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Invalid username or password");
    }
}

// Hàm xử lý gửi dữ liệu đăng ký
static void send_register_data(GtkButton *button, gpointer user_data) {
    RegisterData *data = (RegisterData *)user_data;
    
    const char *username = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(data->username_entry)));
    const char *password = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(data->password_entry)));
    const char *confirm = gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(data->confirm_entry)));
    
    // Check if passwords match
    if (strcmp(password, confirm) != 0) {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Mật khẩu không khớp!");
        return;
    }
    
    // Prepare register message
    char message[256];
    snprintf(message, sizeof(message), "REGISTER|%s|%s", username, password);
    
    // Send to server
    send(data->sock, message, strlen(message), 0);
    
    // Receive response
    char response[256];
    recv(data->sock, response, sizeof(response), 0);
    
    // Handle response
    if (strstr(response, "SUCCESS") != NULL) {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Đăng ký thành công!");
        // Close register window after a delay
        g_timeout_add(1500, (GSourceFunc)gtk_window_destroy, data->window);
    } else {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Đăng ký thất bại!");
    }
}

static void open_register_window(GtkButton *button, AppData *data) {
    // Create register window
    GtkWidget *register_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(register_window), "Đăng ký tài khoản");
    gtk_window_set_default_size(GTK_WINDOW(register_window), 800, 400);
    
    // Create layout box
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 200);
    gtk_widget_set_margin_end(box, 200);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    
    // Create register form widgets
    GtkWidget *username_label = gtk_label_new("Tên đăng nhập:");
    GtkWidget *username_entry = gtk_entry_new();
    
    GtkWidget *password_label = gtk_label_new("Mật khẩu:");
    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    
    GtkWidget *confirm_label = gtk_label_new("Xác nhận mật khẩu:");
    GtkWidget *confirm_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(confirm_entry), FALSE);
    
    GtkWidget *register_button = gtk_button_new_with_label("Đăng ký");
    GtkWidget *status_label = gtk_label_new("");
    
    // Create RegisterData structure to hold the widgets
    RegisterData *reg_data = g_new(RegisterData, 1);
    reg_data->username_entry = username_entry;
    reg_data->password_entry = password_entry;
    reg_data->confirm_entry = confirm_entry;
    reg_data->status_label = status_label;
    reg_data->sock = data->sock;
    reg_data->window = register_window;
    
    // Add widgets to box
    gtk_box_append(GTK_BOX(box), username_label);
    gtk_box_append(GTK_BOX(box), username_entry);
    gtk_box_append(GTK_BOX(box), password_label);
    gtk_box_append(GTK_BOX(box), password_entry);
    gtk_box_append(GTK_BOX(box), confirm_label);
    gtk_box_append(GTK_BOX(box), confirm_entry);
    gtk_box_append(GTK_BOX(box), register_button);
    gtk_box_append(GTK_BOX(box), status_label);
    
    // Connect signal
    g_signal_connect(register_button, "clicked", G_CALLBACK(send_register_data), reg_data);
    
    // Set window content and show
    gtk_window_set_child(GTK_WINDOW(register_window), box);
    gtk_widget_show(register_window);
}

// Sửa đổi hàm activate để thêm nút đăng ký
static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    // Create main window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Login Application");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 480);
    data->window = window;

     // Create socket connection
    data->sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(data->server_port);
    
    if (inet_pton(AF_INET, data->server_ip, &serv_addr.sin_addr) <= 0) {
        printf("Địa chỉ IP không hợp lệ\n");
        return;
    }
    
    if (connect(data->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Không thể kết nối đến server\n");
        return;
    }
    
    // Create layout box
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 200);
    gtk_widget_set_margin_end(box, 200);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    
    // Create widgets
    GtkWidget *username_label = gtk_label_new("Username:");
    data->username_entry = gtk_entry_new();
    
    GtkWidget *password_label = gtk_label_new("Password:");
    data->password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(data->password_entry), FALSE);
    
    GtkWidget *login_button = gtk_button_new_with_label("Đăng nhập");
    GtkWidget *register_button = gtk_button_new_with_label("Đăng ký");
    data->status_label = gtk_label_new("");
    
    // Add widgets to box
    gtk_box_append(GTK_BOX(box), username_label);
    gtk_box_append(GTK_BOX(box), data->username_entry);
    gtk_box_append(GTK_BOX(box), password_label);
    gtk_box_append(GTK_BOX(box), data->password_entry);
    gtk_box_append(GTK_BOX(box), login_button);
    gtk_box_append(GTK_BOX(box), register_button);
    gtk_box_append(GTK_BOX(box), data->status_label);
    
    // Connect signals
    g_signal_connect(login_button, "clicked", G_CALLBACK(send_login_data), data);
    g_signal_connect(register_button, "clicked", G_CALLBACK(open_register_window), data);
    
    // Set window content and show
    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_widget_show(window);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Sử dụng: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        printf("Port không hợp lệ. Vui lòng sử dụng port từ 1-65535\n");
        return 1;
    }

    AppData *data = g_new(AppData, 1);
    strncpy(data->server_ip, argv[1], sizeof(data->server_ip) - 1);
    data->server_ip[sizeof(data->server_ip) - 1] = '\0';
    data->server_port = port;

    GtkApplication *app = gtk_application_new("com.example.login", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), data);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);
    return status;
}
