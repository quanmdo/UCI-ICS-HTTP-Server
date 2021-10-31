#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/ip.h>
#include <pthread.h>  // pthread
#include <stdbool.h>  // true/false
#include <stdio.h>
#include <stdlib.h>     // atoi
#include <string.h>     // memset
#include <sys/queue.h>  //queues
#include <sys/socket.h>
#include <sys/stat.h>     // stat, st_size
#include <sys/syscall.h>  //geting thread ids for debugging
#include <sys/types.h>    //
#include <unistd.h>       // write

#include "List.h"         // Old code from CSE101, will be treated like a queue
#define BUFFER_SIZE 4096  // 4096 = x86 page size

// ======================================= //
// =========== LOGGING STRUCTS =========== //
// ======================================= //

// FOR LOG QUEUE OF HTTP_OBJECTS
// private NodeObj type "Nodeobj refers to this struct w/ data/next/prev"
typedef struct LogNodeObj {
    struct httpObject *data;
    struct NodeObj *next;
    struct NodeObj *prev;
} LogNodeObj;

// private Node type "node refers to a Nodeobj pointer"
typedef LogNodeObj *LogNode;

// private ListObj type
typedef struct LogListObj {
    int length;
    // int index;
    LogNode front;
    LogNode back;
    LogNode cursor;
} LogListObj;

typedef struct LogListObj *LogList;

// ======================================= //
// =========== SERVER STRUCTS ============ //
// ======================================= //

// Keeps track of all the components related to a HTTP message
typedef struct httpObject {
    char method[150];       // PUT, HEAD, GET //5
    char fileName[2000];    // what is the file we are worried about //32
    char httpVersion[150];  // HTTP/1.1
    ssize_t contentLength;
    char buffer[BUFFER_SIZE];

    bool fail;
    bool skiplogging;
    ssize_t code;

    uint16_t currentEntries;  // for health check
    uint16_t currentErrors;
} httpObject;

// To avoid global variables, this will get passed around
typedef struct varShareObject {
    bool useLogging;
    char logName[2000];  // for sharing logfile name
    int logFd;
    pthread_mutex_t condMutex;
    pthread_mutex_t varMutex;
    pthread_mutex_t queueMutex;
    pthread_cond_t cond;  // for sleep/wait
    List queue;           // for holding sockfd's

    // LOGGING
    LogList logQueue;
    pthread_mutex_t logMutex;
    pthread_cond_t logCond;  // for sleep/wait
    pthread_mutex_t logCondMutex;

    uint16_t totalEntries;  // for health check
    uint16_t totalErrors;
} varShareObject;

// =============================================== //
// ============== LOG QUEUE FUNCS ================ //
// =============================================== //

/*
    (these are just copies of funcs form List.c modified
    to be of type httpObject instead of int)
*/

// Returns reference to new Node object. Initializes next and data fields.
// Private.
LogNode newLogNode(httpObject *data) {
    LogNode N = malloc(sizeof(LogNodeObj));
    N->data = data;
    N->next = NULL;
    N->prev = NULL;
    return N;
}

// Frees heap memory pointed to by *pN, sets *pN to NULL.
// Private.
void freeLogNode(LogNode *pN) {
    if (pN != NULL && *pN != NULL) {
        free(*pN);
        *pN = NULL;
    }
}

// Creates and returns a new empty List.
LogList newLogList(void) {
    LogList L = malloc(sizeof(LogListObj));
    L->length = 0;
    // L->index = -1;
    L->front = NULL;
    L->back = NULL;
    L->cursor = NULL;
    return (L);
}

// Returns the number of elements in L.
int lengthLogList(LogList L) {
    if (L == NULL) {
        printf("List Error: calling length() on NULL List reference\n");
        EXIT_FAILURE;
    }
    return (L->length);
}

