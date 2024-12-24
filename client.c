#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MAX_BUFFER 1024

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
    GtkWidget *name_entry;
    GtkWidget *time_limit_entry;
    GtkWidget *start_time_entry;
    GtkWidget *end_time_entry;
    GtkWidget *status_label;
    GtkWidget *window;
    int sock;
} AddRoomData;

static void logout(GtkWidget *button, AppData *data) {
    gtk_window_destroy(GTK_WINDOW(data->main_window));
    gtk_widget_show(data->window);
}

static void dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
}


static void join_exam_room(GtkButton *button, AppData *data) {
    // int room_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "room_id"));
    
    // // Gửi yêu cầu tham gia phòng thi
    // char request[MAX_BUFFER];
    // snprintf(request, sizeof(request), "JOIN_EXAM|%d", room_id);
    // send(data->sock, request, strlen(request), 0);
    
    // // Nhận danh sách câu hỏi
    // char response[MAX_BUFFER];
    // recv(data->sock, response, MAX_BUFFER, 0);
    
    // // Hiển thị giao diện làm bài thi
    // show_exam_interface(data, response, room_id);
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
        g_signal_connect(join_button, "clicked", G_CALLBACK(join_exam_room), data);
        
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        gtk_list_box_append(list_box, GTK_LIST_BOX_ROW(row));
        
        room = strtok(NULL, "|");
    }
    
    gtk_box_append(GTK_BOX(content_area), GTK_WIDGET(list_box));
    
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

// static void show_exam_interface(AppData *data, const char *exam_data, int room_id) {
//     // Tạo cửa sổ mới cho bài thi
//     GtkWidget *exam_window = gtk_window_new();
//     gtk_window_set_title(GTK_WINDOW(exam_window), "Làm bài thi");
//     gtk_window_set_default_size(GTK_WINDOW(exam_window), 1000, 800);

//     // Container chính
//     GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
//     gtk_widget_set_margin_start(main_box, 20);
//     gtk_widget_set_margin_end(main_box, 20);
//     gtk_widget_set_margin_top(main_box, 20);
//     gtk_widget_set_margin_bottom(main_box, 20);

//     // Header hiển thị thời gian
//     GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
//     GtkWidget *time_label = gtk_label_new("Thời gian còn lại: 45:00");
//     GtkWidget *submit_button = gtk_button_new_with_label("Nộp bài");
//     gtk_box_append(GTK_BOX(header_box), time_label);
//     gtk_box_append(GTK_BOX(header_box), submit_button);

//     // Scrolled window cho danh sách câu hỏi
//     GtkWidget *scrolled = gtk_scrolled_window_new();
//     gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 600);
    
//     // Box chứa các câu hỏi
//     GtkWidget *questions_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);

//     // Phân tích dữ liệu câu hỏi từ exam_data
//     char *question = strtok((char *)exam_data, "|");
//     int q_num = 1;
    
//     while (question != NULL) {
//         // Tạo frame cho mỗi câu hỏi
//         GtkWidget *q_frame = gtk_frame_new(NULL);
//         GtkWidget *q_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
//         gtk_widget_set_margin_start(q_box, 10);
//         gtk_widget_set_margin_end(q_box, 10);
//         gtk_widget_set_margin_top(q_box, 10);
//         gtk_widget_set_margin_bottom(q_box, 10);

//         // Phân tích thông tin câu hỏi
//         char q_text[256], a1[100], a2[100], a3[100], a4[100];
//         sscanf(question, "%[^;];%[^;];%[^;];%[^;];%[^;]", 
//                q_text, a1, a2, a3, a4);

//         // Thêm text câu hỏi
//         char q_label_text[300];
//         snprintf(q_label_text, sizeof(q_label_text), "Câu %d: %s", q_num, q_text);
//         GtkWidget *q_label = gtk_label_new(q_label_text);
//         gtk_label_set_wrap(GTK_LABEL(q_label), TRUE);
//         gtk_label_set_xalign(GTK_LABEL(q_label), 0);
//         gtk_box_append(GTK_BOX(q_box), q_label);

//         // Tạo radio buttons cho các đáp án
//         GtkWidget *radio1 = gtk_check_button_new_with_label(a1);
//         GtkWidget *radio2 = gtk_check_button_new_with_label(a2);
//         GtkWidget *radio3 = gtk_check_button_new_with_label(a3);
//         GtkWidget *radio4 = gtk_check_button_new_with_label(a4);

