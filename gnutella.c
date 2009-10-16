/*
   gnutella.c: A Gnutella Ultrapeer plug-in for ion-sampler

   Copyright (C) 2006-2009 Daniel Stutzbach

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "heap.h"
#include "queue.h"

static struct queue *queue;
static float timeout = 10;
static int max_connections = 4000;

static void maybe_dequeue(void);

struct timer
{
        heap_loc_t heap_loc;
        float time;
        void (*func)(void *data);
        void *data;
};

int timer_cmp(const void *v1, const void *v2)
{
        const struct timer *t1  = v1, *t2 = v2;
        return cmp3(t1->time, t2->time);
}

struct event_handler
{
        void (*func)(void *data);
        void *data;
        struct pollfd *pollfd;
};

static struct heap *timers = NULL;
static struct pollfd *pollfds;
static struct event_handler **event_handlers;
static int max_pollfds;
static int num_pollfds;
static unsigned start_time;

float get_now(void)
{
        struct timespec timespec;
        if (0 > clock_gettime(CLOCK_MONOTONIC, &timespec)) die();
        return (timespec.tv_sec - start_time) + timespec.tv_nsec/1000000000.0;
}

void init(void)
{
        struct timespec timespec;

        queue = queue_new();

        if (0 > clock_getres(CLOCK_MONOTONIC, &timespec)) die();

        /* 5 ms resolution should be _plenty_ */
        if (timespec.tv_sec || timespec.tv_nsec > 5000000) die();

        start_time = 0;
        start_time = get_now();
        
        if (timers) die();

        timers = heap_new(timer_cmp, offsetof(struct timer, heap_loc));

        max_pollfds = 128;
        num_pollfds = 0;
        myallocn(pollfds, max_pollfds);
        myallocn(event_handlers, max_pollfds);
}

struct timer *timer_new(float delay_seconds, void (*func) (void *data),
                        void *data)
{
        struct timer *timer;
        myalloc(timer);
        timer->time = get_now() + delay_seconds;
        timer->func = func;
        timer->data = data;
        heap_insert(timers, timer);
        return timer;
}

void timer_cancel(struct timer *timer)
{
        if (!heap_in(timers, timer)) return; /* Called from this timer */

        heap_remove(timers, timer);
        free(timer);
}

void timer_reset(struct timer *timer, float delay_seconds)
{
        timer->time = get_now() + delay_seconds;
        heap_remove(timers, timer);
        heap_insert(timers, timer);
}

struct event_handler *event_handler_new(int fd)
{
        struct pollfd *pollfd;
        struct event_handler *event_handler;

        if (num_pollfds == max_pollfds) {
                max_pollfds <<= 1;
                myrealloc(pollfds, max_pollfds);
                myrealloc(event_handlers, max_pollfds);
                for (int i = 0; i < num_pollfds; i++)
                        event_handlers[i]->pollfd = &pollfds[i];
        }

        myalloc(event_handler);
        event_handlers[num_pollfds] = event_handler;

        pollfd = &pollfds[num_pollfds++];
        memset(pollfd, 0, sizeof (*pollfd));

        event_handler->pollfd = pollfd;
        pollfd->fd = fd;

        /* Wake up on data or errors */
        pollfd->events = POLLIN | POLLPRI | POLLERR | POLLHUP  | POLLNVAL;
        
        return event_handler;
}

void event_handler_delete(struct event_handler *event_handler)
{
        int idx = event_handler->pollfd - pollfds;
        if (0 > close(event_handler->pollfd->fd)) die();
        if (idx < num_pollfds-1) {
                swap(pollfds[idx], pollfds[num_pollfds-1]);
                swap(event_handlers[idx], event_handlers[num_pollfds-1]);
                event_handlers[idx]->pollfd = &pollfds[idx];
        }
        free(event_handler);
        num_pollfds--;
}