// Returns front element of L. Pre: length()>0
void *frontLogList(LogList L) {
    if (L == NULL) {
        printf("List Error: calling front() on NULL List reference\n");
        exit(1);
    }
    if (lengthLogList(L) == 0) {
        printf("List Error: calling front() on on an empty List \n");
        exit(1);
    }
    return (L->front->data);
}
// Insert new element into L. If L is non-empty,
// insertion takes place after back element.
void appendLogList(LogList L, httpObject *data) {
    if (L == NULL) {
        printf("List Error: calling append() on NULL List reference\n");
        exit(1);
    }

    LogNode temp = newLogNode(data);
    if (lengthLogList(L) == 0)
        L->front = L->back = temp;
    else {
        L->back->next = temp;
        temp->prev = L->back;
        L->back = temp;
        temp->next = NULL;
    }
    L->length++;
}
// deleteFront()
// Delete the front element. Pre: length()>0
void deleteFrontLogList(LogList L) {
    if (lengthLogList(L) == 0) {
        printf("List Error: calling deleteFront() on on an empty List \n");
        exit(1);
    }
    if (L == NULL) {
        printf("List Error: calling deleteFront() on NULL List reference\n");
        exit(1);
    }

    LogNode temp = L->front;
    if (temp == L->cursor) {
        L->cursor = NULL;
    }

    if (L->length > 1) {
        L->front = L->front->next;
        L->front->prev = NULL;
    } else  // one element list
        L->front = L->back = NULL;

    L->length--;
    freeLogNode(&temp);
}

// ================================================ //
// ================= SERVER FUNCS ================= //
// ================================================ //

/*
 * Function:  logWrite()
 * --------------------
 * Writes request details to the log file
 *
 *  message: httpObject storing info about the request
 *
 *  vars: varShareObject storing sync. vars and logging data
 */
