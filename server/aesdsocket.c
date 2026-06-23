#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include "queue.h"

struct thread_data {
    pthread_t tid;
    int conn_fd;
    int completed;
};

struct entry {
    struct thread_data* tdata;
    SLIST_ENTRY(entry) entries;
};

SLIST_HEAD(slisthead, entry);

FILE* file = NULL;
pthread_mutex_t file_mutex;
int terminate = 0;

static void signal_handler(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) {
        syslog(LOG_DEBUG, "Caught signal, exiting\n");
        terminate = 1;
    } else if (signal_number == SIGALRM) {
        char timestr[50];        

        time_t t = time(NULL);
        struct tm *localt = localtime(&t);
        if (localt == NULL) {
            syslog(LOG_ERR, "Error in localtime(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        strftime(timestr, sizeof(timestr), "timestamp: %a, %d %b %Y %T %z", localt);

        if (pthread_mutex_lock(&file_mutex) != 0) {
            syslog(LOG_ERR, "Error in pthread_mutex_lock(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (fprintf(file, "%s\n", timestr) < 0) {
            syslog(LOG_ERR, "Error in fprintf(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (pthread_mutex_unlock(&file_mutex) != 0) {
            syslog(LOG_ERR, "Error in pthread_mutex_unlock(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        alarm(10);
    }
}

void send_file_back(int conn_fd) {
    if (fseek(file, 0, SEEK_SET) == -1) {
        syslog(LOG_ERR, "Error in fseek(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    int max_bytes = 100;
    char buf[max_bytes];
    int num_bytes;
    while ((num_bytes = fread(buf, 1, max_bytes, file)) > 0) {
        if (send(conn_fd, buf, num_bytes, 0) == -1) {
            syslog(LOG_ERR, "Error in send(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    if (ferror(file)) {
        syslog(LOG_ERR, "Could not read file %s: %s\n", "/var/tmp/aesdsocketdata", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void* handleConnection(void* arg) {
    struct thread_data* tdata = (struct thread_data*) arg;
    int conn_fd = tdata->conn_fd;
    int max_bytes = 100;
    char* buf = malloc(max_bytes);
    if (buf == NULL) {
        syslog(LOG_ERR, "Error in malloc(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    int num_bytes;
    int bytes_sofar = 0;
    while ((num_bytes = recv(conn_fd, buf + bytes_sofar, max_bytes - 1, 0)) > 0
            || (num_bytes < 0 && !terminate && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))) {
        if (num_bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            continue;
        }
        bytes_sofar += num_bytes;
        if (buf[bytes_sofar - 1] == '\n') {
            buf[bytes_sofar] = '\0';
            if (pthread_mutex_lock(&file_mutex) != 0) {
                syslog(LOG_ERR, "Error in pthread_mutex_lock(): %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (fprintf(file, "%s", buf) < 0) {
                syslog(LOG_ERR, "Error in fprintf(): %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            send_file_back(conn_fd);
            if (pthread_mutex_unlock(&file_mutex) != 0) {
                syslog(LOG_ERR, "Error in pthread_mutex_unlock(): %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            bytes_sofar = 0;
        }
        buf = realloc(buf, bytes_sofar + max_bytes);
        if (buf == NULL) {
            syslog(LOG_ERR, "Error in realloc(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    free(buf);
    if (num_bytes == -1 && !terminate) {
        syslog(LOG_ERR, "Error in recv(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_lock(&file_mutex) != 0) {
        syslog(LOG_ERR, "Error in pthread_mutex_lock(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (fflush(file) == EOF) {
        syslog(LOG_ERR, "Error in fflush(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_unlock(&file_mutex) != 0) {
        syslog(LOG_ERR, "Error in pthread_mutex_unlock(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    close(conn_fd);

    tdata->completed = 1;
    return NULL;
}

void listen_and_accept(int sock_fd) {
    if (listen(sock_fd, 10) != 0) {
        syslog(LOG_ERR, "Error in listen(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    file = fopen("/var/tmp/aesdsocketdata", "a+");
    if (file == NULL) {
        syslog(LOG_ERR, "Could not open file %s: %s\n", "/var/tmp/aesdsocketdata", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&file_mutex, NULL) != 0) {
        syslog(LOG_ERR, "Error in pthread_mutex_init(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    alarm(10);

    struct slisthead head;
    SLIST_INIT(&head);
    struct entry* e;
    struct entry* te;
    
    struct sockaddr_in conn_addr;
    socklen_t addr_len = sizeof(conn_addr);
    int conn_fd;
    while(1) {
        syslog(LOG_DEBUG, "Listening for incoming connection...\n");

        conn_fd = accept(sock_fd, (struct sockaddr*) &conn_addr, &addr_len);
        if (conn_fd == -1) {
            if (terminate) {
                break;
            } else if (errno == EINTR) {
                continue;
            }
            syslog(LOG_ERR, "Error in accept(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof tv);

        struct in_addr* ip_addr = &conn_addr.sin_addr;
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(conn_addr.sin_family, ip_addr, ip_str, sizeof(ip_str)) == NULL) {
            syslog(LOG_ERR, "Error in inet_ntop(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        syslog(LOG_DEBUG, "Accepted connection from %s\n", ip_str);

        struct thread_data* new_tdata = (struct thread_data*) malloc(sizeof(struct thread_data));
        if (new_tdata == NULL) {
            syslog(LOG_ERR, "Error in malloc(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        new_tdata->conn_fd = conn_fd;
        new_tdata->completed = 0;
        struct entry* new_entry = (struct entry*) malloc(sizeof(struct entry));
        if (new_entry == NULL) {
            syslog(LOG_ERR, "Error in malloc(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        new_entry->tdata = new_tdata;

        if (pthread_create(&new_tdata->tid, NULL, handleConnection, (void*) new_tdata) != 0) {
            syslog(LOG_ERR, "Error in pthread_create(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        SLIST_INSERT_HEAD(&head, new_entry, entries);
        
        SLIST_FOREACH_SAFE(e, &head, entries, te) {
            if (e->tdata->completed) {
                if (pthread_join(e->tdata->tid, NULL) != 0) {
                    syslog(LOG_ERR, "Error in pthread_join(): %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                SLIST_REMOVE(&head, e, entry, entries);
                free(e->tdata);
                free(e);
            }
        }

        if (terminate) {
            break;
        }
    }

    SLIST_FOREACH_SAFE(e, &head, entries, te) {
        if (pthread_join(e->tdata->tid, NULL) != 0) {
            syslog(LOG_ERR, "Error in pthread_join(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        SLIST_REMOVE(&head, e, entry, entries);
        free(e->tdata);
        free(e);
    }

    if (pthread_mutex_destroy(&file_mutex) != 0) {
        syslog(LOG_ERR, "Error in pthread_mutex_destroy(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    fclose(file);
    if (remove("/var/tmp/aesdsocketdata") == -1) {
        syslog(LOG_ERR, "Error in remove(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    close(sock_fd);
}

int main(int argc, char** args) {
    openlog(NULL, 0, LOG_USER);

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;
    if (sigaction(SIGTERM, &action, NULL) != 0 || sigaction(SIGINT, &action, NULL) != 0 || sigaction(SIGALRM, &action, NULL) != 0) {
        syslog(LOG_ERR, "Error in sigaction(): %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    struct addrinfo hints;
    struct addrinfo* res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(NULL, "9000", &hints, &res);
    if (ret != 0) {
        syslog(LOG_ERR, "Error in inet_ntop(): %s\n", gai_strerror(ret));
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    int sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_fd == -1) {
        syslog(LOG_ERR, "Error in socket(): %s\n", strerror(errno));
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        syslog(LOG_ERR, "Error in setsockopt(): %s\n", strerror(errno));
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    if (bind(sock_fd, res->ai_addr, res->ai_addrlen) != 0) {
        syslog(LOG_ERR, "Error in bind(): %s\n", strerror(errno));
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    freeaddrinfo(res);

    if (argc > 1 && strcmp(args[1], "-d") == 0) {
        int pid = fork();
        if (pid == -1) {
            syslog(LOG_ERR, "Error in fork(): %s\n", strerror(errno));
            return EXIT_FAILURE;
        } else if (pid == 0) {
            setsid();
            chdir("/");
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            open("/dev/null", O_RDONLY);
            open("/dev/null", O_WRONLY);
            open("/dev/null", O_WRONLY);
            listen_and_accept(sock_fd);
        }
    } else {
        listen_and_accept(sock_fd);
    }
    
    closelog();

    return EXIT_SUCCESS;
}