#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <utmp.h>

#define MAX_RESPONSE_SIZE 8192
#define MAX_PATH_SIZE 4096
#define MAX_BUFF_SIZE 2048
#define MAX_COMMAND_SIZE 1024
#define MAX_WORDS 100
#define MAX_USERNAME_SIZE 50


#define CHECK(condition, message) \
    if (!(condition))             \
    {                             \
        perror(message);          \
        exit(EXIT_FAILURE);       \
    }


const char *READ_FIFO_NAME = "client_server_fifo";
int client_server_fifo_d;
const char *WRITE_FIFO_NAME = "server_client_fifo";
int server_client_fifo_d;

int LOGGED_FLAG = 0;
int child_parent_pipe[2];
int child_parent_socket[2];

pid_t cpid;
char *process_info[5] = {"Name", "State", "PPid", "Uid", "VmSize"};

struct command_parser {
    char arguments[MAX_WORDS][MAX_COMMAND_SIZE];
    int size;
};

int read_until_eoln(int file_descriptor, char *result);
int read_until_eof(int file_descriptor, char *result);
int parse_command(char *command, struct command_parser* command_arguments);
int is_number(char *word);
void command_parser_deallocate(struct command_parser* command_arguments);

void execute_login(char *username, char *response);
void execute_get_logged_users(char *response);
void execute_get_proc_info();
void execute_logout();
void execute_quit();
void open_fifos();

int main(int argc, char **argv) {
    open_fifos();
    while(1) {
        char command[MAX_COMMAND_SIZE];
        while(read_until_eoln(client_server_fifo_d, command) == -1) {
            LOGGED_FLAG = 0;
            sleep(0);
        }

        struct command_parser command_arguments;
        int parsing_code = parse_command(command, &command_arguments);

        if(parsing_code == 4) {
            execute_quit();
            continue;
        }

        char response[MAX_RESPONSE_SIZE];
        if(parsing_code == -1) 
            strncpy(response, "Invalid command structure! Please enter a valid command.", MAX_RESPONSE_SIZE);
        
        switch (parsing_code) {
            case 0: {
                if(LOGGED_FLAG) 
                    strncpy(response, "An user is already logged!", MAX_RESPONSE_SIZE);
                else 
                    execute_login(command_arguments.arguments[2], response);
                break;
            }
            case 1: {
                if(!LOGGED_FLAG) 
                    strncpy(response, "Please login in order to execute this command", MAX_RESPONSE_SIZE);
                else 
                    execute_get_logged_users(response);
                break;
            }
            case 2: {
                if (!LOGGED_FLAG) 
                    strncpy(response, "Please login in order to execute this command", MAX_RESPONSE_SIZE);
                else 
                    execute_get_proc_info(command_arguments.arguments[2], response);
                break;
            }
            case 3: {
                if(!LOGGED_FLAG) 
                    strncpy(response, "Please login in order to execute this command", MAX_RESPONSE_SIZE);
                else 
                    execute_logout(response);
                break;
            }
        }
        int len = strlen(response), write_res;
        write_res = write(server_client_fifo_d, &len, sizeof(int));
        CHECK(write_res != -1, "[S] Error at write(): ");
        write_res = write(server_client_fifo_d, response, len);
        CHECK(write_res != -1, "[S] Error at write(): ");
    }
}

void open_fifos() {
    if (access(READ_FIFO_NAME, F_OK) == -1)
        CHECK(mknod(READ_FIFO_NAME, S_IFIFO | 0666, 0) != -1, "[S] Error at mknod(): ");
    client_server_fifo_d = open(READ_FIFO_NAME, O_RDONLY);
    CHECK(client_server_fifo_d != -1, "[S] Error at open(): ");

    if (access(WRITE_FIFO_NAME, F_OK) == -1)
        CHECK(mknod(WRITE_FIFO_NAME, S_IFIFO | 0666, 0) != -1, "[S] Error at mknod(): ");
    server_client_fifo_d = open(WRITE_FIFO_NAME, O_WRONLY);
    CHECK(server_client_fifo_d != -1, "[S] Error at open(): ");
}

