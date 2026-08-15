#include "academia.h"
#include <fcntl.h>
#include <netinet/tcp.h>
#include <signal.h>

// Function to clear input buffer
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Ignore SIGPIPE to prevent the program from terminating on write errors
void ignore_sigpipe() {
    signal(SIGPIPE, SIG_IGN);
}

// Debug function to print raw bytes of a string
void print_raw_bytes(const char *str, int len) {
    printf("Client: Raw bytes of response: ");
    for (int i = 0; i < len; i++) {
        printf("%02x ", (unsigned char)str[i]);
    }
    printf("\n");
}

// Strip trailing \r and \n
void strip_newlines(char *str, int len) {
    for (int i = len - 1; i >= 0; i--) {
        if (str[i] == '\r' || str[i] == '\n') {
            str[i] = '\0';
        } else {
            break;
        }
    }
}

// Read a message with a length prefix
int read_with_length(int sock, char *buffer, size_t max_size) {
    uint32_t len_net;
    int bytes = read(sock, &len_net, sizeof(len_net));
    if (bytes <= 0) {
        printf("Client: Failed to read length prefix, bytes=%d, errno=%d\n", bytes, errno);
        return -1;
    }
    uint32_t len = ntohl(len_net); // Convert from network byte order
    if (len >= max_size) {
        printf("Client: Message too large (%u bytes), max allowed=%zu\n", len, max_size);
        return -1;
    }
    bytes = read(sock, buffer, len);
    if (bytes <= 0) {
        printf("Client: Failed to read message, bytes=%d, errno=%d\n", bytes, errno);
        return -1;
    }
    buffer[bytes] = '\0';
    // Remove or comment out the debug print to avoid double printing
    // printf("Client: Received message (%d bytes): %s\n", bytes, buffer);
    return bytes;
}

void handle_admin(int sock) {
    char buffer[2048], response[2048];
    while (1) {
        // Receive menu
        int bytes = read_with_length(sock, buffer, sizeof(buffer));
        if (bytes < 0) {
            printf("Client: Failed to read Admin Menu\n");
            return;
        }
        // Print the menu
        printf("%s", buffer);

        // Send choice
        char choice[10] = {0};
        printf("Client: Waiting for user input...\n");
        if (scanf("%s", choice) != 1) {
            printf("Client: Failed to read user choice with scanf\n");
            clear_input_buffer();
            return;
        }
        clear_input_buffer();
        printf("Client: Sending choice: %s\n", choice);
        int bytes_written = write(sock, choice, strlen(choice));
        if (bytes_written < 0) {
            printf("Client: Failed to send choice, errno=%d\n", errno);
            return;
        }
        fsync(sock);
        printf("Client: Sent choice (%d bytes)\n", bytes_written);

        if (atoi(choice) == 9) {
            bytes = read_with_length(sock, response, sizeof(response));
            if (bytes < 0) {
                printf("Client: Server disconnected during logout\n");
                return;
            }
            printf("%s", response);
            printf("Client: Admin logged out\n");
            break;
        }

        // Handle admin inputs
        switch (atoi(choice)) {
            case 1: { // Add Student
                char id[MAX_ID], name[MAX_NAME], password[MAX_PASS];
                printf("Enter Student ID: ");
                scanf("%s", id);
                clear_input_buffer();
                printf("Enter Student Name: ");
                scanf("%s", name);
                clear_input_buffer();
                printf("Enter Password: ");
                scanf("%s", password);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                write(sock, name, sizeof(name));
                write(sock, password, sizeof(password));
                break;
            }
            case 2: { // View Student Details
                break; // Response will be handled below
            }
            case 3: { // Add Faculty
                char id[MAX_ID], name[MAX_NAME], password[MAX_PASS];
                printf("Enter Faculty ID: ");
                scanf("%s", id);
                clear_input_buffer();
                printf("Enter Faculty Name: ");
                scanf("%s", name);
                clear_input_buffer();
                printf("Enter Password: ");
                scanf("%s", password);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                write(sock, name, sizeof(name));
                write(sock, password, sizeof(password));
                break;
            }
            case 4: { // View Faculty Details
                break; // Response will be handled below
            }
            case 5: { // Activate Student
                char id[MAX_ID];
                printf("Enter Student ID to Activate: ");
                scanf("%s", id);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                break;
            }
            case 6: { // Block Student
                char id[MAX_ID];
                printf("Enter Student ID to Block: ");
                scanf("%s", id);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                break;
            }
            case 7: { // Modify Student Details
                char id[MAX_ID], new_name[MAX_NAME];
                printf("Enter Student ID: ");
                scanf("%s", id);
                clear_input_buffer();
                printf("Enter New Name: ");
                scanf("%s", new_name);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                write(sock, new_name, sizeof(new_name));
                break;
            }
            case 8: { // Modify Faculty Details
                char id[MAX_ID], new_name[MAX_NAME];
                printf("Enter Faculty ID: ");
                scanf("%s", id);
                clear_input_buffer();
                printf("Enter New Name: ");
                scanf("%s", new_name);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                write(sock, new_name, sizeof(new_name));
                break;
            }
            default:
                break;
        }

        // Receive response
        printf("Client: Waiting for server response...\n");
        bytes = read_with_length(sock, response, sizeof(response));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading response\n");
            return;
        }
        // Print the response (e.g., student or faculty details)
        printf("%s", response);
    }
}

