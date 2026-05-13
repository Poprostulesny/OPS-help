#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef TEMP_FAILURE_RETRY
/**
 * Retries a system call-like expression when it is interrupted by a signal.
 *
 * @param expression Expression to execute. It should return -1 and set errno
 *                   to EINTR when interrupted.
 * @return The expression result. If the expression keeps failing for a reason
 *         other than EINTR, that failing value is returned unchanged.
 * @errors Does not terminate the program by itself. The wrapped expression may
 *         still fail and set errno.
 */
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

/**
 * Prints a system error message with file and line information, then exits.
 *
 * @param source Short description passed to perror(), usually the failed call.
 * @return This macro never returns.
 * @errors Always terminates the process with EXIT_FAILURE.
 */
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

/**
 * Installs a signal handler for the selected signal.
 *
 * @param f Handler function to call when sigNo is delivered. Use SIG_IGN to
 *          ignore the signal or SIG_DFL to restore default handling.
 * @param sigNo Signal number to handle, for example SIGINT or SIGPIPE.
 * @return 0 on success, -1 if sigaction() fails.
 * @errors Does not call ERR and does not exit. On failure, returns -1 and
 *         leaves errno set by sigaction().
 */
int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

/**
 * Creates an unconnected UNIX domain stream socket and fills its address.
 *
 * @param name Filesystem path of the UNIX socket.
 * @param addr Output address structure filled with AF_UNIX and name.
 * @return File descriptor of the newly created socket.
 * @errors Calls ERR("socket") and exits if socket() fails.
 */
