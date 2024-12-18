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

static void logout(GtkWidget *button, AppData *data) {
    gtk_window_destroy(GTK_WINDOW(data->main_window));
    gtk_widget_show(data->window);
}

static void dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void show_exam_list(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Thông báo",
                                                   GTK_WINDOW(user_data),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_OK",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Danh sách phòng thi sẽ hiển thị ở đây");
    gtk_box_append(GTK_BOX(content_area), label);
    
    g_signal_connect(dialog, "response", G_CALLBACK(dialog_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

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
    GtkWidget *window = gtk_window_new();
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
    g_signal_connect(exam_button, "clicked", G_CALLBACK(show_exam_list), window);
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
