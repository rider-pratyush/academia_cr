#include "academia.h"
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdarg.h> // Added for va_start, va_end

// Semaphore for file operations
sem_t file_sem;

// File pointer for logging
FILE *log_file;

// Ignore SIGPIPE to prevent server from terminating on broken pipe
void ignore_sigpipe() {
    signal(SIGPIPE, SIG_IGN);
}

// Custom logging function to write to log file
void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fflush(log_file); // Ensure logs are written immediately
}

// Send a message with a length prefix
void send_with_length(int sock, const char *message) {
    uint32_t len = strlen(message);
    uint32_t len_net = htonl(len); // Convert to network byte order
    int bytes_written = write(sock, &len_net, sizeof(len_net));
    if (bytes_written < 0) {
        log_message("Server: Failed to send length prefix, errno=%d\n", errno);
        return;
    }
    bytes_written = write(sock, message, len);
    if (bytes_written < 0) {
        log_message("Server: Failed to send message, errno=%d\n", errno);
        return;
    }
    log_message("Server: Sent message (%d bytes): %s\n", len, message);
}

void handle_admin(int sock, char *user_id) {
    char buffer[1024], response[1024];
    const char *menu = "....... Welcome to Admin Menu .......\n"
                       "1. Add Student\n"
                       "2. View Student Details\n"
                       "3. Add Faculty\n"
                       "4. View Faculty Details\n"
                       "5. Activate Student\n"
                       "6. Block Student\n"
                       "7. Modify Student Details\n"
                       "8. Modify Faculty Details\n"
                       "9. Logout and Exit\n"
                       "Enter Your Choice: ";

    while (1) {
        // Send menu with length prefix
        send_with_length(sock, menu);

        // Receive choice
        int bytes = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) {
            log_message("Server: Client disconnected while reading choice, bytes=%d\n", bytes);
            break;
        }
        buffer[bytes] = '\0';
        int choice = atoi(buffer);
        log_message("Server: Received admin menu choice: %d\n", choice);

        if (choice == 9) {
            send_with_length(sock, "Logout successful\n");
            break;
        }

        char temp_response[1024] = {0};
        switch (choice) {
            case 1: { // Add Student
                char id[MAX_ID], name[MAX_NAME], password[MAX_PASS];
                read(sock, id, sizeof(id));
                read(sock, name, sizeof(name));
                read(sock, password, sizeof(password));
                int ret = add_user(id, password, STUDENT);
                if (ret == 0) ret = add_student(id, name);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Student added successfully\n" : "Failed to add student\n");
                break;
            }
            case 2: { // View Student Details
                char *students = view_all_students();
                snprintf(temp_response, sizeof(temp_response), "%s", students ? students : "No students found or error occurred\n");
                if (students) free(students);
                log_message("Server: Sending response for View Student Details: %s", temp_response);
                break;
            }
            case 3: { // Add Faculty
                char id[MAX_ID], name[MAX_NAME], password[MAX_PASS];
                read(sock, id, sizeof(id));
                read(sock, name, sizeof(name));
                read(sock, password, sizeof(password));
                int ret = add_user(id, password, FACULTY);
                if (ret == 0) ret = add_faculty(id, name);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Faculty added successfully\n" : "Failed to add faculty\n");
                break;
            }
            case 4: { // View Faculty Details
                char *faculty = view_all_faculty();
                snprintf(temp_response, sizeof(temp_response), "%s", faculty ? faculty : "No faculty found or error occurred\n");
                if (faculty) free(faculty);
                log_message("Server: Sending response for View Faculty Details: %s", temp_response);
                break;
            }
            case 5: { // Activate Student
                char id[MAX_ID];
                read(sock, id, sizeof(id));
                int ret = activate_deactivate_student(id, 1);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Student activated successfully\n" : "Failed to activate student\n");
                break;
            }
            case 6: { // Block Student
                char id[MAX_ID];
                read(sock, id, sizeof(id));
                int ret = activate_deactivate_student(id, 0);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Student blocked successfully\n" : "Failed to block student\n");
                break;
            }
            case 7: { // Modify Student Details
                char id[MAX_ID], new_name[MAX_NAME];
                read(sock, id, sizeof(id));
                read(sock, new_name, sizeof(new_name));
                int ret = update_student(id, new_name);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Student details updated successfully\n" : "Failed to update student details\n");
                break;
            }
            case 8: { // Modify Faculty Details
                char id[MAX_ID], new_name[MAX_NAME];
                read(sock, id, sizeof(id));
                read(sock, new_name, sizeof(new_name));
                int ret = update_faculty(id, new_name);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Faculty details updated successfully\n" : "Failed to update faculty details\n");
                break;
            }
            default:
                snprintf(temp_response, sizeof(temp_response), "Invalid choice\n");
                break;
        }

        // Send response with length prefix
        send_with_length(sock, temp_response);
    }
}