void logWrite(struct httpObject *message, struct varShareObject *vars) {
    // pid_t x = syscall(__NR_gettid);
    // printf("\033[0;32m %d Logging /%s %s to %s \n\033[0m", x,
    // message->method,
    //        message->fileName, vars->logName);  //

    // reopen log file
    vars->logFd = open(vars->logName, O_WRONLY, S_IRWXU);

    off_t tempOffset;
    int16_t charCount;
    char logBuffer[BUFFER_SIZE];
    // use log file size as starting offset
    struct stat st;
    stat(vars->logName, &st);
    tempOffset = st.st_size;

    // if you have to write a failure
    if (message->fail) {
        ssize_t failcode = message->code;

        // load up error buffer
        charCount = sprintf(
            logBuffer, "FAIL: %s /%s %s --- response %ld\n========\n",
            message->method, message->fileName, message->httpVersion, failcode);

        pthread_mutex_lock(&(vars->varMutex));
        vars->totalEntries += 1;
        vars->totalErrors += 1;
        pthread_mutex_unlock(&(vars->varMutex));

        pwrite(vars->logFd, logBuffer, charCount, tempOffset);
        return;
    }
    // logging healthchecks
    if (strcmp(message->fileName, "healthcheck") == 0) {
        pthread_mutex_lock(&(vars->varMutex));
        int16_t errors = message->currentErrors;
        int16_t entries = message->currentEntries;
        pthread_mutex_unlock(&(vars->varMutex));

        char healthBuf[message->contentLength];
        int16_t charCount1 = sprintf(healthBuf, "%d\n%d", errors, entries);

        charCount = sprintf(logBuffer, "%s /healthcheck length %ld\n",
                            message->method, message->contentLength);

        pwrite(vars->logFd, logBuffer, charCount, tempOffset);
        tempOffset += charCount;
        // write hex to log
        // * I assume the healthcheck response length will be less than 20bytes
        // for our purposes over 20b healthcheck would be a 9-20 digit number
        // for one of the 2 values
        ssize_t paddedBytes = 0;
        ssize_t bytesRecvd = 0;

        // print out data by hex chunk
        for (int i = 0; i < charCount1; i++) {
            if (i % 20 == 0) {
                sprintf(logBuffer, "%08zd ", paddedBytes);
                pwrite(vars->logFd, logBuffer, 9, tempOffset);
                tempOffset += 9;
            }
            if (i < charCount1 - 1) {
                sprintf(logBuffer, "%02x ", (unsigned char)(healthBuf)[i]);
                bytesRecvd = pwrite(vars->logFd, logBuffer, 3, tempOffset);
            } else {
                sprintf(logBuffer, "%02x\n", (unsigned char)(healthBuf)[i]);
                bytesRecvd = pwrite(vars->logFd, logBuffer, 3, tempOffset);
            }
            if (bytesRecvd < 0) {
                printf(" ERRNO:%d\n", errno);
                printf(" vars->logFd :%d\n", vars->logFd);
                // exit(1);
            }
            tempOffset += 3;
        }

        pwrite(vars->logFd, "========\n", 9, tempOffset);
        // count healthcheck, but only increment after responding
        pthread_mutex_lock(&(vars->varMutex));
        vars->totalEntries += 1;
        pthread_mutex_unlock(&(vars->varMutex));
        return;
    }
    // open the file on the server to read from
    int serverFileDesc = open(message->fileName, O_RDONLY, S_IRWXG);
    if (serverFileDesc < 0) {
        printf("COULDN't OPEN FILE ON SERVER\n");
        return;
    }
    // get content len
    struct stat stats;
    stat(message->fileName, &stats);
    message->contentLength = stats.st_size;

    // calculate and set the offset
    // print 1st line

    charCount = sprintf(logBuffer, "%s /%s length %ld\n", message->method,
                        message->fileName, message->contentLength);

    // increment offset vars
    pthread_mutex_lock(&(vars->varMutex));
    vars->totalEntries += 1;
    pthread_mutex_unlock(&(vars->varMutex));
    // write the header line to the log file
    pwrite(vars->logFd, logBuffer, charCount, tempOffset);
    tempOffset += charCount;

    // loop throught the file and print to the log
    ssize_t bytesRead = 0;
    ssize_t paddedBytes = 0;

    ssize_t bytesRecvd = 0;
    if ((message->contentLength != 0) &&
        (strcmp(message->method, "HEAD") != 0) &&
        (strcmp(message->fileName, "healthcheck") != 0)) {
        while ((bytesRead = read(serverFileDesc, message->buffer, 20)) > 0) {
            // write 20 B to log
            // print bytebufffer to log, adjust offset
            sprintf(logBuffer, "%08zd ", paddedBytes);
            pwrite(vars->logFd, logBuffer, 9, tempOffset);
            tempOffset += 9;

            // print out data by hex chunk
            for (int i = 0; i < bytesRead; i++) {
                if (i < bytesRead - 1) {
                    sprintf(logBuffer, "%02x ",
                            (unsigned char)(message->buffer)[i]);
                    /*
                    02: Left-pads the number with 2 zeroes
                    x: Unsigned hexadecimal integer
                    */
                    bytesRecvd = pwrite(vars->logFd, logBuffer, 3, tempOffset);
                } else {
                    sprintf(logBuffer, "%02x\n",
                            (unsigned char)(message->buffer)[i]);
                    bytesRecvd = pwrite(vars->logFd, logBuffer, 3, tempOffset);
                }
                if (bytesRecvd < 0) {
                    printf(" ERRNO:%d\n", errno);
                    printf(" vars->logFd :%d\n", vars->logFd);
                    // exit(1);
                }
                tempOffset += 3;
                // if log buffer is full then pwrite its contents
            }
            paddedBytes += 20;
        }
    }
    pwrite(vars->logFd, "========\n", 9, tempOffset);

    // close(vars->logFd);
    // printf("\033[0;32m %d FINISHED Logging %s %s to %s \n\033[0m", x,
    //       message->method, message->fileName, vars->logName);
}

/*----------------------------------------------------------------------------*/

/*
 * Function:  putFile()
 * --------------------
 * Executes PUT requests
 *
 *  client_sockd: file descriptor of the client socket
 *
 *  message: httpObject storing info about the request
 */
