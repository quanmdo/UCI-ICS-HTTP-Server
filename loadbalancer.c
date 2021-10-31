#include <arpa/inet.h>
#include <ctype.h>  //isprint
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/ip.h>  //#include <arpa/inet.h>
#include <pthread.h>     // mutex
#include <stdbool.h>     // bool
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/syscall.h>  //geting thread ids for debugging
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "List.h"
#define BUFFER_SIZE 4096
#define SECONDS 2  // 2

/*-----------------------SERVER INFO STRUCTS----------------------*/
typedef struct server_info_t {
    uint16_t port;
    bool alive;
    uint16_t totalRequests;
    uint16_t errorRequests;
    pthread_mutex_t mut;  // finer grain lock
} ServerInfo;

typedef struct servers_t {
    uint16_t numServers;
    ServerInfo *servers;       // pointer to array of server info structs
    pthread_mutex_t varMut;    // coarse grained mut, worker threads
    pthread_mutex_t queueMut;  // ensure the queue operations are atomic
    pthread_mutex_t condMut;   // for worker threads to pop off requests
    pthread_cond_t cond;       // for queue

    uint16_t reqCount;  // running count of R requests to trigger hc
    List queue;         // queue of requests

    int16_t healthiestIdx;
    uint16_t listenFd;  // client's port to listen on
} Servers;

Servers servers;  // global object of type Servers named servers
/*------------------------HEALTH THREAD STRUCTS-----------------------*/
typedef struct healthThreadData {
    pthread_mutex_t hMut;
    pthread_cond_t hCond;

    bool resetFlag;
} healthThreadData;

/*------------------------HELPER-----------------------*/

void send500(int client_sockd) {
    dprintf(client_sockd,
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
    return;
}

/*---------------------------STARTER FUNCS--------------------------*/
/*
 * client_connect takes a port number and establishes a connection as a
 * client. connectport: port number of server to connect to returns: valid
 * socket if successful, -1 otherwise
 */
int client_connect(uint16_t connectport) {
    int connfd;
    struct sockaddr_in servaddr;
    // creates client socket where we pass the server address (localhost)
    // and the port
    connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) return -1;
    memset(&servaddr, 0, sizeof servaddr);

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(connectport);

    /* For this assignment the IP address can be fixed */
    inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));

    // connect will return -1 if connection fails (server most likely down)
    // when all servers on the same machine, failing connect 100% indicate a
    // down server can't hc a down server, failure to connect will equal a
    // down server
    if (connect(connfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        return -1;
    return connfd;
}

/*
 * server_listen takes a port number and creates a socket to listen on
 * that port.
 * port: the port number to receive connections
 * returns: valid socket if successful, -1 otherwise
 */
int server_listen(int port) {
    int listenfd;
    int enable = 1;
    struct sockaddr_in servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    servers.listenFd = listenfd;
    if (listenfd < 0) return -1;

    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable,
                   sizeof(enable)) < 0)
        return -1;
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof servaddr) < 0)
        return -1;
    if (listen(listenfd, 500) < 0) return -1;
    return listenfd;
}

/*
 * bridge_connections send up to 100 bytes from fromfd to tofd
 * fromfd, tofd: valid sockets
 * returns: number of bytes sent, 0 if connection closed, -1 on error
 */
int bridge_connections(int fromfd, int tofd) {
    char recvline[BUFFER_SIZE];
    int n = recv(fromfd, recvline, BUFFER_SIZE, 0);
    if (n < 0) {
        printf("connection error receiving\n");
        return -1;
    } else if (n == 0) {
        printf("receiving connection ended\n");
        return 0;
    }
    recvline[n] = '\0';
    // printf("%s", recvline);  //
    // sleep(1);
    n = send(tofd, recvline, n, 0);
    if (n < 0) {
        printf("connection error sending\n");
        return -1;
    } else if (n == 0) {
        printf("sending connection ended\n");
        return 0;
    }
    return n;
}

/*
 * bridge_loop()
 * forwards all messages between both sockets until the connection
 * is interrupted. It also prints a message if both channels are idle.
 * sockfd1, sockfd2: valid sockets
 */
