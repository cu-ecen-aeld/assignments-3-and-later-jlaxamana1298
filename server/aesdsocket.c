#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/queue.h>

#define MAX_LEN 1024
#define TMPFILE "/var/tmp/aesdsocketdata"

pthread_mutex_t lock;
volatile sig_atomic_t sig_caught = 0;

struct conn_thread_data
{
    pthread_mutex_t* mutex;
    int sockfd_in;
    struct sockaddr_in addr_client;
    bool thread_complete;
    bool thread_complete_success;
};


struct conn_thread
{
    pthread_t thread;
    struct conn_thread_data thread_data;
    SLIST_ENTRY(conn_thread) entries;
};


struct timer_thread_data
{
    pthread_mutex_t* mutex;
    bool thread_complete;
    bool thread_complete_success;
};


void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        syslog(LOG_INFO, "Signal caught");
        sig_caught = 1;
    }
}


bool check_newline(char s[], size_t size)
{
    for (int i = 0; i < size; i++)
    {
        if (s[i] == '\n')
        {
            return true;
        }
    }
    return false;
}


void timer_thread(union sigval timer_data)
{
    struct timer_thread_data* thread_args = timer_data.sival_ptr;
    time_t rawtime;
    struct tm *info;
    char buffer[80];
    
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%F %T", info);
    
    FILE* file = fopen(TMPFILE, "a");
    if (file == NULL)
    {
        syslog(LOG_ERR, "error openning file in timer thread");
        thread_args->thread_complete = true;
        thread_args->thread_complete_success = false;
        return;
    }
    
    int rc;
    rc = pthread_mutex_lock(thread_args->mutex);
    if (rc != 0)
    {
        syslog(LOG_ERR, "Mutex lock failed in timer_thread");
        thread_args->thread_complete = true;
        thread_args->thread_complete_success = false;
        return;
    }
    
    if (fprintf(file, "timestamp:%s\n", buffer) < 0)
    {
        syslog(LOG_ERR, "Error writing to file in timer_thread");
        thread_args->thread_complete = true;
        thread_args->thread_complete_success = false;
        return;
    }
    
    rc = pthread_mutex_unlock(thread_args->mutex);
    if (rc != 0)
    {
        syslog(LOG_ERR, "Mutex failed unlock in timer_thread");
        thread_args->thread_complete = true;
        thread_args->thread_complete_success = false;
        return;
    }
    
    if (fclose(file) == EOF)
    {
        syslog(LOG_ERR, "error closing file in timer_thread");
        thread_args->thread_complete = true;
        thread_args->thread_complete_success = false;
        return;
    }
    
    thread_args->thread_complete = true;
    thread_args->thread_complete_success = false;
}


void* handle_conn(void* conn_data)
{
    int rc;
    struct conn_thread_data* thread_args = (struct conn_thread_data *) conn_data;
    
    FILE* file = fopen(TMPFILE, "a");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening file in handle_conn");
        thread_args->thread_complete = true;
        thread_args->thread_complete_success = false;
        return conn_data;
    }
    
    char buffer[MAX_LEN] = {0};
    int bytes_received;
    while ((bytes_received = recv(thread_args->sockfd_in, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        if (sig_caught)
        {
            thread_args->thread_complete = true;
            return conn_data;
        }
        
        if (bytes_received < 0)
        {
            syslog(LOG_ERR, "Error receiving data in handle_conn");
            thread_args->thread_complete = true;
            thread_args->thread_complete_success = false;
            return conn_data;
        }
        
        rc = pthread_mutex_lock(thread_args->mutex);
        if (rc != 0)
        {
            syslog(LOG_ERR, "Mutex lock failed in handle_conn");
            thread_args->thread_complete = true;
            thread_args->thread_complete_success = false;
            return conn_data;
        }
        
        if (fwrite(buffer, sizeof(char), bytes_received, file) < 0)
        {
            syslog(LOG_ERR, "Error writing to file in handle_conn");
            thread_args->thread_complete = true;
            thread_args->thread_complete_success = false;
            return conn_data;
        }
        
        rc = pthread_mutex_unlock(thread_args->mutex);
        if (rc != 0)
        {
            syslog(LOG_ERR, "Mutex unlock failed in handle_conn");
            thread_args->thread_complete = true;
            thread_args->thread_complete_success = false;
            return conn_data;
        }
        
        if (strchr(buffer, '\n') != NULL)
        {
            break;
        }
    }
    
    if (fclose(file) == EOF)
    {
        syslog(LOG_ERR, "Error closing file in handle_conn");
        thread_args->thread_complete = true;
        thread_args->thread_complete_success = false;
        return conn_data;
    }
    
    file = fopen(TMPFILE, "r");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening read file in handle_conn");
        thread_args->thread_complete = true;
        thread_args->thread_complete_success = false;
        return conn_data;
    }
    
    char buff[MAX_LEN] = {0};
    int bytes_read;
    while ((bytes_read = fread(buff, sizeof(char), sizeof(buff), file)) > 0)
    {
        if (send(thread_args->sockfd_in, buff, bytes_read, 0) < 0)
        {
            syslog(LOG_ERR, "Error sending data in handle_conn");
            thread_args->thread_complete = true;
            thread_args->thread_complete_success = false;
            return conn_data;
        }
    }
    
    close(thread_args->sockfd_in);
    syslog(LOG_INFO, "closed connection in handle_conn");
    thread_args->thread_complete = true;
    thread_args->thread_complete_success = true;
    
    return conn_data;
}


