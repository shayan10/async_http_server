#define  _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define exit(N) {fflush(stdout); fflush(stderr); _exit(N); }
#define BMAX 4096
#define RMAX 1024
#define EMAX 200

static int get_port(void);
static int Socket(int namesapce, int style, int protocol);
static void Bind(int sockfd, struct sockaddr* server, socklen_t length);
static void Listen(int sockfd, int qlen);
static int Accept(int sockfd, struct sockaddr* addr, socklen_t * length_ptr);
static int Recv(int clientfd, char* buffer, size_t size);
static int Send(int clientfd, char* buffer, size_t size);
static ssize_t Read(int filefd, char* buffer, size_t size);

static struct sockaddr_in server;

// STATS
static int num_success_requests = 0;
static int header_bytes = 0;
static int body_bytes = 0;
static int errors = 0;
static int error_bytes = 0;

const char response_template[] = "HTTP/1.1 %s\r\nContent-Length: %lu\r\n\r\n";
char error_404_msg[] = "HTTP/1.1 404 Not Found\r\n\r\n";
char error_400_msg[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
char error_413_msg[] = "HTTP/1.1 413 Request Entity Too Large\r\n\r\n";
char request[BMAX + 1];
char response[BMAX + 1];
char headers[BMAX + 1];

char saved_data[BMAX + 1] = "";
int saved_data_content_length = 0;
char file_data[RMAX + 1] = "";

char* endpoints[] = {"/ping:GET", "/echo:GET", "/read:GET", "/write:POST", "/stats:GET"};
const size_t NUM_ENDPOINTS = 5;

static ssize_t RSIZE = 0;

typedef struct file_transfer_entry {
    int clientfd;
    int filefd;
    int file_size;
} file_transfer_entry;

int client_file_fds[EMAX];

int find_endpoint(char* method, char* url) {
    for (size_t i = 0; i < NUM_ENDPOINTS; i++) {
        if (strstr(endpoints[i], url) != NULL) {
            // Check to see if method is correct
            if (strstr(endpoints[i], method) != NULL) {
                return 0;
            } else {
                return 1;
            }
        }
    }
    return -1;    
}

int get_headers_content_length() {
    char* ptr = strstr(headers, "Content-Length: ");
    if (ptr == NULL) {
        return -1;
    }
    ptr += strlen("Content-Length: ");
    char* end = strstr(ptr, "\r\n");
    if (end != NULL) {
        *end = '\0';
    }
    int result = atoi(ptr);
    if (end != NULL) {
        *end = '\r';
    }
    return result;
}

void send_success_response(int clientfd, char* message_body, unsigned long content_length) {
    memset(response, 0x00, sizeof(response));
    snprintf(response, BMAX, response_template,
        "200 OK", content_length);
    header_bytes += Send(clientfd, response, strlen(response));
    body_bytes += Send(clientfd, message_body, content_length);
    num_success_requests++;
}

void send_error_response(int clientfd, int code) {
    if (code == 400) {
        error_bytes += Send(clientfd, error_400_msg, strlen(error_400_msg));
    } else if (code == 404) {
        error_bytes += Send(clientfd, error_404_msg, strlen(error_404_msg));
    } else {
        error_bytes += Send(clientfd, error_413_msg, strlen(error_413_msg));
    }
    errors++;
}

int open_listenfd(int port) {
    int sockfd = Socket(AF_INET, SOCK_STREAM, 0);
    
    server.sin_family = AF_INET;
    server.sin_port = htons(get_port());
    inet_pton(AF_INET, "127.0.0.1", &(server.sin_addr));
    
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    Bind(sockfd, (struct sockaddr*)&server, sizeof(server));
    
    Listen(sockfd, 10);
    return sockfd;
}

int accept_client(int listenfd) {    
    static struct sockaddr_in client;
    static socklen_t client_size;
    memset (&client, 0x00, sizeof(client));
    int clientfd = Accept(listenfd, (struct sockaddr*)&client, &client_size);
    return clientfd;
}

int parse_request(char* request, char** method, char** url,
        ssize_t header_size, char** data) {
    char* head = request;
    // Get end of method substring
    char* method_delim = strstr(head, " ");
    // If not found, error
    if (method_delim == NULL) {
        return -1;
    }
    *method_delim = '\0';
    *method = head;
    if (strcmp(*method, "GET") != 0 && strcmp(*method, "POST") != 0) {
        return 1;
    }
    // Set head to next character after method space
    head = method_delim + 1;
    // Find URL substring end
    char* url_delim = strstr(head, " ");
    // If not found, error
    if (url_delim == NULL) {
        return -1;
    }
    *url_delim = '\0';
    *url = head;
    // Set head to next character
    head = url_delim + 1;
    char* http_delim = strstr(head, "\r\n");
    if (http_delim == NULL) {
        return -1;
    } else {
        *http_delim = '\0';
        // If not following HTTP protocol, return -1
        if (strcmp(head, "HTTP/1.1") != 0) {
            return -1;
        }
        *http_delim = '\r';
		head = http_delim;
	 }

    char* header_end = NULL;
    // If headers exist, populate headers
    if (strstr(head, ":") != NULL || strstr(head, "Content-Length: ") != NULL) {
        head = strstr(head, "\r\n") + 2;
        header_end = strstr(head, "\r\n\r\n");
        if (header_end == NULL) {
            return 1;
        }
        *header_end = '\0';
        strncpy(headers, head, BMAX);
        head = header_end + 4;

    } else {
        // Skip over the six characters between the protocl string and data
        head += 6;
    }

    if (strstr(head, "\r\n") != NULL) {
        head = strstr(head, "\r\n") + 2; // Skip over the "\r\n" sequence
    }

    *data = head;

    return 0;
}

void ping(int clientfd) {
    send_success_response(clientfd, "pong", 4);
}

void echo(int clientfd) {
    int length = strlen(headers);
    if (length > 1024) {
        send_error_response(clientfd, 413);
    } else {
        send_success_response(clientfd, headers, length);
    }
}

void readData(int clientfd) {
    if (saved_data_content_length == 0) {
        send_success_response(clientfd, "<empty>", 7);
        return;
    }
    send_success_response(clientfd, saved_data, saved_data_content_length);
}

void writeData(int clientfd, char* data) {
    int content_length = get_headers_content_length();
    if (content_length > 1024) {
        send_error_response(clientfd, 413);
    } else {
        memset(saved_data, 0x00, sizeof(saved_data));
        memcpy(saved_data, (void*)data, content_length);
        saved_data_content_length = content_length;
        send_success_response(clientfd, saved_data, content_length);
    }
}

int isValidFile(char* filename, struct stat* file_info) {
    filename++;
    int filefd;
    if ((filefd = open(filename, O_RDONLY, 0644)) < 0) {
        return -1;
    }
    fstat(filefd, file_info);
    if (!S_ISREG(file_info->st_mode)) {
        close(filefd);
        return -1;
    }
    return filefd;
}

int send_file_chunk(int clientfd, int filefd) {
    int return_value = 0;
    memset(file_data, 0x00, sizeof(file_data));
    return_value = Read(filefd, file_data, RMAX);
    if (return_value > 0) {
        body_bytes += Send(clientfd, file_data, return_value);
    }
    return return_value;    
}

void stats(int clientfd) {
    char statsBuffer[100];
    snprintf(statsBuffer, 100, "Requests: %d\nHeader bytes: %d\nBody bytes: %d\nErrors: %d\nError bytes: %d",
        num_success_requests, header_bytes, body_bytes, errors, error_bytes);
    send_success_response(clientfd, statsBuffer, strlen(statsBuffer));
}

void handle_request(int clientfd, char* url, char* data) {
    
    if (strcmp(url, "/ping") == 0) {
        ping(clientfd);
    } else if (strcmp(url, "/echo") == 0) {
        echo(clientfd);
    } else if (strcmp(url, "/read") == 0) {
        readData(clientfd);
    } else if (strcmp(url, "/write") == 0) {
        writeData(clientfd, data);
    } else if (strcmp(url, "/stats") == 0) {
        stats(clientfd);
    }
}

int main(int argc, char * argv[])
{
    int port = get_port();

    printf("Using port %d\n", port);
    printf("PID: %d\n", getpid());

    // Make server available on port
    int listenfd = open_listenfd(get_port());

    // Create Epoll instance
    int epfd = epoll_create1(0);

    static struct epoll_event Event;
    
    memset(&Event, 0x00, sizeof(Event));
    Event.events = EPOLLIN;
    Event.data.fd = listenfd;

    // Add listening socket to interest list
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &Event);

    static struct epoll_event Events[EMAX];

    while (1) {
        int nfds = epoll_wait(epfd, Events, EMAX, -1);
        for (int i = 0; i != nfds; i++) {
            int fd = Events[i].data.fd;

            if (fd == listenfd) {
                int clienfd = accept_client(listenfd);
                memset(&Event, 0x00, sizeof(Event));
                Event.events = EPOLLIN;
                Event.data.fd = clienfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clienfd, &Event);
            } else {
                if (Events[i].events == EPOLLIN) {
                    int RSIZE = Recv(fd, request, BMAX);
                    
                    if (RSIZE != 0) {
                        memset (&headers, 0x00, sizeof(headers));
                        // Null terminate request data
                        request[RSIZE] = '\0';
                        char *method, *url, *data;
                        
                        if (parse_request(request, &method, &url, sizeof(headers), &data) != 0) {
                            send_error_response(fd, 400);
                        } else {
                            int found = find_endpoint(method, url);

                            if (found == 0) {
                                handle_request(fd, url, data);
                            } else if (found == 1 && strcmp(method, "GET") != 0) {
                                send_error_response(fd, 400); 
                            } else if (strcmp(method, "GET") == 0) {
                                int filefd;
                                struct stat* file_stats = malloc(sizeof(struct stat));
                                // If file does not exist, send 404 message
                                if ((filefd = isValidFile(url, file_stats)) == -1) {
                                    send_error_response(fd, 404);
                                    free(file_stats);
                                } else {
                                    // Remove read event
                                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &Event);
                                    // Record event data
                                    client_file_fds[fd] = filefd;
                                    memset(&Event, 0x00, sizeof(Event));
                                    Event.events = EPOLLOUT;
                                    // Event.data.ptr = &(client_file_fds[fd]);
                                    Event.data.fd = fd;
                                    // Send header
                                    memset(response, 0x00, sizeof(response));
                                    snprintf(response, BMAX, response_template,
                                        "200 OK", file_stats->st_size);
                                    Send(fd, response, strlen(response));
                                    header_bytes += strlen(response);
                                    num_success_requests++;
                                    // Add write event
                                    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &Event);
                                    continue;
                                }
                            } else {
                                send_error_response(fd, 400);
                            }
                        }
                    }
                    
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &Event);
                    close(fd);
                }

                if (Events[i].events == EPOLLOUT) {
                    // If all chunks sent
                    int filefd = client_file_fds[fd];
                    int bytesRead = send_file_chunk(fd, filefd);
                    struct stat file_stat;
                    fstat(filefd, &file_stat);
                    if (bytesRead <= 0 || bytesRead == file_stat.st_size) {
                        // Close client connection
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &Event);
                        close(fd);
                        // Close file
                        close(filefd);
                    }
                }
            }
        }
    }
    return 0;
}