void bridge_loop(int sockfd1, int sockfd2) {
    fd_set set;
    struct timeval timeout;

    int fromfd, tofd;
    while (1) {
        // set for select usage must be initialized before each select call
        // set manages which file descriptors are being watched
        FD_ZERO(&set);
        FD_SET(sockfd1, &set);
        FD_SET(sockfd2, &set);

        // same for timeout
        // max time waiting, 5 seconds, 0 microseconds
        timeout.tv_sec = SECONDS;
        timeout.tv_usec = 0;

        // select return the number of file descriptors ready for reading in
        switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
            case -1:
                printf("error during select, exiting\n");
                send500(sockfd1);
                return;
            case 0:
                printf("BOTH IDLE, select timed out\n");
                send500(sockfd1);
                return;
            default:
                if (FD_ISSET(sockfd1, &set)) {
                    fromfd = sockfd1;
                    tofd = sockfd2;
                } else if (FD_ISSET(sockfd2, &set)) {
                    fromfd = sockfd2;
                    tofd = sockfd1;
                } else {
                    printf("this should be unreachable\n");
                    return;
                }
        }
        int ret = bridge_connections(fromfd, tofd);
        if (ret <= 0) {
            if (ret < 0) send500(sockfd1);
            return;
        }
        // if (bridge_connections(fromfd, tofd) <= 0) return;
    }
}

/*---------------------------MY FUNCS--------------------------*/
int do_periodic_healthcheck(int port, ServerInfo *svrVars) {
    // poll(struct pollfd fds[], nfds_t nfds, int timeout);
    // fds: array of file descriptors
    // nfds: number of pollfd structires in fds

    int16_t serverFd;
    int ret;
    char buffer[BUFFER_SIZE];
    char tempBuffer[BUFFER_SIZE];

    uint16_t code;
    char codeName[10];
    uint16_t length;
    uint16_t errors;
    uint16_t entries;

    // attempt to connect to specified server port
    if ((serverFd = client_connect(port)) < 0) {
        close(serverFd);
        return -1;
    }

    // poll vars
    int nfds = 1;
    struct pollfd fds[1];

    // Initialize the pollfd structure
    memset(fds, 0, sizeof(fds));
    // reset buffer
    memset(&buffer, 0, BUFFER_SIZE);
    memset(&tempBuffer, 0, BUFFER_SIZE);

    // Set up the initial listening socket
    fds[0].fd = serverFd;
    fds[0].events = POLLIN;
    // Initialize the timeout
    int timeout = (SECONDS * 1000);  // 2 sec

    // call poll and wait 2 sec for it to complete
    // send a hc
    ret = dprintf(serverFd, "GET /healthcheck HTTP/1.1\r\n\r\n");
    // rtodo: return -1 on failure^
    ret = poll(fds, nfds, timeout);

    if (ret == 0) {
        printf("TIMEOUT\n");
        return -1;
    } else {
        if (ret <= 0) {
            printf("Attempt to HC port %d unsuccessful\n", port);
            close(serverFd);
            return -1;
        }

        // read the returning bytes into buffer
        ssize_t bytesRead;
        while ((bytesRead = read(serverFd, tempBuffer, sizeof(tempBuffer))) >
               0) {
            int16_t bytesWritten =
                sprintf(buffer + strlen(buffer), "%s", tempBuffer);

            if (bytesWritten < 0)
                printf("Error: reading healthcheck; written != read\n");
            memset(&tempBuffer, 0, BUFFER_SIZE);
        }

        // if server didn't respond, it's dead
        // may need to remove if using select
        if (bytesRead < 0) {
            printf("Server at port %d did not respond\n", port);  //
            close(serverFd);
            return -1;
        }

        int16_t nscan =
            sscanf(buffer, "HTTP/1.1 %hu %s\r\nContent-Length: %hu\r\n%hu\n%hu",
                   &code, codeName, &length, &errors, &entries);
        if (code != 200 || nscan < 5) {
            close(serverFd);
            return -1;
        }

        // healthy server with good response
        // update health status
        svrVars->alive = true;
        svrVars->errorRequests = errors;
        svrVars->totalRequests = entries;

        printf("Port [%d] health = %d / %d (%d)\n", port,
               svrVars->errorRequests, svrVars->totalRequests, svrVars->alive);
        close(serverFd);
        return 0;
    }
    return 0;
}

