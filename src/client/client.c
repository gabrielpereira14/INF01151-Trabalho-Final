#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <errno.h>
#include <linux/limits.h>

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))
#define OK       0
#define NO_INPUT 1
#define MAX_INPUT_SIZE 128
#define MAX_COMMAND 13
#define MAX_ARGUMENT 115

char sync_dir_path[PATH_MAX];

int create_sync_dir(){
    if (mkdir(sync_dir_path, 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir() error");
            return 1;
        }
    }
    return 0;
}


int get_command(char* command, char* arg)
{
    char input[MAX_INPUT_SIZE];
    
    fflush(stdout);
    if(fgets(input, sizeof(input), stdin) == NULL)
        return NO_INPUT;


    input[strcspn(input, "\n")] = 0;
    
    sscanf(input,"%12s %114s", command, arg);
    
    return OK;
}



void *start_console_input_thread(){
    char command[MAX_COMMAND] = "\0";
    char path[MAX_ARGUMENT] = "\0";

    printf("Client started!");


    while (strcmp(command, "exit") != 0)
    {
        command[0] = '\0';
        path[0] = '\0';

        get_command(command,path);

        //printf("Command: %s, Argument: %s\n", command, path);
        
        if (strcmp(command, "exit") == 0)
        {
            printf("Client closed\n");
            break;
        }
        else if (strcmp(command, "get_sync_dir") == 0)
        {
            //Tem que ver se tem no server antes de criar

            create_sync_dir();
        }
        else if (strcmp(command, "list_client") == 0)
        {
            printf("TODO: list_client\n");
        }
        else if (strcmp(command, "list_server") == 0)
        {
            printf("TODO: list_server\n");
        }
        else if (strcmp(command, "upload") == 0)
        {
            printf("TODO: upload\n");
        }
        else if (strcmp(command, "delete") == 0)
        {
            printf("TODO: delete\n");
        }
        else if (strcmp(command, "download") == 0)
        {
            printf("TODO: download\n");
        }
        else
        {
            printf("Unknown command: %s\n", command);
        }
      
    }
    pthread_exit(0);
}

void *start_directory_watcher_thread() {
    int fd, wd;
    char buffer[EVENT_BUF_LEN];

    fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        perror("inotify_init1");
        pthread_exit(NULL);
    }

    wd = -1;

    do{
        usleep(200);
        wd = inotify_add_watch(fd, sync_dir_path, IN_CREATE | IN_CLOSE_WRITE);
    }while(wd < 0);
  

    //printf("Watching '%s' for IN_CREATE and IN_CLOSE_WRITE...\n", sync_dir_path);

    while (1) {
        ssize_t length = read(fd, buffer, EVENT_BUF_LEN);

        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);
                continue;
            } else {
                perror("read");
                break;
            }
        }

        ssize_t i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            if (event->mask & IN_CREATE) {
                //printf("IN_CREATE: %s\n", event->name);
            }

            if (event->mask & IN_CLOSE_WRITE) {
                //printf("IN_CLOSE_WRITE: %s\n", event->name);
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    pthread_exit(NULL);
}

int set_sync_dir_path(){
    if (getcwd(sync_dir_path, sizeof(sync_dir_path)) == NULL) {
        perror("getcwd() error");
        return 1;
    }

    if (strlen(sync_dir_path) + strlen("/sync_dir") >= sizeof(sync_dir_path)) {
        fprintf(stderr, "Path too long\n");
        return 1;
    }
    strcat(sync_dir_path, "/sync_dir");
    return 0;
}



int main(){ 

    if(set_sync_dir_path() != 0){
        return 1;
    }

    pthread_t console_thread, file_watcher_thread;
    pthread_create(&console_thread, NULL, start_console_input_thread, NULL);
    pthread_create(&file_watcher_thread, NULL, start_directory_watcher_thread, NULL);

    pthread_join(console_thread, NULL);

    return 0;
}