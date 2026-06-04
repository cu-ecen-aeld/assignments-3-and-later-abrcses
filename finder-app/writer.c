#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

int main(int argc, char** argv) {
    openlog(NULL, 0, LOG_USER);

    if (argc < 3) {
        syslog(LOG_ERR, "You have to provide a file path and the file content.");
        return 1;
    }

    FILE* f = fopen(argv[1], "w");
    if (f == NULL) {
        syslog(LOG_ERR, "Could not open file %s: %s", argv[1], strerror(errno));
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
    fprintf(f, "%s\n", argv[2]);

    fclose(f);
    closelog();
    
    return 0;
}