int make_local_socket(char *name, struct sockaddr_un *addr)
{
    int socketfd;
    if ((socketfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
        ERR("socket");
    memset(addr, 0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, name, sizeof(addr->sun_path) - 1);
    return socketfd;
}

/**
 * Connects to a listening UNIX domain stream socket.
 *
 * @param name Filesystem path of the UNIX socket to connect to.
 * @return Connected socket file descriptor.
 * @errors Calls ERR("socket") and exits if socket creation fails. Calls
 *         ERR("connect") and exits if connect() fails.
 */
int connect_local_socket(char *name)
{
    struct sockaddr_un addr;
    int socketfd;
    socketfd = make_local_socket(name, &addr);
    if (connect(socketfd, (struct sockaddr *)&addr, SUN_LEN(&addr)) < 0)
    {
        ERR("connect");
    }
    return socketfd;
}

/**
 * Creates, binds, and starts listening on a UNIX domain stream socket.
 *
 * @param name Filesystem path where the socket should be bound. Any existing
 *             file at this path is unlinked first.
 * @param backlog_size Maximum pending connection backlog passed to listen().
 * @return Listening socket file descriptor.
 * @errors Calls ERR("unlink") and exits if an existing path cannot be removed,
 *         except when it does not exist. Calls ERR("socket"), ERR("bind"), or
 *         ERR("listen") and exits if the matching system call fails.
 */
int bind_local_socket(char *name, int backlog_size)
{
    struct sockaddr_un addr;
    int socketfd;
    if (unlink(name) < 0 && errno != ENOENT)
        ERR("unlink");
    socketfd = make_local_socket(name, &addr);
    if (bind(socketfd, (struct sockaddr *)&addr, SUN_LEN(&addr)) < 0)
        ERR("bind");
    if (listen(socketfd, backlog_size) < 0)
        ERR("listen");

    return socketfd;
}

/**
 * Creates an IPv4 TCP stream socket.
 *
 * @return File descriptor of the newly created TCP socket.
 * @errors Calls ERR("socket") and exits if socket() fails.
 */
int make_tcp_socket(void)
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

/**
 * Resolves a host and service/port into an IPv4 socket address.
 *
 * @param address Hostname or numeric IPv4 address to resolve.
 * @param port Service name or decimal port string to resolve.
 * @return IPv4 socket address suitable for connect().
 * @errors Prints a getaddrinfo() error message and exits with EXIT_FAILURE if
 *         name or service resolution fails.
 */
struct sockaddr_in make_address(char *address, char *port)
{
    int ret;
    struct sockaddr_in addr;
    struct addrinfo *result;
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    if ((ret = getaddrinfo(address, port, &hints, &result)))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }
    addr = *(struct sockaddr_in *)(result->ai_addr);
    freeaddrinfo(result);
    return addr;
}

/**
 * Creates a TCP socket and connects it to a remote IPv4 endpoint.
 *
 * @param name Hostname or numeric IPv4 address of the remote endpoint.
 * @param port Service name or decimal port string of the remote endpoint.
 * @return Connected TCP socket file descriptor.
 * @errors Calls ERR("socket") and exits if socket creation fails. Exits from
 *         make_address() if address resolution fails. Calls ERR("connect") and
 *         exits if connect() fails.
 */
int connect_tcp_socket(char *name, char *port)
{
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_tcp_socket();
    addr = make_address(name, port);
    if (connect(socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        ERR("connect");
    }
    return socketfd;
}

/**
 * Creates a TCP socket and attempts to connect to a remote IPv4 endpoint.
 *
 * Unlike connect_tcp_socket(), a refused connection is reported as a normal
 * return value instead of terminating the program.
 *
 * @param name Hostname or numeric IPv4 address of the remote endpoint.
 * @param port Service name or decimal port string of the remote endpoint.
 * @return Connected TCP socket file descriptor on success, or -1 if connect()
 *         fails with ECONNREFUSED. When -1 is returned, the temporary socket is
 *         closed before returning.
 * @errors Calls ERR("socket") and exits if socket creation fails. Exits from
 *         make_address() if address resolution fails. Calls ERR("connect") and
 *         exits for connect() failures other than ECONNREFUSED.
 */
int maybe_connect_tcp_socket(char *name, char *port)
{
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_tcp_socket();
    addr = make_address(name, port);
    if (connect(socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        if (errno != ECONNREFUSED)
            ERR("connect");
        close(socketfd);
        return -1;
    }
    return socketfd;
}

/**
 * Creates, binds, and starts listening on an IPv4 TCP socket.
 *
 * The socket is bound to INADDR_ANY and has SO_REUSEADDR enabled.
 *
 * @param port TCP port in host byte order.
 * @param backlog_size Maximum pending connection backlog passed to listen().
 * @return Listening TCP socket file descriptor.
 * @errors Calls ERR("socket"), ERR("setsockopt"), ERR("bind"), or
 *         ERR("listen") and exits if the matching system call fails.
 */
int bind_tcp_socket(uint16_t port, int backlog_size)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_tcp_socket();
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind");
    if (listen(socketfd, backlog_size) < 0)
        ERR("listen");
    return socketfd;
}

/**
 * Accepts one pending client connection from a listening socket.
 *
 * Interrupted accept() calls are retried automatically.
 *
 * @param sfd Listening socket file descriptor.
 * @return New client socket file descriptor on success, or -1 if accept()
 *         fails with EAGAIN or EWOULDBLOCK on a nonblocking listening socket.
 * @errors Calls ERR("accept") and exits for accept() failures other than
 *         EAGAIN and EWOULDBLOCK.
 */
int add_new_client(int sfd)
{
    int nfd;
    if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, NULL, NULL))) < 0)
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            return -1;
        ERR("accept");
    }
    return nfd;
}

/**
 * Reads up to count bytes from a file descriptor into a buffer.
 *
 * The function keeps reading until count bytes have been read, EOF is reached,
 * or read() fails. Interrupted read() calls are retried automatically.
 *
 * @param fd File descriptor to read from.
 * @param buf Destination buffer with space for at least count bytes.
 * @param count Number of bytes requested.
 * @return Number of bytes actually read. This can be less than count if EOF is
 *         reached. Returns -1 if read() fails.
 * @errors Does not call ERR and does not exit. On read() failure, returns -1
 *         and leaves errno set by read().
 */