void main_loop(void)
{
        int n;
        struct timer *timer;
        int delay;

        while (num_pollfds > 1 || heap_len(timers) > 1
               || pollfds[0].events & POLLOUT) {
                if (heap_empty(timers)) {
                        timer = NULL;
                        delay = -1;
                } else {
                        timer = heap_peek(timers);
                        delay = (timer->time - get_now()) * 1000;
                }
                
                if (timer && delay <= 0)
                        n = 0;
                else
                        n = poll(pollfds, num_pollfds, delay);

                if (!n) {
                        timer = heap_extract_min(timers);
                        timer->func(timer->data);
                        free(timer);
                        goto cont;
                }

                for (int i = 0; i < num_pollfds && n; i++) {
                        if (pollfds[i].revents) {
                                event_handlers[i]->func(event_handlers[i]->data);
                                n--;

                                /* We may have just deleted this id.
                                 * Try it again in case it's a new
                                 * one. */
                                i--;
                        }
                }

        cont:
                maybe_dequeue();                
        }
}

struct file
{
        char *wbuf;
        char *rbuf;
        unsigned wlen;
        unsigned rlen;
        unsigned wmax;
        unsigned rmax;
        struct event_handler *event_handler;
        bool deleted;
        bool eof;
        void (*err_handler)(void *data);
        void *err_data;
        void (*read_handler)(void *data);
        void *read_data;
};

#define BLOCK_SIZE 4096

void file_handler(void *vfile);

void file_err_handler(void *vfile __unused)
{
        fprintf(stderr, "Unhandled file error\n");
        exit(1);
}

struct file *file_new(int fd)
{
        struct file *file;
        int value;
        
        myalloc(file);
        file->wmax = BLOCK_SIZE;
        file->rmax = 2*BLOCK_SIZE;
        myallocn(file->wbuf, file->wmax);
        myallocn(file->rbuf, file->rmax);
        file->event_handler = event_handler_new(fd);
        file->event_handler->func = file_handler;
        file->event_handler->data = file;
        file->err_handler = file_err_handler;
        file->err_data = file;

        /* This shouldn't be needed.  We set it for debugging purposes. */
        value = fcntl(fd, F_GETFL, O_NONBLOCK);
        if (value == -1) die();
        if (fcntl(fd, F_SETFL, value | O_NONBLOCK) < 0) die();

        return file;
}

void file_write(struct file *file, const void *data, size_t n)
{
        if (!n) return;
        grow(file->wbuf, file->wmax, file->wlen + n);
        memcpy(&file->wbuf[file->wlen], data, n);
        file->wlen += n;
        file->event_handler->pollfd->events |= POLLOUT;
}

void file_vprintf(struct file *file, const char *format, va_list ap)
{
        va_list ap2;
        va_copy(ap2, ap);
        int n = vsnprintf(&file->wbuf[file->wlen], file->wmax - file->wlen,
                          format, ap);
        if (n < 0) die ();
        n++; /* Account for the trailing nul byte */
        if ((unsigned) n > file->wmax - file->wlen) {
                grow(file->wbuf, file->wmax, file->wlen + n);
                n = vsnprintf(&file->wbuf[file->wlen], file->wmax - file->wlen,
                              format, ap2);
                if (n < 0) die();
                n++; /* Account for the trailing nul byte */
                if ((unsigned) n > file->wmax - file->wlen) die();
        }
        va_end(ap2);

        file->wlen += n - 1; /* Don't count trailing nul byte */
        file->event_handler->pollfd->events |= POLLOUT;                
}

void file_printf(struct file *file, const char *format, ...)
{
        va_list ap;
        va_start(ap, format);
        file_vprintf(file, format, ap);
        va_end(ap);
}

static void _file_delete(struct file *file)
{
        event_handler_delete(file->event_handler);
        free(file->wbuf);
        free(file->rbuf);
        free(file);
}

static bool in_file_handler = False;
void file_delete(struct file *file)
{
        /* Delay actual freeing of resources */
        file->deleted = True;
        if (!in_file_handler) _file_delete(file);
}