void putFile(int client_sockd, struct httpObject *message) {
    /*
        1) get content length from the PUT header
        2) use open to create file or overwrite preexisting one
        3) loop recv and write out bytes into new file
        4) send header
    */

    // get/skip the first token in the request
    // (everything up to ...Content-Length:)
    char *token = strtok(message->buffer, "\r\n");
    // get integer after "Content-Length: "
    while (token != NULL) {
        sscanf(token, "Content-Length: %ld", &(message->contentLength));
        token = strtok(NULL, "\r\n");
    }

    // create a new file with appropriate name
    int newFileDesc = open(message->fileName, O_RDWR | O_TRUNC | O_CREAT, 0666);
    // 0644 S_IRWXU

    // Error check permissions/file properties
    struct stat stats;
    stat(message->fileName, &stats);

    if ((newFileDesc == -1) || (S_ISDIR(stats.st_mode))) {
        dprintf(client_sockd,
                "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");

        message->fail = true;
        message->code = 403;
        return;
    }

    // check total bytes written against content length to break loop
    ssize_t bytesRecvd, bytesWritten = 0;
    // ssize_t paddedBytes = 0;

    // receiving 20 bytes at time now instead of BUFFER_SIZE
    if (message->contentLength != 0) {
        while ((bytesRecvd =
                    recv(client_sockd, message->buffer, BUFFER_SIZE, 0)) > 0) {
            bytesWritten += write(newFileDesc, message->buffer, bytesRecvd);
            if (bytesWritten == message->contentLength) break;
        }
    }

    // send 201 header if everything went smoothly, 500 other wise
    if (bytesWritten != -1) {
        dprintf(client_sockd,
                "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n");
    } else {
        dprintf(
            client_sockd,
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
    }
    close(newFileDesc);
}

/*----------------------------------------------------------------------------*/

/*
 * Function:  getFile()
 * --------------------
 * Executes GET + HEAD requests
 *
 *  client_sockd: file descriptor of the client socket
 *
 *  message: httpObject storing info about the request
 *
 *  vars: varShareObject storing sync. vars and logging data
 */
void getFile(int client_sockd, struct httpObject *message,
             struct varShareObject *vars) {
    /*
     *  1) Get content length
     *  2) Check for errors from opening
     *  3) Send OK header (return here if HEAD)
     *  4) Read the local file then write it to the socket descriptor
     *
     */
    // Error check permissions/file properties
    struct stat stats;
    stat(message->fileName, &stats);

    // vars for logging healthcheck
    int16_t charCount;
    char logBuffer[BUFFER_SIZE];

    // take care of healthcheck
    if ((vars->useLogging) && (strcmp(message->fileName, "healthcheck") == 0) &&
        (strcmp(message->method, "GET") == 0)) {
        // get length of entries+errors string
        pthread_mutex_lock(&(vars->varMutex));
        int16_t errors = vars->totalErrors;
        int16_t entries = vars->totalEntries;
        pthread_mutex_unlock(&(vars->varMutex));
        message->currentErrors = errors;
        message->currentEntries = entries;

        char tEntr[6];
        sprintf(tEntr, "%d", entries);
        char tErr[6];
        sprintf(tErr, "%d", errors);
        // add one to for the newlline
        // ssize_t len = 1 + strlen(tEntr) + strlen(tErr);
        message->contentLength = 1 + strlen(tEntr) + strlen(tErr);

        char healthBuf[message->contentLength];
        int16_t charCount1 = sprintf(healthBuf, "%d\n%d", errors, entries);

        charCount =
            sprintf(logBuffer, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n",
                    message->contentLength);

        write(client_sockd, logBuffer, charCount);
        write(client_sockd, healthBuf, charCount1);
        return;
    }

    // 404+403 CHECk
    if (access(message->fileName, F_OK) != 0) {
        dprintf(client_sockd,
                "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");

        message->code = 404;
        message->fail = true;
        return;
    } else if ((S_ISDIR(stats.st_mode)) || !(stats.st_mode & S_IRUSR)) {
        dprintf(client_sockd,
                "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");

        message->code = 404;
        message->fail = true;
        return;
    } else {
        // get the # bytes in the file/content length
        message->contentLength = stats.st_size;
        dprintf(client_sockd, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n",
                message->contentLength);
    }

    // attempt opening the file specidfied
    int serverFileDesc = open(message->fileName, O_RDONLY, S_IRWXG);
    // if for some reason the obove eror checker didn't catch the bad file
    if (serverFileDesc == -1) {
        dprintf(client_sockd,
                "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
        return;
    }

    // if it was a head request, you can terminate here
    if (strcmp(message->method, "HEAD") == 0) {
        close(serverFileDesc);
        return;
    }

    ssize_t bytesRead = 0;
    ssize_t bytesWrote = 0;

    // read from file descriptor and write data to socket
    // 20 bytes at a time
    while ((bytesRead = read(serverFileDesc, message->buffer, BUFFER_SIZE)) >
           0) {
        // write to client socket
        bytesWrote = write(client_sockd, message->buffer, bytesRead);
        // Process got interrupted --> internal server error
        if (bytesWrote != bytesRead) {
            dprintf(client_sockd,
                    "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                    "0\r\n\r\n");
            continue;
        }
    }

    close(serverFileDesc);
}

/*----------------------------------------------------------------------------*/

/*
 * Function:  processRequest()
 * --------------------
 * Process the request, error check and call proper function
 *
 *  client_sockd: file descriptor of the client socket
 *
 *  message: httpObject storing info about the request
 *
 *  vars: varShareObject storing sync. vars and logging data
 */
void processRequest(int client_sockd, struct httpObject *message,
                    struct varShareObject *vars) {
    // check if correct request syntax, http version, fileName length,
    // and characters within given ascii constraint
    if ((strcmp(message->httpVersion, "HTTP/1.1") != 0) ||
        (strlen(message->fileName) > 27) ||
        (strspn(message->fileName,
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz012345"
                "6789"
                "-_") != strlen(message->fileName))) {
        dprintf(client_sockd,
                "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");

        message->code = 400;
        message->fail = true;
        return;
    }

    // Requests to HEAD or PUT the healthcheck resource should get a 403
    // error response, even if there is no log_file in use.
    if (strcmp(message->fileName, "healthcheck") == 0) {
        if (strcmp(message->method, "PUT") == 0 ||
            strcmp(message->method, "HEAD") == 0) {
            dprintf(client_sockd,
                    "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");

            message->code = 403;
            message->fail = true;
            return;
        }
    }

    // Send http message to the corresponding func
    if (strcmp(message->method, "GET") == 0 ||
        strcmp(message->method, "HEAD") == 0) {
        getFile(client_sockd, message, vars);
    } else if (strcmp(message->method, "PUT") == 0) {
        putFile(client_sockd, message);  // ,vars);
    } else {
        dprintf(client_sockd,
                "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
        message->code = 400;
        message->fail = true;
    }
}

/*----------------------------------------------------------------------------*/

/*
 * Function:  parseRequest()
 * --------------------
 * Read in & parse HTTP message, data coming in from socket
 *
 *  client_sockd: file descriptor of the client socket
 *
 *  message: httpObject storing info about the request
 *
 *  vars: varShareObject storing sync. vars and logging data
 */
void parseRequest(int client_sockd, struct httpObject *message) {
    ssize_t bytesRecvd = recv(client_sockd, message->buffer, BUFFER_SIZE, 0);
    // printf("\033[0;32m recv done\n\033[0m");

    if (bytesRecvd < 0) {
        printf(" ERRNO:%d\n", errno);
        printf(" client_sockd:%d\n", client_sockd);
        // exit(1);
    }

    // Print server side info
    printf("[+] received %ld bytes from client\n[+] response: ", bytesRecvd);
    write(STDOUT_FILENO, message->buffer, bytesRecvd);
    printf("\n");

    // parse
    sscanf(message->buffer, "%s %s %s \n", message->method, message->fileName,
           message->httpVersion);

    char tempfn[2000];
    strncpy(tempfn, message->fileName + 1, 2000);
    strcpy(message->fileName, tempfn);

    message->fail = false;
}

// ============================================= //
// =============== WORKER THREAD =============== //
// ============================================= //
/*----------------------------------------------------------------------------*/

/*
 * Thread: logThread()
 * --------------------
 * Pop httpObjects off the logging queue
 * Write the request data to the log
 * by calling logWrite()
 *
 *  variables: varShareObject storing sync. vars and logging data
 */
void *logThread(void *variables) {
    varShareObject *vars = (varShareObject *)variables;

    while (true) {
        // Lock/sedate threads until a sockfd is in the queue
        pthread_mutex_lock(&(vars->logCondMutex));
        while (lengthLogList(vars->logQueue) == 0) {
            pthread_cond_wait(&(vars->logCond), &(vars->logCondMutex));
        }
        // pop off oldest request sockfd off queue
        httpObject *msg = frontLogList(vars->logQueue);
        deleteFrontLogList(vars->logQueue);  //
        pthread_mutex_unlock(&(vars->logCondMutex));

        // write to the log
        logWrite(msg, vars);

        close(vars->logFd);
    }
}

// ============================================= //
// =============== WORKER THREAD =============== //
// ============================================= //
/*----------------------------------------------------------------------------*/

/*
 * Thread: workerThread()
 * --------------------
 * Pop client socked descriptors off the queue
 * parse and fulfill the requests
 * Push httpObjects onto the logging queue
 *
 *  variables: varShareObject storing sync. vars and logging data
 */
void *workerThread(void *variables) {
    // pid_t x = syscall(__NR_gettid);
    // printf("\033[0;32m Thread Made %d\n\033[0m", x);

    // cast new pointer to the variable struct
    // directly accessing (vars->name) will give you a compiler error
    varShareObject *vars = (varShareObject *)variables;

    // INIT httpObject
    struct httpObject message;
    message.fail = false;
    if (vars->useLogging)
        message.skiplogging = false;
    else
        message.skiplogging = true;

    while (true) {
        // Lock/sedate threads until a sockfd is in the queue
        pthread_mutex_lock(&(vars->condMutex));

        while (length(vars->queue) == 0) {
            pthread_cond_wait(&(vars->cond), &(vars->condMutex));
        }
        // pop off oldest request sockfd off queue
        int client_sockd = front(vars->queue);
        deleteFront(vars->queue);  //

        // printf("\033[0;32mQueue popped: Thread %d has FD: %d\n\033[0m", x,
        // client_sockd);

        pthread_mutex_unlock(&(vars->condMutex));

        parseRequest(client_sockd, &message);

        if (!(message.fail)) {
            processRequest(client_sockd, &message, vars);
        }
        close(client_sockd);

        if (!(message.skiplogging)) {
            // lock, push a client socket fd onto the queue, then unlock
            pthread_mutex_lock(&(vars->logMutex));
            appendLogList(vars->logQueue, &message);
            pthread_mutex_unlock(&(vars->logMutex));
            // wake up log thread waiting on cond

            pthread_cond_signal(&(vars->logCond));
        }

        printf("[+] server is waiting...\n");
    }
}

// ==================================== //
// =============== MAIN =============== //
// ==================================== //

int main(int argc, char **argv) {
    // Parse through cmdline options
    char *port;
    int threadCount = 4;
    char logFileName[100];
    logFileName[0] = '\0';  //= NULL;

    if (argc == 1 || argc > 6) {
        printf(
            "ERROR: Usage\nRun with ./httpserver [port_num] -N "
            "[num_threads] "
            "-l [log_file]\n");
        exit(1);
    } else if (argc == 2) {
        port = argv[1];
    } else {
        // parse through specified options
        int option;
        while ((option = getopt(argc, argv, "N:l:")) != -1) {
            switch (option) {
                case 'N':
                    threadCount = atoi(optarg);
                    if (threadCount < 1) {
                        printf("ERROR (-N): Must use at least 1 thread\n");
                        exit(1);
                    }
                    break;
                case 'l':
                    strcpy(logFileName, optarg);
                    break;
                case '?':
                    if (isprint(optopt)) {
                        fprintf(stderr, "Unknown option -%c.\n", optopt);
                    } else {
                        fprintf(stderr, "Unknown option character \\x%x'.\n",
                                optopt);
                    }
                    return EXIT_FAILURE;
            }
        }
        port = argv[optind];
    }

    // Check for valid port number
    if (atoi(port) < 1024) {
        printf("ERROR: Use a port number above 1024\n");
        exit(1);
    }

    // --STARTER CODE--
    /* Create sockaddr_in with server information */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t addrlen = sizeof(server_addr);

    /* Create server socket */
    int server_sockd = socket(AF_INET, SOCK_STREAM, 0);
    // Need to check if server_sockd < 0, meaning an error
    if (server_sockd < 0) perror("socket");

    /* Configure server socket */
    int enable = 1;
    /* This allows you to avoid: 'Bind: Address Already in Use' error */
    int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable,
                         sizeof(enable));

    /* Bind server address to socket that is open */
    ret = bind(server_sockd, (struct sockaddr *)&server_addr, addrlen);

    /* Listen for incoming connections */
    ret = listen(server_sockd, 5);
    // 5 should be enough, if not use SOMAXCONN
    if (ret < 0) return EXIT_FAILURE;

    /* Connecting with a client */
    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(client_addr);

    // Create structure for "global" shared variables
    struct varShareObject vars;
    // init objects in variable
    vars.queue = newList();                        // init "queue"
    pthread_mutex_init(&(vars.queueMutex), NULL);  // init mutex
    pthread_mutex_init(&(vars.varMutex), NULL);    // init mutex
    pthread_mutex_init(&(vars.condMutex), NULL);   // init mutex

    if (strcmp(logFileName, "\0") == 0) {  // init flag telling us to log
        strcpy(vars.logName, "\0");
        vars.useLogging = false;
    } else {
        strncpy(vars.logName, logFileName, sizeof(logFileName));
        vars.useLogging = true;
        // appease test08, create log file immediately
        vars.logFd = open(vars.logName, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);
    }

    pthread_cond_init(&(vars.cond), NULL);  // init condition variable
    vars.totalEntries = 0;                  // init healthcheck vars
    vars.totalErrors = 0;

    // create array of N worker threads
    pthread_t dispatcherArray[threadCount + 1];
    for (int i = 0; i < threadCount; i++) {
        pthread_create(&dispatcherArray[i], NULL, &workerThread, &vars);
    }
    // create thread for logging
    pthread_create(&dispatcherArray[threadCount], NULL, &logThread, &vars);
    vars.logQueue = newLogList();
    pthread_mutex_init(&(vars.logMutex), NULL);
    pthread_mutex_init(&(vars.logCondMutex), NULL);
    pthread_cond_init(&(vars.logCond), NULL);

    printf("[+] server is up\n");
    while (true) {
        /* 1. Accept Connection */
        int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
        if (client_sockd < 0) {
            printf("MAIN\nACCEPT ERROR\n ERRNO:%d\n", errno);
            printf(" server_sockd:%d\n", server_sockd);
            printf(" client_addr:%ld\n", (unsigned long)&client_addr);
            printf(" client_addrlen:%ld\n\n", (unsigned long)&client_addr);
            printf(" client_sockd:%d\n", client_sockd);
            exit(1);
        }

        // lock, push a client socket fd onto the queue, then unlock
        pthread_mutex_lock(&(vars.queueMutex));
        append(vars.queue, client_sockd);
        pthread_mutex_unlock(&(vars.queueMutex));
        printf("[+] server is waiting...\n");
        // wake up a thread waiting on cond

        pthread_cond_signal(&(vars.cond));
    }
    return EXIT_SUCCESS;
}