ssize_t bulk_read(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (0 == c)
            return len;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

/**
 * Writes exactly count bytes from a buffer to a file descriptor.
 *
 * The function keeps writing until all bytes have been written or write()
 * fails. Interrupted write() calls are retried automatically.
 *
 * @param fd File descriptor to write to.
 * @param buf Source buffer containing at least count bytes.
 * @param count Number of bytes to write.
 * @return count on success, or -1 if write() fails before all bytes are sent.
 * @errors Does not call ERR and does not exit. On write() failure, returns -1
 *         and leaves errno set by write(). For a nonblocking descriptor,
 *         errno may be EAGAIN or EWOULDBLOCK when writing would block; this is
 *         usually not a fatal error and means the remaining data should be
 *         written later, after epoll reports the descriptor writable. Because
 *         this function may already have written some bytes before returning
 *         -1, callers that must preserve message boundaries should track
 *         partial writes themselves. errno may be EPIPE when the peer has
 *         closed the connection; this program ignores SIGPIPE, so write()
 *         returns -1 instead of terminating the process. Other possible errno
 *         values come directly from write(), for example EBADF for an invalid
 *         descriptor or ECONNRESET when the connection was reset.
 */
ssize_t bulk_write(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

/**
 * Creates an epoll instance and registers the initial listening socket.
 *
 * The socket is watched for EPOLLIN events, which means epoll will report it
 * when a new incoming connection can be accepted.
 *
 * @param socket Listening socket file descriptor to add to the epoll instance.
 * @return File descriptor of the newly created epoll instance.
 * @errors Calls ERR("epoll_create1") and exits if epoll_create1() fails.
 *         Calls ERR("epoll_ctl: tcp_socket") and exits if registering socket
 *         in the epoll instance fails.
 */
int create_epoll_init(int socket)
{
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        ERR("epoll_create1");
    }
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket, &event) == -1)
    {
        ERR("epoll_ctl: tcp_socket");
    }
    return epoll_fd;
}

/**
 * Sets a file descriptor to nonblocking mode.
 *
 * After this call, operations such as accept(), recv(), and write() may fail
 * with EAGAIN or EWOULDBLOCK instead of waiting.
 *
 * @param fd File descriptor whose O_NONBLOCK flag should be enabled.
 * @return Nothing.
 * @errors Calls ERR("fcntl") and exits if setting the descriptor flags fails.
 */
void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        ERR("fcntl");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        ERR("fcntl");
}

/**
 * Adds a file descriptor to an existing epoll instance.
 *
 * The descriptor is watched for EPOLLIN events, which means epoll will report
 * it when there is data to read or, for a listening socket, a connection to
 * accept.
 *
 * @param epoll_fd File descriptor of the epoll instance.
 * @param fd File descriptor to watch.
 * @return Nothing.
 * @errors Calls ERR("epoll_ctl: add fd") and exits if epoll_ctl() fails.
 */
void add_to_epoll(int epoll_fd, int fd)
{
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1)
    {
        ERR("epoll_ctl: add fd");
    }
    return;
}

/**
 * Removes a file descriptor from an epoll instance.
 *
 * Use this before closing a client descriptor while the epoll instance keeps
 * running.
 *
 * @param epoll_fd File descriptor of the epoll instance.
 * @param fd File descriptor to remove from the epoll watch list.
 * @return Nothing.
 * @errors Calls ERR("epoll_ctl: add fd") and exits if epoll_ctl() fails.
 */
void delete_from_epoll(int epoll_fd, int fd)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
    {
        ERR("epoll_ctl: add fd");
    }
    return;
}

/**
 * Stores a new client descriptor in the first free slot, or rejects it.
 *
 * A free slot is marked by -1. Accepted clients are switched to nonblocking
 * mode and added to the epoll instance for EPOLLIN events. If the client table
 * is full, fd is closed.
 *
 * @param epoll_fd File descriptor of the epoll instance.
 * @param clients Array of client file descriptors, with -1 meaning free.
 * @param maxclients Number of elements in clients.
 * @param fd Newly accepted client file descriptor.
 * @return Index where the client was stored, or -1 if the client was rejected.
 * @errors Calls ERR("fcntl") or ERR("epoll_ctl: add fd") and exits if making
 *         the descriptor nonblocking or adding it to epoll fails. close() errors
 *         while rejecting a client are ignored.
 */
int add_or_reject_client(int epoll_fd, int clients[], int maxclients, int fd)
{

    for (int i = 0; i < maxclients; i++)
    {
        if (clients[i] == -1)
        {
            clients[i] = fd;
            set_nonblocking(fd);
            add_to_epoll(epoll_fd, fd);
            return i;
        }
    }
    close(fd);
    return -1;
}

/**
 * Closes all active client file descriptors from a client table.
 *
 * Slots with value -1 are ignored. This helper does not remove descriptors from
 * epoll; it is intended for shutdown cleanup when the epoll instance is also
 * about to be closed.
 *
 * @param clients Array of client file descriptors, with -1 meaning free.
 * @param maxclients Number of elements in clients.
 * @return Nothing.
 * @errors Does not call ERR and does not exit. close() errors are ignored.
 */