// determine_best_server()
void determine_best_server() {
    // iterate through array of servers and find the healthiest one
    uint16_t currentEntries;
    uint16_t currentErrors;
    bool currentLive;
    uint16_t nextEntries;
    uint16_t nextErrors;
    bool nextLive;

    // init healthiest as 1st index's port
    // uint16_t currentIdx = 0;
    // currentEntries = servers.servers[0].totalRequests;
    // currentErrors = servers.servers[0].errorRequests;
    // currentLive = servers.servers[0].alive;
    // int serverCount = servers.numServers;

    uint16_t currentIdx = -1;
    currentEntries = 0;
    currentErrors = 0;
    currentLive = false;

    int serverCount = servers.numServers;
    // iterate through the aray of servers and compare
    for (int i = 0; i < serverCount; i++) {
        nextLive = servers.servers[i].alive;
        nextEntries = servers.servers[i].totalRequests;
        nextErrors = servers.servers[i].errorRequests;

        // only compare request counts if the next server is alive
        if (nextLive) {
            // current server is dead or
            // server we are comparing to has less total requests
            if (!currentLive || (nextEntries < currentEntries)) {
                currentIdx = i;
                currentLive = nextLive;
                currentEntries = nextEntries;
                currentErrors = nextErrors;
            }
            // server we are comparing to has equal requests
            // tie break, one with less erors is healthier
            else if (nextEntries == currentEntries) {
                if (nextErrors < currentErrors) {
                    currentIdx = i;
                    currentEntries = nextEntries;
                    currentErrors = nextErrors;
                    currentLive = nextLive;
                }
            }
        }
    }

    // coarse mut to update servers struct
    // update healthiest
    pthread_mutex_lock(&servers.varMut);
    servers.healthiestIdx = currentIdx;  //-1
    pthread_mutex_unlock(&servers.varMut);
}

// precond:
// (1) we called getopt() to move the flag opts to the beginning of arg list
// (2) we extracted the port the loadbalancer will listen on
// (3) start_of_ports points to arg right after the load balancer port
int initServers(int argc, char **argv, int start_of_ports) {
    servers.numServers = argc - start_of_ports;
    // printf("numServers = %d\n", servers.numServers);  //

    if (servers.numServers <= 0) {
        printf("ERROR: please specify backend httpserver ports\n");
        return -1;
    }
    servers.servers = malloc(servers.numServers * sizeof(ServerInfo));

    // store port nums in server array
    for (int i = start_of_ports; i < argc; ++i) {
        int sidx = i - start_of_ports;
        // argc[i] = httpserver port
        servers.servers[sidx].port = atoi(argv[i]);
        // printf("server [%d] port = %d\n", sidx, servers.servers[sidx].port);

        if (servers.servers[sidx].port < 1024) {
            printf("ERROR: Server port number must be above 1024\n");
            exit(EXIT_FAILURE);
        }
        servers.servers[sidx].alive = false;
    }
    return 0;
}

/*---------------------------HEALTHCHECK THREAD---------------------------*/
void *healthCheckThread(void *vars) {
    // pid_t x = syscall(__NR_gettid);
    // if (DEBUG) printf("\033[0;32m  Health Thread Made %d\n\033[0m", x);

    // run healthchecks periodically
    ServerInfo *svrVars = (ServerInfo *)vars;

    int myPort = svrVars->port;
    // if (DEBUG)
    //     printf("\033[0;32m   Health Thread %d has port: %d\n\033[0m", x,
    //            myPort);

    // lock single server thread while we update info
    pthread_mutex_lock(&(svrVars->mut));
    int ret = do_periodic_healthcheck(myPort, svrVars);

    // if healtchecking process errs out, the server is dead
    if (ret == -1) {
        svrVars->alive = false;
        // increment local health vars
        svrVars->totalRequests += 1;
        svrVars->errorRequests += 1;
        // BIG help debugging
        // printf("Port [%d] health = %d / %d (%d)\n", myPort,
        //        svrVars->errorRequests, svrVars->totalRequests,
        //        svrVars->alive);
    }
    pthread_mutex_unlock(&(svrVars->mut));

    return NULL;
}