void handle_student(int sock, char *student_id) {
    char buffer[1024], response[1024];
    const char *menu = "....... Welcome to Student Menu .......\n"
                       "1. View All Courses\n"
                       "2. Enroll New Course\n"
                       "3. Drop Course\n"
                       "4. View Enrolled Course Details\n"
                       "5. Change Password\n"
                       "6. Logout and Exit\n"
                       "Enter Your Choice: ";

    while (1) {
        send_with_length(sock, menu);

        int bytes = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) {
            log_message("Server: Client disconnected while reading choice, bytes=%d\n", bytes);
            break;
        }
        buffer[bytes] = '\0';
        int choice = atoi(buffer);
        log_message("Server: Received student menu choice: %d\n", choice);

        if (choice == 6) {
            send_with_length(sock, "Logout successful\n");
            break;
        }

        char temp_response[1024] = {0};
        switch (choice) {
            case 1: { // View All Courses
                char *courses = view_all_courses();
                snprintf(temp_response, sizeof(temp_response), "%s", courses ? courses : "No courses found or error occurred\n");
                if (courses) free(courses);
                break;
            }
            case 2: { // Enroll New Course
                char course_id[MAX_ID];
                read(sock, course_id, sizeof(course_id));
                int ret = enroll_course(student_id, course_id);
                if (ret == 0) {
                    snprintf(temp_response, sizeof(temp_response), "Enrolled successfully\n");
                } else if (ret == ERR_FULL) {
                    snprintf(temp_response, sizeof(temp_response), "Course is full\n");
                } else if (ret == ERR_ALREADY_ENROLLED) {
                    snprintf(temp_response, sizeof(temp_response), "Already enrolled in this course\n");
                } else if (ret == ERR_COURSE_NOT_FOUND) {
                    snprintf(temp_response, sizeof(temp_response), "Course not found\n");
                } else if (ret == ERR_INVALID_INPUT) {
                    snprintf(temp_response, sizeof(temp_response), "Student is blocked\n");
                } else {
                    snprintf(temp_response, sizeof(temp_response), "Failed to enroll\n");
                }
                break;
            }
            case 3: { // Drop Course
                char course_id[MAX_ID];
                read(sock, course_id, sizeof(course_id));
                int ret = unenroll_course(student_id, course_id);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Dropped successfully\n" : ret == ERR_NOT_ENROLLED ? "Not enrolled in this course\n" : "Failed to drop course\n");
                break;
            }
            case 4: { // View Enrolled Course Details
                char *courses = view_enrolled_courses(student_id);
                snprintf(temp_response, sizeof(temp_response), "%s", courses ? courses : "No courses enrolled or error occurred\n");
                if (courses) free(courses);
                break;
            }
            case 5: { // Change Password
                char new_password[MAX_PASS];
                read(sock, new_password, sizeof(new_password));
                int ret = change_password(student_id, new_password);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Password changed successfully\n" : "Failed to change password\n");
                break;
            }
            default:
                snprintf(temp_response, sizeof(temp_response), "Invalid choice\n");
                break;
        }

        send_with_length(sock, temp_response);
    }
}

void handle_faculty(int sock, char *faculty_id) {
    char buffer[1024], response[1024];
    const char *menu = "....... Welcome to Faculty Menu .......\n"
                       "1. View Offering Courses\n"
                       "2. Add New Course\n"
                       "3. Remove Course from Catalog\n"
                       "4. Update Course Details\n"
                       "5. Change Password\n"
                       "6. Logout and Exit\n"
                       "Enter Your Choice: ";

    while (1) {
        send_with_length(sock, menu);

        int bytes = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) {
            log_message("Server: Client disconnected while reading choice, bytes=%d\n", bytes);
            break;
        }
        buffer[bytes] = '\0';
        int choice = atoi(buffer);
        log_message("Server: Received faculty menu choice: %d\n", choice);

        if (choice == 6) {
            send_with_length(sock, "Logout successful\n");
            break;
        }

        char temp_response[1024] = {0};
        switch (choice) {
            case 1: { // View Offering Courses
                char *courses = view_faculty_courses(faculty_id);
                snprintf(temp_response, sizeof(temp_response), "%s", courses ? courses : "No courses found or error occurred\n");
                if (courses) free(courses);
                break;
            }
            case 2: { // Add New Course
                char id[MAX_ID], name[MAX_NAME];
                int seats;
                read(sock, id, sizeof(id));
                read(sock, name, sizeof(name));
                read(sock, &seats, sizeof(seats));
                int ret = add_course(id, name, faculty_id, seats);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Course added successfully\n" : "Failed to add course\n");
                break;
            }
            case 3: { // Remove Course from Catalog
                char id[MAX_ID];
                read(sock, id, sizeof(id));
                int ret = remove_course(id);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Course removed successfully\n" : "Failed to remove course\n");
                break;
            }
            case 4: { // Update Course Details
                char id[MAX_ID], new_name[MAX_NAME];
                int new_seats;
                read(sock, id, sizeof(id));
                read(sock, new_name, sizeof(new_name));
                read(sock, &new_seats, sizeof(new_seats));
                int ret = update_course(id, new_name, new_seats);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Course updated successfully\n" : "Failed to update course\n");
                break;
            }
            case 5: { // Change Password
                char new_password[MAX_PASS];
                read(sock, new_password, sizeof(new_password));
                int ret = change_password(faculty_id, new_password);
                snprintf(temp_response, sizeof(temp_response), ret == 0 ? "Password changed successfully\n" : "Failed to change password\n");
                break;
            }
            default:
                snprintf(temp_response, sizeof(temp_response), "Invalid choice\n");
                break;
        }

        send_with_length(sock, temp_response);
    }
}

