#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MAX_BUFFER 16384

typedef struct {
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

static void logout(GtkWidget *button, AppData *data) {
    // Gửi thông báo logout đến server
    send(data->sock, "LOGOUT", 6, 0);
    
    // Đóng socket hiện tại
    close(data->sock);
    
    // Tạo socket mới
    data->sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    connect(data->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    
    // Đóng cửa sổ chính và hiển thị cửa sổ đăng nhập
    gtk_window_destroy(GTK_WINDOW(data->main_window));
    gtk_widget_show(data->window);
}

static void dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
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

static void show_room_list(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Thông báo",
                                                   GTK_WINDOW(user_data),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_OK",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Quản lý phòng thi sẽ được mở ở đây");
    gtk_box_append(GTK_BOX(content_area), label);
    
    g_signal_connect(dialog, "response", G_CALLBACK(dialog_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void show_stats(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Thông báo",
                                                   GTK_WINDOW(user_data),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_OK",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Thống kê điểm theo phòng sẽ được mở ở đây");
    gtk_box_append(GTK_BOX(content_area), label);
    
    g_signal_connect(dialog, "response", G_CALLBACK(dialog_response), NULL);
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
    GtkWidget *grade_button = gtk_button_new_with_label("Xem điểm phòng thi");
    
    // Set size for buttons
    gtk_widget_set_size_request(room_button, 400, 400);
    gtk_widget_set_size_request(grade_button, 400, 400);
    
    // Add buttons to grid
    gtk_grid_attach(GTK_GRID(grid), room_button, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), grade_button, 1, 0, 1, 1);
    
    // Connect signals
    g_signal_connect(logout_button, "clicked", G_CALLBACK(logout), data);
    g_signal_connect(room_button, "clicked", G_CALLBACK(show_room_list), window);
    g_signal_connect(grade_button, "clicked", G_CALLBACK(show_stats), window);
    
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
    // Create main window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Login Application");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 480);
    
    // Create layout box
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 200);
    gtk_widget_set_margin_end(box, 200);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    
    // Create AppData structure
    AppData *data = g_new(AppData, 1);
    data->window = window;
    
    // Create socket connection
    data->sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    connect(data->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    
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
    GtkApplication *app = gtk_application_new("com.example.login", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
