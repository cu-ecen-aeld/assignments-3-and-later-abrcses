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

int terminate = 0;

static void signal_handler(int signal_number) {
    syslog(LOG_DEBUG, "Caught signal, exiting\n");
    terminate = 1;
}

void send_file_back(FILE* file, int conn_fd) {
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

void listen_and_reply(int sock_fd) {
    if (listen(sock_fd, 10) != 0) {
        syslog(LOG_ERR, "Error in listen(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    FILE* f = fopen("/var/tmp/aesdsocketdata", "a+");
    if (f == NULL) {
        syslog(LOG_ERR, "Could not open file %s: %s\n", "/var/tmp/aesdsocketdata", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in conn_addr;
    socklen_t addr_len = sizeof(conn_addr);
    int conn_fd;
    while(1) {
        syslog(LOG_DEBUG, "Listening for incoming connection...\n");

        conn_fd = accept(sock_fd, (struct sockaddr*) &conn_addr, &addr_len);
        if (conn_fd == -1) {
            if (terminate) {
                break;
            }
            syslog(LOG_ERR, "Error in accept(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        struct in_addr* ip_addr = &conn_addr.sin_addr;
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(conn_addr.sin_family, ip_addr, ip_str, sizeof(ip_str)) == NULL) {
            syslog(LOG_ERR, "Error in inet_ntop(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        syslog(LOG_DEBUG, "Accepted connection from %s\n", ip_str);

        int max_bytes = 100;
        char* buf = malloc(max_bytes);
        if (buf == NULL) {
            syslog(LOG_ERR, "Error in malloc(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        int num_bytes;
        int bytes_sofar = 0;
        while ((num_bytes = recv(conn_fd, buf + bytes_sofar, max_bytes - 1, 0)) > 0) {
            bytes_sofar += num_bytes;
            if (buf[bytes_sofar - 1] == '\n') {
                buf[bytes_sofar] = '\0';
                if (fprintf(f, "%s", buf) < 0) {
                    syslog(LOG_ERR, "Error in fprintf(): %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                send_file_back(f, conn_fd);
                bytes_sofar = 0;
            }
            buf = realloc(buf, bytes_sofar + max_bytes);
            if (buf == NULL) {
                syslog(LOG_ERR, "Error in realloc(): %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        free(buf);
        if (num_bytes == -1) {
            if (terminate) {
                close(conn_fd);
                break;
            }
            syslog(LOG_ERR, "Error in recv(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (fflush(f) == EOF) {
            syslog(LOG_ERR, "Error in fflush(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        close(conn_fd);

        if (terminate) {
            break;
        }
    }

    fclose(f);
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
    if (sigaction(SIGTERM, &action, NULL) != 0 || sigaction(SIGINT, &action, NULL)) {
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
        return EXIT_FAILURE;
    }

    int sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_fd == -1) {
        syslog(LOG_ERR, "Error in socket(): %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        syslog(LOG_ERR, "Error in setsockopt(): %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (bind(sock_fd, res->ai_addr, res->ai_addrlen) != 0) {
        syslog(LOG_ERR, "Error in bind(): %s\n", strerror(errno));
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
            listen_and_reply(sock_fd);
        }
    } else {
        listen_and_reply(sock_fd);
    }
    
    closelog();

    return EXIT_SUCCESS;
}