//         // Nhóm các radio buttons
//         gtk_check_button_set_group(GTK_CHECK_BUTTON(radio2), GTK_CHECK_BUTTON(radio1));
//         gtk_check_button_set_group(GTK_CHECK_BUTTON(radio3), GTK_CHECK_BUTTON(radio1));
//         gtk_check_button_set_group(GTK_CHECK_BUTTON(radio4), GTK_CHECK_BUTTON(radio1));

//         // Thêm các đáp án vào box
//         gtk_box_append(GTK_BOX(q_box), radio1);
//         gtk_box_append(GTK_BOX(q_box), radio2);
//         gtk_box_append(GTK_BOX(q_box), radio3);
//         gtk_box_append(GTK_BOX(q_box), radio4);

//         // Thêm vào frame và box chính
//         gtk_frame_set_child(GTK_FRAME(q_frame), q_box);
//         gtk_box_append(GTK_BOX(questions_box), q_frame);

//         question = strtok(NULL, "|");
//         q_num++;
//     }

//     // Thiết lập scrolled window
//     gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), questions_box);

//     // Thêm tất cả vào main box
//     gtk_box_append(GTK_BOX(main_box), header_box);
//     gtk_box_append(GTK_BOX(main_box), scrolled);

//     // Thiết lập và hiển thị cửa sổ
//     gtk_window_set_child(GTK_WINDOW(exam_window), main_box);
//     gtk_window_present(GTK_WINDOW(exam_window));
// }

static void show_history(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Thông báo",
                                                   GTK_WINDOW(user_data),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_OK",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Thống kê lịch sử thi sẽ hiển thị ở đây");
    gtk_box_append(GTK_BOX(content_area), label);
    
    g_signal_connect(dialog, "response", G_CALLBACK(dialog_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void show_practice(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Thông báo",
                                                   GTK_WINDOW(user_data),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_OK",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Chế độ luyện tập sẽ được mở ở đây");
    gtk_box_append(GTK_BOX(content_area), label);
    
    g_signal_connect(dialog, "response", G_CALLBACK(dialog_response), NULL);
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
    
    const char *headers[] = {"Username", "Score", "Start Time"};
    int widths[] = {200, 100, 200};
    
    for (int i = 0; i < 3; i++) {
        GtkWidget *label = gtk_label_new(headers[i]);
        gtk_widget_set_size_request(label, widths[i], -1);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_box_append(GTK_BOX(header_box), label);
    }
    
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(header_row), header_box);
    gtk_list_box_append(GTK_LIST_BOX(list_box), header_row);
    
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
            
            char username[50];
            int score_val;
            char start_time[50];
            
            sscanf(score, "%[^,],%d,%s", username, &score_val, start_time);
            
            // Tạo và thêm các label
            GtkWidget *username_label = gtk_label_new(username);
            gtk_widget_set_size_request(username_label, widths[0], -1);
            gtk_label_set_xalign(GTK_LABEL(username_label), 0);
            
            char score_text[20];
            snprintf(score_text, sizeof(score_text), "%d", score_val);
            GtkWidget *score_label = gtk_label_new(score_text);
            gtk_widget_set_size_request(score_label, widths[1], -1);
            gtk_label_set_xalign(GTK_LABEL(score_label), 0);
            
            GtkWidget *time_label = gtk_label_new(start_time);
            gtk_widget_set_size_request(time_label, widths[2], -1);
            gtk_label_set_xalign(GTK_LABEL(time_label), 0);
            
            gtk_box_append(GTK_BOX(box), username_label);
            gtk_box_append(GTK_BOX(box), score_label);
            gtk_box_append(GTK_BOX(box), time_label);
            
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
            gtk_list_box_append(GTK_LIST_BOX(list_box), row);
            
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
// Sửa lại hàm send_add_room_data
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
        
        // Thêm các widget vào box
        gtk_box_append(GTK_BOX(box), name_label);
        gtk_box_append(GTK_BOX(box), status_label);
        gtk_box_append(GTK_BOX(box), time_label);
        gtk_box_append(GTK_BOX(box), start_label);
        gtk_box_append(GTK_BOX(box), end_label);
        gtk_box_append(GTK_BOX(box), show_score_button);
        
        // Lưu room_id vào button để sử dụng khi click
        g_object_set_data(G_OBJECT(show_score_button), "room_id", GINT_TO_POINTER(room_id));
        g_signal_connect(show_score_button, "clicked", G_CALLBACK(show_score_list), data);
        
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
    g_signal_connect(history_button, "clicked", G_CALLBACK(show_history), window);
    g_signal_connect(practice_button, "clicked", G_CALLBACK(show_practice), window);
    
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
    g_signal_connect(room_button, "clicked", G_CALLBACK(show_room_list), data);
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
