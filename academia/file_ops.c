#include "academia.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

extern sem_t file_sem;

// Utility functions
int validate_id(const char *id) {
    if (strlen(id) == 0 || strlen(id) >= MAX_ID) return ERR_INVALID_INPUT;
    for (int i = 0; id[i]; i++) {
        if (!isalnum(id[i])) return ERR_INVALID_INPUT;
    }
    return 0;
}

int validate_name(const char *name) {
    if (strlen(name) == 0 || strlen(name) >= MAX_NAME) return ERR_INVALID_INPUT;
    for (int i = 0; name[i]; i++) {
        if (!isalpha(name[i]) && name[i] != ' ' && name[i] != '.') return ERR_INVALID_INPUT;
    }
    return 0;
}

int validate_password(const char *password) {
    if (strlen(password) == 0 || strlen(password) >= MAX_PASS) return ERR_INVALID_INPUT;
    return 0;
}

int validate_number(const char *input, int min, int max) {
    for (int i = 0; input[i]; i++) {
        if (!isdigit(input[i])) return ERR_INVALID_INPUT;
    }
    int num = atoi(input);
    if (num < min || num > max) return ERR_INVALID_INPUT;
    return num;
}

// File locking functions
int read_lock(int fd) {
    struct flock lock = {F_RDLCK, SEEK_SET, 0, 0, 0};
    return fcntl(fd, F_SETLKW, &lock);
}

int write_lock(int fd) {
    struct flock lock = {F_WRLCK, SEEK_SET, 0, 0, 0};
    return fcntl(fd, F_SETLKW, &lock);
}

int unlock(int fd) {
    struct flock lock = {F_UNLCK, SEEK_SET, 0, 0, 0};
    return fcntl(fd, F_SETLK, &lock);
}