int read_until_eoln(int file_descriptor, char *result) {
    int cursor = 0;
    while (1) {
        char read_ch;
        int read_res = read(file_descriptor, &read_ch, sizeof(char));
        CHECK(read_res != -1, "[S] Error at read(): ");
        if(!read_res) {
            result[cursor] = '\0';
            return !cursor ? -1 : cursor;
        }
        if(read_ch == '\n') {
            result[cursor] = '\0';
            return cursor;
        }
        result[cursor++] = read_ch;
    }
}

int read_until_eof(int file_descriptor, char *result) {
    int cursor = 0;
    while(1) {
        char read_ch;
        int read_res = read(file_descriptor, &read_ch, sizeof(char));
        CHECK(read_res != -1, "[S] Error at read(): ");
        if (!read_res) {
            result[cursor] = '\0';
            return cursor;
        }
        result[cursor++] = read_ch;
    }
}

int parse_command(char *command, struct command_parser *command_arguments) {
    char copy[strlen(command) + 1];
    int nr_words = 0;
    strcpy(copy, command);
    char *word = strtok(copy, " ");

    while(word != NULL) {
        strcpy(command_arguments->arguments[nr_words++], word);
        word = strtok(NULL, " ");
    }

    if(!nr_words)
        return -1;
        
    command_arguments->size = nr_words;
    char *command_name = command_arguments->arguments[0];

    if (!strcmp(command_name, "login")) {
        if(nr_words != 3)
            return -1;
        if (strcmp(command_arguments->arguments[1], ":"))
            return -1;
        return 0;
    }

    if (!strcmp(command_name, "get-logged-users")) {
        if(nr_words != 1)
            return -1;
        return 1;
    }

    if (!strcmp(command_name, "get-proc-info")) {
        if(nr_words != 3) 
            return -1;
        if (strcmp(command_arguments->arguments[1], ":"))
            return -1;
        if (is_number(command_arguments->arguments[2]) == -1)
            return -1;
        return 2;
    }

    if (!strcmp(command_name, "logout")) {
        if(nr_words != 1) 
            return -1;
        return 3;
    }

    if(!strcmp(command_name, "quit")) {
        if(nr_words != 1) 
            return -1;
        return 4;
    }
    return -1;
}

int is_number(char *word) {
    for(int i = 0; i < strlen(word); ++i) {
        if(!(word[i] >= '0' && word[i] <= '9'))
            return -1;
    }
    return 0;
}

void execute_login(char *logged_user, char *response) {
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, child_parent_socket) != -1, "[S] Error at socketpair(): ");
    cpid = fork();
    if(cpid) {
        CHECK(close(child_parent_socket[1]) != -1, "[SP] Error at close(): ");
        wait(NULL);
        read_until_eof(child_parent_socket[0], response);
        CHECK(close(child_parent_socket[0]) != -1, "[SP] Error at close(): ");
        if(!strcmp(response, "Welcome")) {
            LOGGED_FLAG = 1;
        }
    } else { 
        CHECK(close(child_parent_socket[0]) != -1, "[SC] Error at close(): ");
        int fd = open("user_list.txt", O_RDONLY);
        char valid_user[MAX_USERNAME_SIZE];
        int FOUND_FLAG = 0;
        while(read_until_eoln(fd, valid_user) != -1) {
            printf("%s\n", valid_user);
            if(!strncmp(valid_user, logged_user, MAX_USERNAME_SIZE)) {
                FOUND_FLAG = 1;
                break;
            }
        }

        char return_message[MAX_RESPONSE_SIZE];
        if(FOUND_FLAG)
            strncpy(return_message, "Welcome", MAX_RESPONSE_SIZE);
        else 
            strncpy(return_message, "User not found", MAX_RESPONSE_SIZE);

        int write_res = write(child_parent_socket[1], return_message, strlen(return_message));
        CHECK(write_res != -1, "[SC] Error at write(): ");
        CHECK(close(child_parent_socket[1]) != -1, "[SC] Error at close(): ");
        exit(0);
    }
}

void execute_logout(char *response) {
    LOGGED_FLAG = 0;
    strncpy(response, "Bye :(", MAX_RESPONSE_SIZE);
}