void close_all_clients(int clients[], int maxclients)
{

    for (int i = 0; i < maxclients; i++)
    {
        if (clients[i] != -1)
        {
            close(clients[i]);
        }
    }
}

/**
 * Finds the client-table index that owns a file descriptor.
 *
 * @param fd Client file descriptor to look for.
 * @param clients Array of client file descriptors, with -1 meaning free.
 * @param maxclients Number of elements in clients.
 * @return Index of fd in clients, or -1 if fd is not present.
 * @errors Does not call ERR and does not set errno intentionally.
 */
int find_client_id(int fd, int clients[], int maxclients)
{
    for (int i = 0; i < maxclients; i++)
    {
        if (clients[i] == fd)
        {
            return i;
        }
    }
    return -1;
}

/**
 * Reads one message-sized chunk from a client socket.
 *
 * The function uses recv() with flags set to 0. On successful read, it
 * null-terminates buffer and removes one trailing newline if the received data
 * ends with '\n'. If the peer closes the connection, the descriptor is removed
 * from epoll, removed from the clients table, and closed.
 *
 * @param fd Client socket file descriptor to read from.
 * @param buffer Destination buffer. It must have space for buf_len + 1 bytes,
 *               because this function writes a terminating '\0'.
 * @param buf_len Maximum number of bytes to receive.
 * @param epoll_fd File descriptor of the epoll instance.
 * @param clients Array of client file descriptors, with -1 meaning free.
 * @param maxclients Number of elements in clients.
 * @return Positive number of bytes read after optional newline trimming, 0 if
 *         the peer closed the connection, or -1 if recv() would block on a
 *         nonblocking descriptor.
 * @errors Returns -1 and leaves errno as EAGAIN or EWOULDBLOCK when no data is
 *         currently available on a nonblocking descriptor. Calls ERR("recv")
 *         and exits for other recv() errors. Calls ERR through
 *         delete_from_epoll() if removing a closed client from epoll fails.
 */
int read_from_fd(int fd, char buffer[], int buf_len, int epoll_fd, int clients[], int maxclients)
{
    int r = recv(fd, buffer, buf_len, 0);
    if (r < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return r;
        }
        ERR("recv");
    }
    if (r == 0)
    {
        delete_from_epoll(epoll_fd, fd);
        int id = find_client_id(fd, clients, maxclients);
        clients[id] = -1;
        close(fd);
        return 0;
    }
    buffer[r] = '\0';
    if (buffer[r - 1] == '\n')
    {
        buffer[r - 1] = '\0';
        r--;
    }

    return r;
}

/**
 * Writes a buffer to a client socket and handles common socket failures.
 *
 * The function uses send() with MSG_NOSIGNAL, so writing to a closed socket
 * reports EPIPE instead of delivering SIGPIPE to the process. On nonblocking
 * descriptors, a successful call may still write fewer than buf_len bytes.
 *
 * @param fd Client socket file descriptor to write to.
 * @param buffer Source buffer containing at least buf_len bytes.
 * @param buf_len Number of bytes to write.
 * @param epoll_fd File descriptor of the epoll instance.
 * @param clients Array of client file descriptors, with -1 meaning free.
 * @param maxclients Number of elements in clients.
 * @return Number of bytes written on success. This can be less than buf_len.
 *         Returns -1 if writing would block or the peer connection is
 *         closed/reset.
 * @errors Returns -1 and leaves errno as EAGAIN or EWOULDBLOCK when a
 *         nonblocking descriptor cannot accept more data right now. If errno is
 *         EPIPE or ECONNRESET, removes the descriptor from epoll, removes it
 *         from clients, closes it, and returns -1. Calls ERR("send") and exits
 *         for other write() errors. Calls ERR through delete_from_epoll() if
 *         removing a dead client from epoll fails.
 */
int write_to_fd(int fd, char buffer[], int buf_len, int epoll_fd, int clients[], int maxclients)
{
    int r = send(fd, buffer, buf_len, MSG_NOSIGNAL);
    if (r < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return r;
        }
        if (errno == EPIPE || errno == ECONNRESET)
        {
            int id = find_client_id(fd, clients, maxclients);
            if (id >= 0)
                clients[id] = -1;
            delete_from_epoll(epoll_fd, fd);
            close(fd);
            return r;
        }
        else
        {
            ERR("send");
        }
    }
    return r;
}