void handle_student(int sock) {
    char buffer[2048], response[2048];
    while (1) {
        int bytes = read_with_length(sock, buffer, sizeof(buffer));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading menu\n");
            return;
        }
        printf("%s", buffer);

        char choice[10] = {0};
        printf("Client: Waiting for user input...\n");
        if (scanf("%s", choice) != 1) {
            printf("Client: Failed to read user choice with scanf\n");
            clear_input_buffer();
            return;
        }
        clear_input_buffer();
        printf("Client: Sending choice: %s\n", choice);
        int bytes_written = write(sock, choice, strlen(choice));
        if (bytes_written < 0) {
            printf("Client: Failed to send choice, errno=%d\n", errno);
            return;
        }
        fsync(sock);
        printf("Client: Sent choice (%d bytes)\n", bytes_written);

        if (atoi(choice) == 6) {
            bytes = read_with_length(sock, response, sizeof(response));
            if (bytes < 0) {
                printf("Client: Server disconnected during logout\n");
                return;
            }
            printf("%s", response);
            printf("Client: Student logged out\n");
            break;
        }

        switch (atoi(choice)) {
            case 1: { // View All Courses
                break;
            }
            case 2: { // Enroll New Course
                char course_id[MAX_ID];
                printf("Enter Course ID to Enroll: ");
                scanf("%s", course_id);
                clear_input_buffer();
                write(sock, course_id, sizeof(course_id));
                break;
            }
            case 3: { // Drop Course
                char course_id[MAX_ID];
                printf("Enter Course ID to Drop: ");
                scanf("%s", course_id);
                clear_input_buffer();
                write(sock, course_id, sizeof(course_id));
                break;
            }
            case 4: { // View Enrolled Course Details
                break;
            }
            case 5: { // Change Password
                char new_password[MAX_PASS];
                printf("Enter New Password: ");
                scanf("%s", new_password);
                clear_input_buffer();
                write(sock, new_password, sizeof(new_password));
                break;
            }
            default:
                break;
        }

        printf("Client: Waiting for server response...\n");
        bytes = read_with_length(sock, response, sizeof(response));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading response\n");
            return;
        }
        printf("%s", response);
    }
}