void file_handler(void *vfile)
{
        struct file *file = vfile;
        in_file_handler = True;
        short revents = file->event_handler->pollfd->revents;

        if (revents & (POLLERR | POLLHUP | POLLNVAL | POLLPRI)) {
        error:
                file->err_handler(file->err_data);
                goto deleted;
        }

        if (revents & POLLOUT) {
                int n;
                if (!file->wlen) die();

                n = write(file->event_handler->pollfd->fd,
                          file->wbuf, file->wlen);
                if (!n) die();
                if (n < 0) {
                        if (errno == EAGAIN) die();
                        if (errno != EINTR) {
                                goto error;
                        }
                } else if ((unsigned) n > file->wlen) die();
                else {
                        memmove(file->wbuf, &file->wbuf[n], file->wlen - n);
                        file->wlen -= n;
                }
        }

        if (revents & POLLIN) {
                int n;

                grow(file->rbuf, file->rmax, file->rlen + BLOCK_SIZE);
                
                n = read(file->event_handler->pollfd->fd,
                         &file->rbuf[file->rlen], BLOCK_SIZE);
                if (!n) {
                        file->eof = True;
                } else if (n < 0) {
                        if (errno == EAGAIN) die();
                        if (errno != EINTR) {
                                goto error;
                        }
                } else {
                        file->rlen += n;
                        file->read_handler(file->read_data);
                }
        }

        file->event_handler->pollfd->revents = 0;

        if (file->wlen)
                file->event_handler->pollfd->events |= POLLOUT;
        else {
                file->event_handler->pollfd->events &= ~POLLOUT;
                if (file->eof) goto error;
                if (file->deleted) {
                deleted:
                        _file_delete(file);
                }
        }
        in_file_handler = False;
}

struct file *file_stdout = NULL;
struct file *file_stdin = NULL;

void file_init(void)
{
        if (file_stdin || file_stdout) die();
        file_stdout = file_new(STDOUT_FILENO);
        file_stdin = file_new(STDIN_FILENO);
        file_stdout->event_handler->pollfd->events &= ~POLLIN;
}

struct read_line
{
        void (*line_handler)(void *data, char *line);
        void *data;
        struct file *file;
};

void read_line_handler(void *vread_line)
{
        struct read_line *read_line = vread_line;
        struct file *file = read_line->file;
        int last = 0;

        for (unsigned i = 0; i < file->rlen; i++) {
                if (file->rbuf[i] == '\n') {
                        file->rbuf[i] = 0;
                        read_line->line_handler(read_line->data,
                                                &file->rbuf[last]);
                        last = i+1;
                } else if (i+1 < file->rlen &&
                           file->rbuf[i] == '\r' && file->rbuf[i+1] == '\n') {
                        file->rbuf[i] = 0;
                        read_line->line_handler(read_line->data,
                                                &file->rbuf[last]);
                        last = i+2;
                        i++;
                }
                if (file->deleted) return;
        }

        memmove(file->rbuf, &file->rbuf[last], file->rlen - last);
        file->rlen -= last;
}

struct read_line *
read_line_new(struct file *file,
              void (*line_handler)(void *data, char *line), void *data)
{
        struct read_line *read_line;
        myalloc(read_line);
        read_line->line_handler = line_handler;
        read_line->data = data;
        read_line->file = file;
        file->read_handler = read_line_handler;
        file->read_data = read_line;
        return read_line;
}

void read_line_delete(struct read_line *read_line)
{
        free(read_line);
}

struct gnutella_conn
{
        struct read_line *read_line;
        struct file *file;
        char *addr;
        char *user_agent;
        const char *peer_type;
        char *neighbors;
        char *leafs;
        struct timer *timer;
};

void gnutella_delete(struct gnutella_conn *conn)
{
        read_line_delete(conn->read_line);
        file_delete(conn->file);
        free(conn->addr);
        if (conn->user_agent) free(conn->user_agent);
        if (conn->neighbors) free(conn->neighbors);
        if (conn->leafs) free(conn->leafs);
        timer_cancel(conn->timer);
        free(conn);
}

static void report_error(const char *addr, const char *format, ...)
{
        va_list ap;
        va_start(ap, format);
        file_printf(file_stdout, "R: %s(): ", addr);
        file_vprintf(file_stdout, format, ap);
        file_write(file_stdout, "\n", 1);
        va_end(ap);        
}

