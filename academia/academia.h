#ifndef ACADEMIA_H
#define ACADEMIA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/file.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define MAX_COURSES 100
#define MAX_USERS 100
#define MAX_NAME 50
#define MAX_PASS 50
#define MAX_ID 10

// Error codes
#define ERR_NONE 0
#define ERR_NOT_FOUND -1
#define ERR_FULL -2
#define ERR_ALREADY_ENROLLED -3
#define ERR_NOT_ENROLLED -4
#define ERR_INVALID_INPUT -5
#define ERR_COURSE_NOT_FOUND -6

// User roles
enum Role { ADMIN, STUDENT, FACULTY };

// User structure
typedef struct {
    char id[MAX_ID];
    enum Role role;
    char password[MAX_PASS];
} User;

// Course structure
typedef struct {
    char id[MAX_ID];
    char name[MAX_NAME];
    char faculty_id[MAX_ID];
    int total_seats;
    int enrolled_count;
    char enrolled_students[MAX_USERS][MAX_ID];
} Course;

// Student structure
typedef struct {
    char id[MAX_ID];
    char name[MAX_NAME];
    int active;
    char enrolled_courses[MAX_COURSES][MAX_ID];
} Student;

// Faculty structure
typedef struct {
    char id[MAX_ID];
    char name[MAX_NAME];
} Faculty;

// Utility functions
int validate_id(const char *id);
int validate_name(const char *name);
int validate_password(const char *password);
int validate_number(const char *input, int min, int max);

// Server functions
void *handle_client(void *client_socket);
int authenticate_user(int client_socket, char *user_id, char *password, enum Role *role);
void serve_admin(int client_socket, char *user_id);
void serve_student(int client_socket, char *user_id);
void serve_faculty(int client_socket, char *user_id);

// File operation functions
int read_lock(int fd);
int write_lock(int fd);
int unlock(int fd);
int add_user(char *id, char *password, enum Role role);
int add_student(char *id, char *name);
int add_faculty(char *id, char *name);
int activate_deactivate_student(char *id, int activate);
int update_student(char *id, char *new_name);
int update_faculty(char *id, char *new_name);
int add_course(char *id, char *name, char *faculty_id, int seats);
int update_course(char *id, char *new_name, int new_seats);
int remove_course(char *id);
int enroll_course(char *student_id, char *course_id);
int unenroll_course(char *student_id, char *course_id);
char *view_enrolled_courses(char *student_id);
char *view_course_enrollments(char *course_id);
char *view_all_courses();
char *view_faculty_courses(char *faculty_id);
char *view_all_students();
char *view_all_faculty();
int change_password(char *user_id, char *new_password);
void initial_setup();

#endif