int main(int argc, char *argv[])
{
    // init variables
    int sockfd, status, opt = 1;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    bool rundaemon = false;
    
    SLIST_HEAD(list_head, conn_thread) threads_head;
    SLIST_INIT(&threads_head);
    
    if (pthread_mutex_init(&lock, NULL) < 0)
    {
        syslog(LOG_ERR, "Error initalizing mutex");
        exit(1);
    }
    
    struct sigaction new_action = {.sa_handler = signal_handler};
    
    // check daemon mode
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        rundaemon = true;
    }
    
    if (sigaction(SIGTERM, &new_action, NULL) != 0)
    {
        syslog(LOG_ERR, "Error for SIGTERM handler");
        exit(1);
    }
    if (sigaction(SIGINT, &new_action, NULL) != 0)
    {
        syslog(LOG_ERR, "Error for SIGINT handler");
        exit(1);
    }
    
    // set up logger
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_DAEMON);
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0)
    {
        syslog(LOG_ERR, "getaddrinfo error");
        exit(1);
    }
    
    // create socket
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) < 0)
    {
        syslog(LOG_ERR, "Error opening socket");
        exit(1);
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        syslog(LOG_ERR, "setsockopt error");
        exit(1);
    }
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
    {
        syslog(LOG_ERR, "bind error");
        exit(1);
    }
    
    if (rundaemon)
    {
        daemon(0, 0);
    }
    
    if (listen(sockfd, 5) < 0)
    {
        syslog(LOG_ERR, "listen error");
        exit(1);
    }
    
    struct timer_thread_data timer_data = {
        .mutex = &lock,
        .thread_complete = false,
        .thread_complete_success = true
    };
    
    timer_t timer;
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_ptr = &timer_data,
        .sigev_notify_function = &timer_thread
    };
    struct itimerspec its = {
        .it_value.tv_sec = 0,
        .it_value.tv_nsec = 1,
        .it_interval.tv_sec = 10,
        .it_interval.tv_nsec = 0
    };
    
    if (timer_create(CLOCK_MONOTONIC, &sev, &timer) != 0)
    {
        syslog(LOG_ERR, "Error creating timer");
        exit(1);
    }
    if (timer_settime(timer, 0, &its, NULL) != 0)
    {
        syslog(LOG_ERR, "Error starting timer");
        exit(1);
    }
    
    while (!sig_caught)
    {
        int sockfd_in;
        struct sockaddr_in addr_client;
        socklen_t sockaddr_client_len = sizeof(addr_client);
        
        if ((sockfd_in = accept(sockfd, (struct sockaddr*) &addr_client, &sockaddr_client_len)) < 0)
        {
            syslog(LOG_ERR, "accept error");
            break;
        }
        
        struct conn_thread* finished_threads[64] = {NULL};
        struct conn_thread* tmp_thread;
        int thread_index = 0;
        SLIST_FOREACH(tmp_thread, &threads_head, entries)
        {
            if (tmp_thread->thread_data.thread_complete)
            {
                finished_threads[thread_index++] = tmp_thread;
            }
        }
        for (int i = 0; i < thread_index; i++)
        {
            pthread_join(finished_threads[i]->thread, NULL);
            SLIST_REMOVE(&threads_head, finished_threads[i], conn_thread, entries);
            free(finished_threads[i]);
        }
        
        syslog(LOG_INFO, "Connection accepted");
        
        struct conn_thread_data data = {
            .mutex = &lock,
            .sockfd_in = sockfd_in,
            .addr_client = addr_client,
            .thread_complete = false,
            .thread_complete_success = true
        };
        struct conn_thread* new_thread = malloc(sizeof(struct conn_thread));
        new_thread->thread_data = data;
        int rc = pthread_create(&new_thread->thread, NULL, handle_conn, &new_thread->thread_data);
        if (rc != 0)
        {
            syslog(LOG_ERR, "Failed to create thread");
            new_thread->thread_data.thread_complete_success = false;
            break;
        }
        
        SLIST_INSERT_HEAD(&threads_head, new_thread, entries);
    }
    
    while (!SLIST_EMPTY(&threads_head))
    {
        struct conn_thread* tmp = SLIST_FIRST(&threads_head);
        pthread_join(tmp->thread, NULL);
        SLIST_REMOVE_HEAD(&threads_head, entries);
        free(tmp);
    }
    
    timer_delete(timer);
    pthread_mutex_destroy(&lock);
    close(sockfd);
    freeaddrinfo(servinfo);
    
    remove(TMPFILE);
    return 0;
}