static int get_port(void)
{
    int fd = open("port.txt", O_RDONLY);
    if (fd < 0) {
        perror("Could not open port.txt");
        exit(1);
    }

    char buffer[32];
    int r = read(fd, buffer, sizeof(buffer));
    if (r < 0) {
        perror("Could not read port.txt");
        exit(1);
    }

    return atoi(buffer);
}


static int Socket(int namesapce, int style, int protocol) {
    int sockfd = socket(namesapce, style, protocol);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }
    return sockfd;
}

static void Bind(int sockfd, struct sockaddr* server, socklen_t length) {
    int code = bind(sockfd, server, length);
    if (code < 0) {
        perror("bind");
        exit(1);
    }
}

static void Listen(int sockfd, int qlen) {
    int listenfd = listen(sockfd, qlen);
    if (listenfd < 0) {
        perror("listen");
        exit(1);
    }
}

static int Accept(int sockfd, struct sockaddr* addr, socklen_t * length_ptr) {
    int connfd = accept(sockfd, addr, length_ptr);
    if (connfd < 0) {
        perror("accept");
        exit(1);
    }
    return connfd;
}

static int Recv(int clientfd, char* buffer, size_t size) {
    int rsize = recv(clientfd, buffer, size, 0);
    if (rsize < 0) {
        perror("recv");
        exit(1);
    }
    return rsize;
}

static int Send(int clientfd, char* buffer, size_t size) {
    ssize_t sentSize = send(clientfd, buffer, size, 0);
    if (sentSize < 0) {
        perror("send");
        exit(1);
    }
    return sentSize;
}

static ssize_t Read(int filefd, char* buffer, size_t size) {
    ssize_t read_size = read(filefd, buffer, size);
    if (read_size < 0) {
        perror("read");
        exit(1);
    }
    buffer[read_size] = '\0';
    return read_size;
}