void execute_quit() {
    if(LOGGED_FLAG) 
        LOGGED_FLAG = 0;
}

void execute_get_logged_users(char *response) {
    CHECK(pipe(child_parent_pipe) != -1, "[S] Error at socketpair(): ");
    cpid = fork();
    if(cpid) {
        CHECK(close(child_parent_pipe[1]) != -1, "[SP] Error at close(): ");
        wait(NULL);
        read_until_eof(child_parent_pipe[0], response);
        CHECK(close(child_parent_pipe[0]) != -1, "[SP] Error at close(): ");
    } 
    else {
        CHECK(close(child_parent_pipe[0]) != -1, "[SC] Error at close(): ");
        struct utmp *current_utmp;
        char returned_message[MAX_RESPONSE_SIZE];
        int cursor = 0;
        while((current_utmp = getutent()) != NULL) {
            char ut_user[UT_NAMESIZE];
            strncpy(ut_user,  current_utmp -> ut_user, UT_NAMESIZE);
            char ut_host[UT_HOSTSIZE];
            strncpy(ut_host, current_utmp -> ut_host, UT_HOSTSIZE);
            long tv_sec = current_utmp -> ut_tv.tv_sec;
            char user_info[MAX_BUFF_SIZE];
            int utmp_length = snprintf(user_info, MAX_BUFF_SIZE, "Username: %s\nHostname For Remote Login: %s\nTime Entry Was Made: %lds\n\n", ut_user, ut_host, tv_sec);
            CHECK(utmp_length >= 0, "[S] Error at sprintf(): ");
            for(int i = 0; i < utmp_length; ++i) {
                returned_message[cursor++] = user_info[i];
            }
        }
        
        if(cursor && returned_message[cursor - 1] == '\n')
            returned_message[cursor - 1] = '\0';
        else 
            returned_message[cursor] = '\0';

        int write_res = write(child_parent_pipe[1], returned_message, strlen(returned_message));
        CHECK(write_res != -1, "[SC] Error at write(): ");
        CHECK(close(child_parent_pipe[1]) != -1, "[SC] Error at close(): ");
        exit(0);
    }
}

void execute_get_proc_info(char *process_pid, char *response) {
    CHECK(pipe(child_parent_pipe) != -1, "[S] Error at socketpair(): ");
    cpid = fork();
    if(cpid) {
        CHECK(close(child_parent_pipe[1]) != -1, "[SP] Error at close(): ");
        wait(NULL);
        read_until_eof(child_parent_pipe[0], response);
        CHECK(close(child_parent_pipe[0]) != -1, "[SP] Error at close(): ");
    } 
    else {
        CHECK(close(child_parent_pipe[0]) != -1, "[SC] Error at close(): ");
        char searched_file[MAX_PATH_SIZE];
        char return_message[MAX_RESPONSE_SIZE];
        CHECK(snprintf(searched_file, MAX_PATH_SIZE, "/proc/%s/status", process_pid) >= 0, "[S] Error at snprintf(): ");
        if (access(searched_file, F_OK) == -1) {
            strncpy(return_message, "There isn't a process with this pid. Try again.", MAX_RESPONSE_SIZE);
        }
        else {
            int fd = open(searched_file, O_RDONLY), cursor = 0;
            CHECK(fd != -1, "[SC] Error at fopen(): ");
            char file_line[MAX_BUFF_SIZE];
            int line_size;
            while ((line_size = read_until_eoln(fd, file_line)) != -1) {
                for (int i = 0; i < 5; ++i) {
                    if (strstr(file_line, process_info[i])) {
                        for (int j = 0; j < line_size; ++j) {
                            return_message[cursor++] = file_line[j];
                        }
                        return_message[cursor++] = '\n';
                    }
                }
            }
        }
        int write_res = write(child_parent_pipe[1], return_message, strlen(return_message));
        CHECK(write_res != -1, "[SC] Error at write(): ");
        CHECK(close(child_parent_pipe[1]) != -1, "[SC] Error at close(): ");
        exit(0);
    }

}   