void *healthDispatcher(void *vars) {
    // pid_t x = syscall(__NR_gettid);
    // if (DEBUG) printf("\033[0;32m Health Dispatcher Made %d\n\033[0m", x);

    healthThreadData *healthVars = (healthThreadData *)vars;

    uint8_t serverCount = servers.numServers;

    // conduct preliminary hc before waiting
    pthread_t hcThreadrArr[serverCount];
    for (int i = 0; i < serverCount; i++) {
        pthread_create(&hcThreadrArr[i], NULL, &healthCheckThread,
                       &(servers.servers[i]));
    }

    // join the threads, decide on a healthiest server
    for (int i = 0; i < serverCount; i++) {
        pthread_join(hcThreadrArr[i], NULL);
    }

    // all healthchecking is done at this point
    determine_best_server();

    struct timespec ts;
    struct timeval now;
    while (1) {
        // lock mutex for timed wait
        pthread_mutex_lock(&(healthVars->hMut));

        // reset time
        memset(&ts, 0, sizeof(ts));
        // set wait to 2 sec
        gettimeofday(&now, NULL);
        ts.tv_sec = now.tv_sec + SECONDS;

        // wait on time
        pthread_cond_timedwait(&(healthVars->hCond), &(healthVars->hMut), &ts);
        // wait on flag (R requests hit)

        if (healthVars->resetFlag) {
            pthread_mutex_unlock(&(healthVars->hMut));
            // printf("*R FLAG TRIGGERED*\n");
            memset(&ts, 0, sizeof(ts));
        }

        pthread_mutex_unlock(&(healthVars->hMut));

        // start up healthcheck threads, 1 for each server
        pthread_t hcThreadrArray[serverCount];
        for (int i = 0; i < serverCount; i++) {
            pthread_create(&hcThreadrArray[i], NULL, &healthCheckThread,
                           &(servers.servers[i]));
        }

        // join the threads, decide on a healthiest server
        for (int i = 0; i < serverCount; i++) {
            pthread_join(hcThreadrArray[i], NULL);
        }

        // all healthchecking is done at this point
        determine_best_server();

        healthVars->resetFlag = false;

        printf("HEALTHIEST = %hu \n",
               servers.servers[servers.healthiestIdx].port);
        if (!(servers.servers[servers.healthiestIdx].alive)) {
            printf("HEALTHIEST IS DEAD\n");
        }
    }
}
/*-------------------------------WORKER THREAD-------------------------------*/
/*
 * takes a client socket descriptor and forwarrds it to the healthiest
 * server sends 500 to the client if all are down increments local health
 * vars
 */