void *client_handler(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    // Disable Nagle's algorithm for immediate data transmission
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    // Send login screen with length prefix
    const char *login_screen = "....................Welcome Back to Academia :: Course Registration....................\n"
                               "Login Type\n"
                               "Enter Your Choice { 1.Admin , 2.Professor, 3. Student } : ";
    send_with_length(sock, login_screen);

    // Receive login choice
    char buffer[1024];
    int bytes = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        log_message("Server: Client disconnected while reading login choice, bytes=%d\n", bytes);
        close(sock);
        return NULL;
    }
    buffer[bytes] = '\0';
    int login_choice = atoi(buffer);
    log_message("Server: Received login choice: %d\n", login_choice);

    if (login_choice < 1 || login_choice > 3) {
        send_with_length(sock, "Invalid choice\n");
        close(sock);
        return NULL;
    }

    // Prompt for credentials
    send_with_length(sock, "Enter User ID: ");
    bytes = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        log_message("Server: Client disconnected while reading user ID, bytes=%d\n", bytes);
        close(sock);
        return NULL;
    }
    buffer[bytes] = '\0';
    char user_id[MAX_ID];
    strncpy(user_id, buffer, MAX_ID - 1);
    user_id[MAX_ID - 1] = '\0';
    log_message("Server: Received user ID: %s\n", user_id);

    send_with_length(sock, "Enter Password: ");
    bytes = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        log_message("Server: Client disconnected while reading password, bytes=%d\n", bytes);
        close(sock);
        return NULL;
    }
    buffer[bytes] = '\0';
    char password[MAX_PASS];
    strncpy(password, buffer, MAX_PASS - 1);
    password[MAX_PASS - 1] = '\0';
    log_message("Server: Received password: %s\n", password);

    // Authenticate user
    int fd = open("users.dat", O_RDONLY);
    if (fd < 0) {
        send_with_length(sock, "Server error: Cannot open users file\n");
        close(sock);
        return NULL;
    }

    sem_wait(&file_sem);
    read_lock(fd);

    User user;
    int authenticated = 0;
    enum Role expected_role = (login_choice == 1) ? ADMIN : (login_choice == 2) ? FACULTY : STUDENT;
    while (read(fd, &user, sizeof(User)) > 0) {
        if (strcmp(user.id, user_id) == 0 && strcmp(user.password, password) == 0 && user.role == expected_role) {
            authenticated = 1;
            break;
        }
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);

    // Send authentication result
    char auth_response[32];
    snprintf(auth_response, sizeof(auth_response), authenticated ? "Login successful\n" : "Login failed\n");
    log_message("Server: Raw bytes of message: ");
    for (int i = 0; i < strlen(auth_response); i++) {
        log_message("%02x ", (unsigned char)auth_response[i]);
    }
    log_message("\n");
    if (authenticated) {
        log_message("Server: Login successful for user %s\n", user_id);
    } else {
        log_message("Server: Login failed for user %s\n", user_id);
        send_with_length(sock, auth_response);
        close(sock);
        return NULL;
    }
    send_with_length(sock, auth_response);

    // Handle user based on role
    switch (login_choice) {
        case 1: handle_admin(sock, user_id); break;
        case 2: handle_faculty(sock, user_id); break;
        case 3: handle_student(sock, user_id); break;
        default:
            send_with_length(sock, "Invalid role\n");
            break;
    }

    close(sock);
    return NULL;
}

int main() {
    // Initialize log file
    log_file = fopen("server.log", "a");
    if (!log_file) {
        perror("Failed to open server.log");
        exit(1);
    }

    // Initialize semaphore
    if (sem_init(&file_sem, 0, 1) < 0) {
        perror("Semaphore initialization failed");
        fclose(log_file);
        exit(1);
    }

    // Perform initial setup
    initial_setup();

    // Ignore SIGPIPE
    ignore_sigpipe();

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        fclose(log_file);
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        fclose(log_file);
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        fclose(log_file);
        exit(1);
    }

    // Print to terminal (not redirected to log file)
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (*client_sock < 0) {
            perror("Accept failed");
            free(client_sock);
            continue;
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, client_sock) != 0) {
            perror("Thread creation failed");
            close(*client_sock);
            free(client_sock);
        }
        pthread_detach(thread);
    }

    close(server_sock);
    sem_destroy(&file_sem);
    fclose(log_file);
    return 0;
}