int add_user(char *id, char *password, enum Role role) {
    int fd = open("users.dat", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    User user;
    strncpy(user.id, id, MAX_ID);
    user.role = role;
    strncpy(user.password, password, MAX_PASS);

    write(fd, &user, sizeof(User));

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return 0;
}

int add_student(char *id, char *name) {
    int fd = open("students.dat", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Student student;
    strncpy(student.id, id, MAX_ID);
    strncpy(student.name, name, MAX_NAME);
    student.active = 1;
    memset(student.enrolled_courses, 0, sizeof(student.enrolled_courses));

    write(fd, &student, sizeof(Student));

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return 0;
}

int add_faculty(char *id, char *name) {
    int fd = open("faculty.dat", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Faculty faculty;
    strncpy(faculty.id, id, MAX_ID);
    strncpy(faculty.name, name, MAX_NAME);

    write(fd, &faculty, sizeof(Faculty));

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return 0;
}

int activate_deactivate_student(char *id, int activate) {
    int fd = open("students.dat", O_RDWR);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Student student;
    off_t pos = 0;
    while (read(fd, &student, sizeof(Student)) > 0) {
        if (strcmp(student.id, id) == 0) {
            student.active = activate;
            lseek(fd, pos, SEEK_SET);
            write(fd, &student, sizeof(Student));
            unlock(fd);
            sem_post(&file_sem);
            close(fd);
            return 0;
        }
        pos += sizeof(Student);
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return -1;
}

int update_student(char *id, char *new_name) {
    int fd = open("students.dat", O_RDWR);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Student student;
    off_t pos = 0;
    while (read(fd, &student, sizeof(Student)) > 0) {
        if (strcmp(student.id, id) == 0) {
            strncpy(student.name, new_name, MAX_NAME);
            lseek(fd, pos, SEEK_SET);
            write(fd, &student, sizeof(Student));
            unlock(fd);
            sem_post(&file_sem);
            close(fd);
            return 0;
        }
        pos += sizeof(Student);
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return -1;
}

int update_faculty(char *id, char *new_name) {
    int fd = open("faculty.dat", O_RDWR);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Faculty faculty;
    off_t pos = 0;
    while (read(fd, &faculty, sizeof(Faculty)) > 0) {
        if (strcmp(faculty.id, id) == 0) {
            strncpy(faculty.name, new_name, MAX_NAME);
            lseek(fd, pos, SEEK_SET);
            write(fd, &faculty, sizeof(Faculty));
            unlock(fd);
            sem_post(&file_sem);
            close(fd);
            return 0;
        }
        pos += sizeof(Faculty);
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return -1;
}

int add_course(char *id, char *name, char *faculty_id, int seats) {
    int fd = open("courses.dat", O_RDWR | O_CREAT, 0644);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    // Check for duplicate course ID
    Course course;
    while (read(fd, &course, sizeof(Course)) > 0) {
        if (strcmp(course.id, id) == 0) {
            unlock(fd);
            sem_post(&file_sem);
            close(fd);
            return -1; // Duplicate course ID
        }
    }

    // Add new course
    memset(&course, 0, sizeof(Course));
    strncpy(course.id, id, MAX_ID);
    strncpy(course.name, name, MAX_NAME);
    strncpy(course.faculty_id, faculty_id, MAX_ID);
    course.total_seats = seats;
    course.enrolled_count = 0;
    memset(course.enrolled_students, 0, sizeof(course.enrolled_students));

    write(fd, &course, sizeof(Course));

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return 0;
}

int update_course(char *id, char *new_name, int new_seats) {
    int fd = open("courses.dat", O_RDWR);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Course course;
    off_t pos = 0;
    while (read(fd, &course, sizeof(Course)) > 0) {
        if (strcmp(course.id, id) == 0) {
            strncpy(course.name, new_name, MAX_NAME);
            course.total_seats = new_seats;
            if (course.enrolled_count > new_seats) {
                course.enrolled_count = new_seats; // Adjust enrolled count if seats reduced
                for (int i = new_seats; i < MAX_USERS; i++) {
                    memset(course.enrolled_students[i], 0, MAX_ID);
                }
            }
            lseek(fd, pos, SEEK_SET);
            write(fd, &course, sizeof(Course));
            unlock(fd);
            sem_post(&file_sem);
            close(fd);
            return 0;
        }
        pos += sizeof(Course);
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return -1;
}

int remove_course(char *id) {
    int fd = open("courses.dat", O_RDWR);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Course course;
    off_t pos = 0;
    int found = 0;
    while (read(fd, &course, sizeof(Course)) > 0) {
        if (strcmp(course.id, id) == 0) {
            found = 1;
            break;
        }
        pos += sizeof(Course);
    }

    if (found) {
        int temp_fd = open("courses_temp.dat", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (temp_fd < 0) {
            unlock(fd);
            sem_post(&file_sem);
            close(fd);
            return -1;
        }

        lseek(fd, 0, SEEK_SET);
        while (read(fd, &course, sizeof(Course)) > 0) {
            if (strcmp(course.id, id) != 0) {
                write(temp_fd, &course, sizeof(Course));
            }
        }

        close(temp_fd);
        rename("courses_temp.dat", "courses.dat");
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);

    // Unenroll all students from this course
    int sfd = open("students.dat", O_RDWR);
    if (sfd >= 0) {
        sem_wait(&file_sem);
        write_lock(sfd);

        Student student;
        off_t spos = 0;
        while (read(sfd, &student, sizeof(Student)) > 0) {
            int changed = 0;
            for (int i = 0; i < MAX_COURSES; i++) {
                if (strcmp(student.enrolled_courses[i], id) == 0) {
                    memset(student.enrolled_courses[i], 0, MAX_ID);
                    changed = 1;
                }
            }
            if (changed) {
                lseek(sfd, spos, SEEK_SET);
                write(sfd, &student, sizeof(Student));
            }
            spos += sizeof(Student);
        }

        unlock(sfd);
        sem_post(&file_sem);
        close(sfd);
    }

    return found ? 0 : -1;
}

int enroll_course(char *student_id, char *course_id) {
    // 1. Open and lock students.dat first to check student status and update their record
    int sfd = open("students.dat", O_RDWR);
    if (sfd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(sfd);

    Student student;
    off_t spos = 0;
    int student_found = 0;
    int already_enrolled = 0;
    int empty_slot = -1;
    while (read(sfd, &student, sizeof(Student)) > 0) {
        if (strcmp(student.id, student_id) == 0) {
            student_found = 1;
            if (!student.active) {
                unlock(sfd);
                sem_post(&file_sem);
                close(sfd);
                return ERR_INVALID_INPUT; // Student is blocked
            }
            // Check if already enrolled
            for (int i = 0; i < MAX_COURSES; i++) {
                if (strcmp(student.enrolled_courses[i], course_id) == 0) {
                    already_enrolled = 1;
                    break;
                }
                if (empty_slot == -1 && student.enrolled_courses[i][0] == '\0') {
                    empty_slot = i;
                }
            }
            break;
        }
        spos += sizeof(Student);
    }

    if (!student_found) {
        unlock(sfd);
        sem_post(&file_sem);
        close(sfd);
        return ERR_NOT_FOUND; // Student does not exist
    }
    if (already_enrolled) {
        unlock(sfd);
        sem_post(&file_sem);
        close(sfd);
        return ERR_ALREADY_ENROLLED;
    }
    if (empty_slot == -1) {
        unlock(sfd);
        sem_post(&file_sem);
        close(sfd);
        return ERR_FULL; // No slot for more courses
    }

    // 2. Now open and lock courses.dat, check course existence and seat availability
    int cfd = open("courses.dat", O_RDWR);
    if (cfd < 0) {
        unlock(sfd);
        sem_post(&file_sem);
        close(sfd);
        return -1;
    }
    write_lock(cfd);

    Course course;
    off_t cpos = 0;
    int course_found = 0;
    int course_full = 0;
    int course_already = 0;
    while (read(cfd, &course, sizeof(Course)) > 0) {
        if (strcmp(course.id, course_id) == 0) {
            course_found = 1;
            if (course.enrolled_count >= course.total_seats) {
                course_full = 1;
                break;
            }
            for (int i = 0; i < course.enrolled_count; i++) {
                if (strcmp(course.enrolled_students[i], student_id) == 0) {
                    course_already = 1;
                    break;
                }
            }
            break;
        }
        cpos += sizeof(Course);
    }

    if (!course_found) {
        unlock(cfd);
        unlock(sfd);
        sem_post(&file_sem);
        close(cfd);
        close(sfd);
        return ERR_COURSE_NOT_FOUND;
    }
    if (course_full) {
        unlock(cfd);
        unlock(sfd);
        sem_post(&file_sem);
        close(cfd);
        close(sfd);
        return ERR_FULL;
    }
    if (course_already) {
        unlock(cfd);
        unlock(sfd);
        sem_post(&file_sem);
        close(cfd);
        close(sfd);
        return ERR_ALREADY_ENROLLED;
    }

    // 3. Update both records
    // Update student
    strncpy(student.enrolled_courses[empty_slot], course_id, MAX_ID);
    lseek(sfd, spos, SEEK_SET);
    write(sfd, &student, sizeof(Student));

    // Update course
    strncpy(course.enrolled_students[course.enrolled_count], student_id, MAX_ID);
    course.enrolled_count++;
    lseek(cfd, cpos, SEEK_SET);
    write(cfd, &course, sizeof(Course));

    unlock(cfd);
    close(cfd);
    unlock(sfd);
    sem_post(&file_sem);
    close(sfd);

    return 0;
}

int unenroll_course(char *student_id, char *course_id) {
    int fd = open("courses.dat", O_RDWR);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Course course;
    off_t pos = 0;
    while (read(fd, &course, sizeof(Course)) > 0) {
        if (strcmp(course.id, course_id) == 0) {
            for (int i = 0; i < course.enrolled_count; i++) {
                if (strcmp(course.enrolled_students[i], student_id) == 0) {
                    for (int j = i; j < course.enrolled_count - 1; j++) {
                        strncpy(course.enrolled_students[j], course.enrolled_students[j + 1], MAX_ID);
                    }
                    course.enrolled_count--;
                    lseek(fd, pos, SEEK_SET);
                    write(fd, &course, sizeof(Course));
                    break;
                }
            }
            break;
        }
        pos += sizeof(Course);
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);

    // Update student's enrolled courses
    fd = open("students.dat", O_RDWR);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    Student student;
    pos = 0;
    while (read(fd, &student, sizeof(Student)) > 0) {
        if (strcmp(student.id, student_id) == 0) {
            for (int i = 0; i < MAX_COURSES; i++) {
                if (strcmp(student.enrolled_courses[i], course_id) == 0) {
                    memset(student.enrolled_courses[i], 0, MAX_ID);
                    lseek(fd, pos, SEEK_SET);
                    write(fd, &student, sizeof(Student));
                    unlock(fd);
                    sem_post(&file_sem);
                    close(fd);
                    return 0;
                }
            }
            unlock(fd);
            sem_post(&file_sem);
            close(fd);
            return ERR_NOT_ENROLLED;
        }
        pos += sizeof(Student);
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return -1;
}

char *view_enrolled_courses(char *student_id) {
    int fd = open("students.dat", O_RDONLY);
    if (fd < 0) {
        printf("Server: Failed to open students.dat, errno=%d\n", errno);
        return NULL;
    }

    sem_wait(&file_sem);
    read_lock(fd);

    size_t buffer_size = 2048;
    char *result = malloc(buffer_size);
    if (!result) {
        printf("Server: Failed to allocate memory for result in view_enrolled_courses\n");
        unlock(fd);
        sem_post(&file_sem);
        close(fd);
        return NULL;
    }
    snprintf(result, buffer_size, "Enrolled Courses:\n");

    Student student;
    int found = 0;
    while (read(fd, &student, sizeof(Student)) > 0) {
        if (strcmp(student.id, student_id) == 0) {
            found = 1;
            int count = 1;
            for (int i = 0; i < MAX_COURSES; i++) {
                if (student.enrolled_courses[i][0] != '\0') {
                    char course_info[100];
                    snprintf(course_info, sizeof(course_info), "%d. %s\n", count++, student.enrolled_courses[i]);
                    size_t needed_size = strlen(result) + strlen(course_info) + 1;
                    if (needed_size > buffer_size) {
                        buffer_size = needed_size + 1024;
                        char *new_result = realloc(result, buffer_size);
                        if (!new_result) {
                            printf("Server: Failed to reallocate memory for result in view_enrolled_courses\n");
                            free(result);
                            unlock(fd);
                            sem_post(&file_sem);
                            close(fd);
                            return NULL;
                        }
                        result = new_result;
                    }
                    strcat(result, course_info);
                }
            }
            if (count == 1) {
                size_t needed_size = strlen(result) + strlen("No courses enrolled.\n") + 1;
                if (needed_size > buffer_size) {
                    buffer_size = needed_size + 1024;
                    char *new_result = realloc(result, buffer_size);
                    if (!new_result) {
                        printf("Server: Failed to reallocate memory for result in view_enrolled_courses\n");
                        free(result);
                        unlock(fd);
                        sem_post(&file_sem);
                        close(fd);
                        return NULL;
                    }
                    result = new_result;
                }
                strcat(result, "No courses enrolled.\n");
            }
            break;
        }
    }

    if (!found) {
        size_t needed_size = strlen("Student not found\n") + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_enrolled_courses\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcpy(result, "Student not found\n");
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return result;
}

char *view_course_enrollments(char *course_id) {
    int fd = open("courses.dat", O_RDONLY);
    if (fd < 0) {
        printf("Server: Failed to open courses.dat, errno=%d\n", errno);
        return NULL;
    }

    sem_wait(&file_sem);
    read_lock(fd);

    size_t buffer_size = 2048;
    char *result = malloc(buffer_size);
    if (!result) {
        printf("Server: Failed to allocate memory for result in view_course_enrollments\n");
        unlock(fd);
        sem_post(&file_sem);
        close(fd);
        return NULL;
    }
    snprintf(result, buffer_size, "Enrollments for Course %s:\n", course_id);

    Course course;
    int found = 0;
    while (read(fd, &course, sizeof(Course)) > 0) {
        if (strcmp(course.id, course_id) == 0) {
            found = 1;
            int count = 1;
            for (int i = 0; i < course.enrolled_count; i++) {
                char student_info[100];
                snprintf(student_info, sizeof(student_info), "%d. %s\n", count++, course.enrolled_students[i]);
                size_t needed_size = strlen(result) + strlen(student_info) + 1;
                if (needed_size > buffer_size) {
                    buffer_size = needed_size + 1024;
                    char *new_result = realloc(result, buffer_size);
                    if (!new_result) {
                        printf("Server: Failed to reallocate memory for result in view_course_enrollments\n");
                        free(result);
                        unlock(fd);
                        sem_post(&file_sem);
                        close(fd);
                        return NULL;
                    }
                    result = new_result;
                }
                strcat(result, student_info);
            }
            if (count == 1) {
                size_t needed_size = strlen(result) + strlen("No students enrolled.\n") + 1;
                if (needed_size > buffer_size) {
                    buffer_size = needed_size + 1024;
                    char *new_result = realloc(result, buffer_size);
                    if (!new_result) {
                        printf("Server: Failed to reallocate memory for result in view_course_enrollments\n");
                        free(result);
                        unlock(fd);
                        sem_post(&file_sem);
                        close(fd);
                        return NULL;
                    }
                    result = new_result;
                }
                strcat(result, "No students enrolled.\n");
            }
            break;
        }
    }

    if (!found) {
        size_t needed_size = strlen("Course not found\n") + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_course_enrollments\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcpy(result, "Course not found\n");
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return result;
}

char *view_all_courses() {
    int fd = open("courses.dat", O_RDONLY);
    if (fd < 0) {
        printf("Server: Failed to open courses.dat, errno=%d\n", errno);
        return NULL;
    }

    sem_wait(&file_sem);
    read_lock(fd);

    size_t buffer_size = 2048;
    char *result = malloc(buffer_size);
    if (!result) {
        printf("Server: Failed to allocate memory for result in view_all_courses\n");
        unlock(fd);
        sem_post(&file_sem);
        close(fd);
        return NULL;
    }
    strcpy(result, "All Available Courses:\n");

    Course course;
    int count = 1;
    while (read(fd, &course, sizeof(Course)) > 0) {
        char course_info[200];
        snprintf(course_info, sizeof(course_info), "%d. ID: %s, Name: %s, Faculty ID: %s, Seats: %d, Enrolled: %d\n",
                 count++, course.id, course.name, course.faculty_id, course.total_seats, course.enrolled_count);
        size_t needed_size = strlen(result) + strlen(course_info) + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_all_courses\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcat(result, course_info);
    }

    if (count == 1) {
        size_t needed_size = strlen(result) + strlen("No courses available.\n") + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_all_courses\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcat(result, "No courses available.\n");
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return result;
}

char *view_faculty_courses(char *faculty_id) {
    int fd = open("courses.dat", O_RDONLY);
    if (fd < 0) {
        printf("Server: Failed to open courses.dat, errno=%d\n", errno);
        return NULL;
    }

    sem_wait(&file_sem);
    read_lock(fd);

    size_t buffer_size = 2048;
    char *result = malloc(buffer_size);
    if (!result) {
        printf("Server: Failed to allocate memory for result in view_faculty_courses\n");
        unlock(fd);
        sem_post(&file_sem);
        close(fd);
        return NULL;
    }
    snprintf(result, buffer_size, "Courses Offered by Faculty %s:\n", faculty_id);

    Course course;
    int count = 1;
    while (read(fd, &course, sizeof(Course)) > 0) {
        if (strcmp(course.faculty_id, faculty_id) == 0) {
            char course_info[200];
            snprintf(course_info, sizeof(course_info), "%d. ID: %s, Name: %s, Seats: %d, Enrolled: %d\n",
                     count++, course.id, course.name, course.total_seats, course.enrolled_count);
            size_t needed_size = strlen(result) + strlen(course_info) + 1;
            if (needed_size > buffer_size) {
                buffer_size = needed_size + 1024;
                char *new_result = realloc(result, buffer_size);
                if (!new_result) {
                    printf("Server: Failed to reallocate memory for result in view_faculty_courses\n");
                    free(result);
                    unlock(fd);
                    sem_post(&file_sem);
                    close(fd);
                    return NULL;
                }
                result = new_result;
            }
            strcat(result, course_info);
        }
    }

    if (count == 1) {
        size_t needed_size = strlen(result) + strlen("No courses offered.\n") + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_faculty_courses\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcat(result, "No courses offered.\n");
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return result;
}

char *view_all_students() {
    int fd = open("students.dat", O_RDONLY);
    if (fd < 0) {
        printf("Server: Failed to open students.dat, errno=%d\n", errno);
        return NULL;
    }

    sem_wait(&file_sem);
    read_lock(fd);

    size_t buffer_size = 2048;
    char *result = malloc(buffer_size);
    if (!result) {
        printf("Server: Failed to allocate memory for result in view_all_students\n");
        unlock(fd);
        sem_post(&file_sem);
        close(fd);
        return NULL;
    }
    strcpy(result, "All Students:\n");

    Student student;
    int count = 1;
    while (read(fd, &student, sizeof(Student)) > 0) {
        char student_info[200];
        snprintf(student_info, sizeof(student_info), "%d. ID: %s, Name: %s, Status: %s\n",
                 count++, student.id, student.name, student.active ? "Active" : "Blocked");
        size_t needed_size = strlen(result) + strlen(student_info) + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_all_students\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcat(result, student_info);
    }

    if (count == 1) {
        size_t needed_size = strlen(result) + strlen("No students available.\n") + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_all_students\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcat(result, "No students available.\n");
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return result;
}

char *view_all_faculty() {
    int fd = open("faculty.dat", O_RDONLY);
    if (fd < 0) {
        printf("Server: Failed to open faculty.dat, errno=%d\n", errno);
        return NULL;
    }

    sem_wait(&file_sem);
    read_lock(fd);

    size_t buffer_size = 2048;
    char *result = malloc(buffer_size);
    if (!result) {
        printf("Server: Failed to allocate memory for result in view_all_faculty\n");
        unlock(fd);
        sem_post(&file_sem);
        close(fd);
        return NULL;
    }
    strcpy(result, "All Faculty:\n");

    Faculty faculty;
    int count = 1;
    while (read(fd, &faculty, sizeof(Faculty)) > 0) {
        char faculty_info[200];
        snprintf(faculty_info, sizeof(faculty_info), "%d. ID: %s, Name: %s\n",
                 count++, faculty.id, faculty.name);
        size_t needed_size = strlen(result) + strlen(faculty_info) + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_all_faculty\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcat(result, faculty_info);
    }

    if (count == 1) {
        size_t needed_size = strlen(result) + strlen("No faculty available.\n") + 1;
        if (needed_size > buffer_size) {
            buffer_size = needed_size + 1024;
            char *new_result = realloc(result, buffer_size);
            if (!new_result) {
                printf("Server: Failed to reallocate memory for result in view_all_faculty\n");
                free(result);
                unlock(fd);
                sem_post(&file_sem);
                close(fd);
                return NULL;
            }
            result = new_result;
        }
        strcat(result, "No faculty available.\n");
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return result;
}

int change_password(char *user_id, char *new_password) {
    int fd = open("users.dat", O_RDWR);
    if (fd < 0) return -1;

    sem_wait(&file_sem);
    write_lock(fd);

    User user;
    off_t pos = 0;
    while (read(fd, &user, sizeof(User)) > 0) {
        if (strcmp(user.id, user_id) == 0) {
            strncpy(user.password, new_password, MAX_PASS);
            lseek(fd, pos, SEEK_SET);
            write(fd, &user, sizeof(User));
            unlock(fd);
            sem_post(&file_sem);
            close(fd);
            return 0;
        }
        pos += sizeof(User);
    }

    unlock(fd);
    sem_post(&file_sem);
    close(fd);
    return -1;
}

void initial_setup() {
    // Check if users.dat is empty to avoid duplicate setup
    int fd = open("users.dat", O_RDONLY);
    if (fd >= 0) {
        off_t size = lseek(fd, 0, SEEK_END);
        close(fd);
        if (size > 0) {
            return; // File is not empty, skip setup
        }
    }

    // Add admin user
    if (add_user("admin1", "adminpass", ADMIN) != 0) {
        fprintf(stderr, "Failed to add admin user\n");
        return;
    }
    printf("Added admin user: ID=admin1, Password=adminpass\n");

    // Add sample student
    if (add_user("s1", "pass1", STUDENT) == 0 && add_student("s1", "John") == 0) {
        printf("Added student: ID=s1, Name=John, Password=pass1\n");
    } else {
        fprintf(stderr, "Failed to add student\n");
    }

    // Add sample faculty
    if (add_user("f1", "pass2", FACULTY) == 0 && add_faculty("f1", "Dr. Smith") == 0) {
        printf("Added faculty: ID=f1, Name=Dr. Smith, Password=pass2\n");
    } else {
        fprintf(stderr, "Failed to add faculty\n");
    }
}
