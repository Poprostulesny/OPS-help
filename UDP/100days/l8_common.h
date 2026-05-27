#define _GNU_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <semaphore.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

/**
 * @brief Sleep for approximately the requested number of milliseconds.
 *
 * Wraps nanosleep() with a millisecond-based interface. If the sleep is
 * interrupted by a signal, the remaining time is ignored and the function
 * returns early.
 *
 * @param milli Number of milliseconds to sleep.
 *
 * @return This function does not return a value.
 *
 * @note This helper does not report nanosleep() errors. In this program it is
 *       intended for short best-effort delays.
 */
void ms_sleep(unsigned int milli)
{
    struct timespec ts = {milli / 1000, (milli % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

/**
 * @brief Install a simple signal handler for one signal.
 *
 * Registers @p f as the handler for @p sigNo using sigaction(). No special
 * flags are set and the signal mask is left empty.
 *
 * @param f Function called when the signal is delivered.
 * @param sigNo Signal number to handle, for example SIGINT or SIGTERM.
 *
 * @return 0 on success, -1 on failure.
 *
 * @warning On failure, errno is set by sigaction(). Typical failures such as
 *          EINVAL mean the signal number is invalid or cannot be caught.
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
 * @brief Create a socket or terminate the program on failure.
 *
 * Calls socket(@p domain, @p type, 0). This helper is for code paths where a
 * socket creation error is fatal.
 *
 * @param domain Communication domain, for example PF_INET, AF_INET, or AF_UNIX.
 * @param type Socket type, for example SOCK_STREAM or SOCK_DGRAM.
 *
 * @return File descriptor of the newly created socket.
 *
 * @warning If socket() fails, this function prints the error with perror() and
 *          exits the program via ERR("socket"). It never returns -1.
 */
int make_socket(int domain, int type)
{
    int sock;
    sock = socket(domain, type, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

/**
 * @brief Create, bind, and optionally listen on an IPv4 socket.
 *
 * Creates an IPv4 socket bound to INADDR_ANY:@p port. SO_REUSEADDR is enabled
 * before bind(). When @p type is SOCK_STREAM, listen() is also called.
 *
 * @param port Local TCP/UDP port in host byte order. Use 0 to let the system
 *             choose an ephemeral port.
 * @param type Socket type, usually SOCK_STREAM for TCP or SOCK_DGRAM for UDP.
 * @param backlog Listen queue length used only for SOCK_STREAM sockets.
 *
 * @return File descriptor of the bound socket.
 *
 * @warning On socket(), setsockopt(), bind(), or listen() failure, this
 *          function prints the failing call name and exits the program via
 *          ERR(). It never returns an invalid file descriptor.
 */
int bind_inet_socket(uint16_t port, int type, int backlog)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_socket(PF_INET, type);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind");
    if (SOCK_STREAM == type)
        if (listen(socketfd, backlog) < 0)
            ERR("listen");
    return socketfd;
}

/**
 * @brief Read exactly @p count bytes unless EOF or an error occurs.
 *
 * Repeatedly calls read() until the buffer is full, EOF is reached, or a
 * non-retryable error occurs. Interrupted reads (errno == EINTR) are retried
 * automatically by TEMP_FAILURE_RETRY.
 *
 * @param fd File descriptor to read from.
 * @param buf Destination buffer. It must contain at least @p count writable
 *            bytes.
 * @param count Number of bytes requested.
 *
 * @return Number of bytes read. This is @p count on a complete read, less than
 *         @p count on EOF, and -1 on error.
 *
 * @warning On error, returns -1 and leaves errno set by read(). For
 *          non-blocking descriptors, EAGAIN/EWOULDBLOCK means no data is
 *          currently available and the caller may retry later.
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
 * @brief Write exactly @p count bytes unless an error occurs.
 *
 * Repeatedly calls write() until all bytes are written or a non-retryable error
 * occurs. Interrupted writes (errno == EINTR) are retried automatically by
 * TEMP_FAILURE_RETRY.
 *
 * @param fd File descriptor to write to.
 * @param buf Source buffer. It must contain at least @p count readable bytes.
 * @param count Number of bytes to write.
 *
 * @return Number of bytes written. This is @p count on success and -1 on error.
 *
 * @warning On error, returns -1 and leaves errno set by write(). For
 *          non-blocking descriptors, EAGAIN/EWOULDBLOCK means the descriptor
 *          cannot accept more data now and the caller may retry later. EPIPE
 *          means the peer has closed the connection and may also raise SIGPIPE
 *          unless that signal is ignored or blocked.
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
