#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>

#define MAX_SIZE 1024
#define CHECK(condition, message) \
    if (!(condition))             \
    {                             \
        perror(message);          \
        exit(EXIT_FAILURE);       \
    }

const char *WRITE_FIFO_NAME = "client_server_fifo";
int client_server_fifo_d;
const char *READ_FIFO_NAME = "server_client_fifo";
int server_client_fifo_d;

void open_fifos() {
    if (access(WRITE_FIFO_NAME, F_OK) == -1)
        CHECK(mknod(WRITE_FIFO_NAME, S_IFIFO | 0666, 0) != -1, "[C] Error at mknod(): ");
    client_server_fifo_d = open(WRITE_FIFO_NAME, O_WRONLY);
    CHECK(client_server_fifo_d != -1, "[C] Error at open(): ");

    if (access(READ_FIFO_NAME, F_OK) == -1)
        CHECK(mknod(READ_FIFO_NAME, S_IFIFO | 0666, 0) != -1, "[C] Error at mknod(): ");
    server_client_fifo_d = open(READ_FIFO_NAME, O_RDONLY);
    CHECK(server_client_fifo_d != -1, "[C] Error at open(): ");
}

int main(int argc, char **argv) {

    open_fifos();

    while(1) {
        printf("-> ");
        fflush(stdout);
        char input[MAX_SIZE];
        if(fgets(input, MAX_SIZE, stdin) == NULL) {
            fprintf(stderr, "Something unexpected was typed by the client");
            break;
        }
        CHECK(write(client_server_fifo_d, input, strlen(input)) != -1, "[C] Error at write(): ");
        if(!strcmp(input, "quit\n")) {
            exit(0);
        }
        int response_len;
        CHECK(read(server_client_fifo_d, &response_len, sizeof(int)) != -1, "[C] Error at reading the header");
        char response[response_len + 1];
        CHECK(read(server_client_fifo_d, response, response_len) != -1, "[C] Error at reading the response(): ");
        response[response_len] = '\0';
        printf("%s\n", response);
    }
    CHECK(close(client_server_fifo_d) != -1, "[C] Error at close(): ");
    CHECK(close(server_client_fifo_d) != -1, "[C] Error at close(): ");
}