void *workerThread() {  // void *svrs
    // pid_t x = syscall(__NR_gettid);
    // printf("\033[0;33m Worker Thread Made %d\n\033[0m", x);
    // Servers *vars = (Servers *)svrs;

    while (1) {
        // Lock/sedate threads until a sockfd is in the queue
        pthread_mutex_lock(&servers.condMut);

        while (length(servers.queue) == 0)
            pthread_cond_wait(&(servers.cond), &(servers.condMut));

        // pop off oldest request sockfd off queue
        int client_sockd = front(servers.queue);
        deleteFront(servers.queue);

        // printf("\033[0;33mQueue popped: Thread %d has FD: %d\n\033[0m", x,
        //        client_sockd);
        pthread_mutex_unlock(&servers.condMut);
        // we now have a client socket fd

        // get index of healthiest server
        // todo: probably should lock this part too
        int16_t serverIdx = servers.healthiestIdx;
        // unlock

        // if healthiest server is dead, they're all dead
        // send a 500
        if ((serverIdx == -1) || (!servers.servers[serverIdx].alive)) {
            send500(client_sockd);
            continue;
        }

        pthread_mutex_lock(&servers.varMut);
        uint16_t serverPort = servers.servers[serverIdx].port;
        pthread_mutex_unlock(&servers.varMut);

        // attempt connecting to it
        int serverFd = client_connect(serverPort);
        if (serverFd < 0) {
            // something happened to the server in between hc
            servers.servers[serverIdx].alive = false;
            send500(client_sockd);
            continue;
        }

        bridge_loop(client_sockd, serverFd);

        close(client_sockd);
        close(serverFd);
    }
}
/*----------------------------------MAIN---------------------------------------*/
int main(int argc, char **argv) {
    int listenfd;

    uint16_t listenport;  // connectport
    int N = 4;
    int R = 5;

    // getopt jazz
    if (argc < 3) {
        printf("Missing arguments: usage %s port_to_listen port_to_connect",
               argv[0]);
        return 1;
    } else {
        // get values of N and R
        /*
        -N defining the number N of parallel connections
        -R defining how often (# requests) to retrieve the healthcheck from
        the servers
        */
        int option;
        while ((option = getopt(argc, argv, "N:R:")) != -1) {
            switch (option) {
                case 'N':
                    N = atoi(optarg);
                    if (N < 1) {
                        printf("ERROR (-N): Invalid N value (<1)\n");
                        exit(1);
                    }
                    break;
                case 'R':
                    R = atoi(optarg);
                    if (R < 1) {
                        printf("ERROR (-R): Invalid R value (<1)\n");
                        exit(1);
                    }
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
        // get port to listen on
        listenport = atoi(argv[optind]);  // 1234
        if (listenport < 1024) {
            printf("ERROR: Use a client port number above 1024\n");
            exit(EXIT_FAILURE);
        }
        // todo: At least one server port number is required.
    }

    // get ports to connect to (servers)
    int ret = initServers(argc, argv, optind + 1);

    // start healthcheck dispatcher
    // init hc struct
    struct healthThreadData healthVars;
    pthread_mutex_init(&(healthVars.hMut), NULL);
    pthread_cond_init(&(healthVars.hCond), NULL);
    healthVars.resetFlag = false;  // flag will signal # R requests
    int serverCount = servers.numServers;

    pthread_t healthDispatch;
    pthread_create(&healthDispatch, NULL, &healthDispatcher, &healthVars);

    // Listen for incoming connections
    if ((listenfd = server_listen(listenport)) < 0) {
        err(1, "failed listening");
    }

    // Connecting with a client
    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(client_addr);

    // init Servers Object
    pthread_mutex_init(&(servers.varMut), NULL);
    pthread_mutex_init(&(servers.queueMut), NULL);
    pthread_mutex_init(&(servers.condMut), NULL);
    pthread_cond_init(&(servers.cond), NULL);
    servers.reqCount = 0;
    servers.queue = newList();
    servers.healthiestIdx = -1;
    // servers.numAlive = 0;

    // init threads
    // create array of N worker threads
    pthread_t dispatcherArray[N];
    for (int i = 0; i < N; i++) {
        pthread_create(&dispatcherArray[i], NULL, &workerThread, NULL);
    }
    // sem wait

    printf("[+] loadbalancer is up\n");

    while (1) {
        // Accept Connection
        printf("Waiting on accept..\n");  //
        int client_sockd = accept(listenfd, &client_addr, &client_addrlen);

        if (client_sockd < 0) {
            printf("MAIN\nACCEPT ERROR\n ERRNO:%d\n", errno);
            exit(1);
        }

        servers.reqCount += 1;

        // lock, push a client socket fd onto the queue, then unlock
        pthread_mutex_lock(&(servers.queueMut));
        append(servers.queue, client_sockd);
        pthread_mutex_unlock(&(servers.queueMut));
        printf("[+] server is waiting...\n");
        // wake up a thread waiting on cond
        pthread_cond_signal(&(servers.cond));

        if (servers.reqCount == R) {
            // printf("R REQUESTS HIT, FLAGGING\n");
            healthVars.resetFlag = true;
            pthread_cond_broadcast(&(healthVars.hCond));
            servers.reqCount = 0;
        }
    }
    return ret;
}