void gnutella_err_handler(void *vconn)
{
        struct gnutella_conn *conn = vconn;
        int err;
        socklen_t optlen = sizeof err;
        if (0 > getsockopt(conn->file->event_handler->pollfd->fd,
                           SOL_SOCKET, SO_ERROR, &err, &optlen)) die();
        if (err) report_error(conn->addr, "Failed: %s", strerror(err));
        else report_error(conn->addr, "Connection Dropped");
        gnutella_delete(conn);
}

static void gnutella_timeout(void *vconn);
void gnutella_line_handler1(void *bconn, char *line);
void gnutella_line_handler2(void *bconn, char *line);
void gnutella_conn_new(char *addr);

static void maybe_dequeue(void)
{
        if (num_pollfds >= max_connections) return;
        if (queue_empty(queue)) return;

        /* Make sure there are still file descriptors available */
        int fd = open("/dev/null", O_RDONLY);
        if (0 > fd) return;
        close(fd);

        gnutella_conn_new(queue_pop(queue));

        maybe_dequeue();
}

void gnutella_conn_queue(const char *caddr)
{
        char *addr = strdup(caddr);
        queue_push(queue, addr);
}

void gnutella_conn_new(char *addr)
{
        struct gnutella_conn *conn;
        struct sockaddr_in sin;
        int fd = -1;
        unsigned i;
        char ip[16];
        unsigned long int port;
        char *endptr;
        int err;
        int value;

        for (i = 0; addr[i] != ':'; i++) {
                if (!addr[i]) goto bad_address;
                if (i > (sizeof ip) - 1) goto bad_address;
                ip[i] = addr[i];
        }
        ip[i++] = 0;
                
        if (0 > inet_aton(ip, &sin.sin_addr)) goto bad_address;
        port = strtoul(&addr[i], &endptr, 10);
        if (!addr[i] || *endptr) goto bad_address;
        if (port > 65535) goto bad_address;
        sin.sin_port = htons(port);
        sin.sin_family = AF_INET;

        /* Setup connection */
        fd = socket(PF_INET, SOCK_STREAM, 0);
        if (0 > fd) die();

        value = fcntl(fd, F_GETFL, O_NONBLOCK);
        if (value == -1) die();
        if (fcntl(fd, F_SETFL, value | O_NONBLOCK) < 0) die();

        err = connect(fd, (struct sockaddr *) &sin, sizeof sin);
        if (0 > err) {
                if (errno == EINPROGRESS) { /* This is OK */ } 
                else if (errno == EAGAIN) {
                bad_address:
                        report_error(addr, "Bind error");
                        goto error;
                }
                else {
                        report_error(addr, "Failed: %s", strerror(errno));
                error:
                        if (fd >= 0) close(fd);
                        return;
                }
        }

        myalloc(conn);

        conn->addr = addr;
        conn->file = file_new(fd);
        conn->file->err_handler = gnutella_err_handler;
        conn->file->err_data = conn;
        conn->read_line = read_line_new(conn->file, gnutella_line_handler1,
                                        conn);
        conn->timer = timer_new(timeout, gnutella_timeout, conn);
        conn->peer_type = "Peer";

        file_printf(conn->file, "GNUTELLA CONNECT/0.6\r\n" 
                   "User-Agent: Cruiser (http://mirage.cs.uoregon.edu/P2P/root-tools.html)\r\n" 
                   "X-Ultrapeer: False\r\n"     
                   "Crawler: 0.1\r\n"           
                   "\r\n");                   
}

static void gnutella_timeout(void *vconn)
{
        struct gnutella_conn *conn = vconn;
        report_error(conn->addr, "Timeout");
        gnutella_delete(conn);
}

void gnutella_update_timer(struct gnutella_conn *conn)
{
        timer_reset(conn->timer, timeout);
}