void handle_faculty(int sock) {
    char buffer[2048], response[2048];
    while (1) {
        int bytes = read_with_length(sock, buffer, sizeof(buffer));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading menu\n");
            return;
        }
        printf("%s", buffer);

        char choice[10] = {0};
        printf("Client: Waiting for user input...\n");
        if (scanf("%s", choice) != 1) {
            printf("Client: Failed to read user choice with scanf\n");
            clear_input_buffer();
            return;
        }
        clear_input_buffer();
        printf("Client: Sending choice: %s\n", choice);
        int bytes_written = write(sock, choice, strlen(choice));
        if (bytes_written < 0) {
            printf("Client: Failed to send choice, errno=%d\n", errno);
            return;
        }
        fsync(sock);
        printf("Client: Sent choice (%d bytes)\n", bytes_written);

        if (atoi(choice) == 6) {
            bytes = read_with_length(sock, response, sizeof(response));
            if (bytes < 0) {
                printf("Client: Server disconnected during logout\n");
                return;
            }
            printf("%s", response);
            printf("Client: Faculty logged out\n");
            break;
        }

        switch (atoi(choice)) {
            case 1: { // View Offering Courses
                break;
            }
            case 2: { // Add New Course
                char id[MAX_ID], name[MAX_NAME];
                int seats;
                printf("Enter Course ID: ");
                scanf("%s", id);
                clear_input_buffer();
                printf("Enter Course Name: ");
                scanf("%s", name);
                clear_input_buffer();
                printf("Enter Total Seats: ");
                scanf("%d", &seats);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                write(sock, name, sizeof(name));
                write(sock, &seats, sizeof(seats));
                break;
            }
            case 3: { // Remove Course from Catalog
                char id[MAX_ID];
                printf("Enter Course ID to Remove: ");
                scanf("%s", id);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                break;
            }
            case 4: { // Update Course Details
                char id[MAX_ID], new_name[MAX_NAME];
                int new_seats;
                printf("Enter Course ID to Update: ");
                scanf("%s", id);
                clear_input_buffer();
                printf("Enter New Course Name: ");
                scanf("%s", new_name);
                clear_input_buffer();
                printf("Enter New Total Seats: ");
                scanf("%d", &new_seats);
                clear_input_buffer();
                write(sock, id, sizeof(id));
                write(sock, new_name, sizeof(new_name));
                write(sock, &new_seats, sizeof(new_seats));
                break;
            }
            case 5: { // Change Password
                char new_password[MAX_PASS];
                printf("Enter New Password: ");
                scanf("%s", new_password);
                clear_input_buffer();
                write(sock, new_password, sizeof(new_password));
                break;
            }
            default:
                break;
        }

        printf("Client: Waiting for server response...\n");
        bytes = read_with_length(sock, response, sizeof(response));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading response\n");
            return;
        }
        printf("%s", response);
    }
}

int main() {
    char buffer[2048], response[2048], login_choice[10], user_id[MAX_ID], password[MAX_PASS];
    int bytes;

    ignore_sigpipe();

    while (1) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation failed");
            exit(1);
        }

        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);

        int flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            close(sock);
            sleep(1);
            continue;
        }

        bytes = read_with_length(sock, buffer, sizeof(buffer));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading login screen\n");
            close(sock);
            continue;
        }
        printf("%s", buffer);

        scanf("%s", login_choice);
        clear_input_buffer();
        printf("Client: Sending login choice: %s\n", login_choice);
        int bytes_written = write(sock, login_choice, strlen(login_choice));
        if (bytes_written < 0) {
            printf("Client: Failed to send login choice, errno=%d\n", errno);
            close(sock);
            continue;
        }
        fsync(sock);

        bytes = read_with_length(sock, response, sizeof(response));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading user ID prompt\n");
            close(sock);
            continue;
        }
        printf("%s", response);
        scanf("%s", user_id);
        clear_input_buffer();
        printf("Client: Sending user ID: %s\n", user_id);
        bytes_written = write(sock, user_id, sizeof(user_id));
        if (bytes_written < 0) {
            printf("Client: Failed to send user ID, errno=%d\n", errno);
            close(sock);
            continue;
        }
        fsync(sock);

        bytes = read_with_length(sock, response, sizeof(response));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading password prompt\n");
            close(sock);
            continue;
        }
        printf("%s", response);
        scanf("%s", password);
        clear_input_buffer();
        printf("Client: Sending password: %s\n", password);
        bytes_written = write(sock, password, sizeof(password));
        if (bytes_written < 0) {
            printf("Client: Failed to send password, errno=%d\n", errno);
            close(sock);
            continue;
        }
        fsync(sock);

        bytes = read_with_length(sock, response, sizeof(response));
        if (bytes < 0) {
            printf("Client: Server disconnected while reading auth response\n");
            close(sock);
            continue;
        }
        printf("%s", response);

        print_raw_bytes(response, bytes);
        strip_newlines(response, bytes);

        if (strcmp(response, "Login successful") == 0) {
            printf("Client: Login successful, proceeding to handle role\n");
            int choice = atoi(login_choice);
            switch (choice) {
                case 1: handle_admin(sock); break;
                case 2: handle_faculty(sock); break;
                case 3: handle_student(sock); break;
            }
            close(sock);
            break;
        } else {
            printf("Client: Login response mismatch, expected 'Login successful'\n");
            printf("Please try again.\n");
            close(sock);
        }
    }

    return 0;
}