void gnutella_line_handler1(void *vconn, char *line)
{
        struct gnutella_conn *conn = vconn;
        int code;
        
        if (0 != strncmp(line, "GNUTELLA/0.6 ", 13)) {
        bad_handshake:
                report_error(conn->addr, "Bad Handshake %s", line);
                gnutella_delete(conn);
                return;
        }

        line += 13;

        code = atoi (line);
        if (code != 200 && code != 503 && code != 593)
                goto bad_handshake;

        conn->read_line->line_handler = gnutella_line_handler2;

        gnutella_update_timer(conn);
}

void string_extend(char **ps, const char *s2)
{
        char *s = *ps;

        if (!s) *ps = strdup(s2);
        else {
                if (0 > asprintf(ps, "%s %s", s, s2)) die();
                free(s);
        }
}

static void report_neighbors(const char *addr, const char *user_agent,
                             const char *peer_type,
                             const char *neighbors, const char *leafs)
{
        file_printf(file_stdout, "R: %s(|%s|): %s %s, %s\n",
                    addr, user_agent, peer_type, neighbors, leafs);
}

static void gnutella_line_handler_done(struct gnutella_conn *conn)
{
        static char nothing[] = "";

        if (!conn->user_agent) conn->user_agent = nothing;
        if (!conn->neighbors) conn->neighbors = nothing;
        if (!conn->leafs) conn->leafs = nothing;

        report_neighbors(conn->addr, conn->user_agent, conn->peer_type,
                         conn->neighbors, conn->leafs);

        if (conn->user_agent == &nothing[0]) conn->user_agent = NULL;
        if (conn->neighbors == &nothing[0]) conn->neighbors = NULL;
        if (conn->leafs == &nothing[0]) conn->leafs = NULL;
        
        gnutella_delete(conn);
}

void gnutella_line_handler2(void *vconn, char *line)
{
        struct gnutella_conn *conn = vconn;
        char *colon, *label, *value, *s;

        if (!*line) {
                gnutella_line_handler_done(conn);
                return;
        }
        
        colon = strchr(line, ':');
        if (!colon) {
                report_error(conn->addr, "Bad Headers: %s", line);
                gnutella_delete(conn);
                return;
        }

        *colon = 0;
        label = line;
        value = colon+1;
        while (*value && isspace(*value)) value++;

        if (0 == strcmp("X-Ultrapeer", label)) {
                if (0 != strcmp(conn->peer_type, "Peer")) {
                        report_error(conn->addr, "Multiple X-Ultrapeer");
                        gnutella_delete(conn);
                        return;
                }
                
                if (0 == strcasecmp("true", value)) {
                        conn->peer_type = "Ultrapeer";
                } else if (0 == strcasecmp("false", value)) {
                        conn->peer_type = "Leaf";
                } else {
                        report_error(conn->addr, "Bad X-Ultrapeer: %s", value);
                        gnutella_delete(conn);
                        return;
                }
        } else if (0 == strcmp("Peers", label)) {
                for (s = value; *s; s++) if (*s == ',') *s = ' ';
                string_extend(&conn->neighbors, value);
        } else if (0 == strcmp("Leaves", label)) {
                for (s = value; *s; s++) if (*s == ',') *s = ' ';
                string_extend(&conn->leafs, value);
        } else if (0 == strcmp("User-Agent", label)) {
                string_extend(&conn->user_agent, value);
        }

        gnutella_update_timer(conn);
}

static void stdin_line_handler(void *v __unused, char *line)
{
        gnutella_conn_queue(line);
}

void stdin_err_handler(void *vfile __unused)
{
}

void tick(void *vdata __unused)
{
        file_printf(file_stdout, "Q: %d %d\n", queue_len(queue), num_pollfds - 2);
        timer_new(0.01, tick, NULL);
}

int main(void)
{
        struct read_line *stdin_read_line;
        init();
        file_init();

        //int fd = open("gnutella.in", O_RDONLY);
        //if (0 > fd) die();
        //struct file *file = file_new(fd);
        //file_delete(file_stdin);
        stdin_read_line = read_line_new(file_stdin, stdin_line_handler, NULL);
        file_stdin->err_handler = stdin_err_handler;
        timer_new(1, tick, NULL);
        
        main_loop();

        file_delete(file_stdout);
        read_line_delete(stdin_read_line);
        fclose(stderr);
        
        return 0;
}
