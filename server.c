#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>


#define DIRECTION_MAX 45
#define BUFFER_MAX 128
#define VALUE_MAX 256

typedef struct socks{
    int serv_sock;
    int clnt_sock;
} socks;

int mode=0;//vibration=0 / speaker=1

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

struct socks create_sock(const int port)
{
    int serv_sock, clnt_sock = -1;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;
    char msg[BUFFER_MAX];
    int str_len;

    struct socks sockets;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");
    if (clnt_sock < 0)
    {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr,
                           &clnt_addr_size);
        if (clnt_sock == -1)
            error_handling("accept() error");
    }

    sockets.serv_sock=serv_sock;
    sockets.clnt_sock=clnt_sock;

    return sockets;
}

void *press_sock(void* p_sock)
{
    char msg[BUFFER_MAX];
    int str_len;
    int state;
    int serv_sock=((struct socks*)p_sock)->serv_sock;
    int clnt_sock=((struct socks*)p_sock)->clnt_sock;

    while (1)
    {
        // read and alert

        str_len=read(clnt_sock, msg, sizeof(msg));
        if (str_len == -1)
            error_handling("read() error");
        state=atoi(msg);
        // vibration speeker

        
        usleep(500 * 100);
    }
    close(clnt_sock);
    close(serv_sock);

    return (0);
}

void *heat_sock(void *h_sock)
{
    char msg[BUFFER_MAX];
    int str_len;
    double degree;
    int serv_sock=((struct socks *)h_sock)->serv_sock;
    int clnt_sock=((struct socks *)h_sock)->clnt_sock;

    while (1)
    {
        // read and alert

        str_len=read(clnt_sock, msg, sizeof(msg));
        if (str_len == -1)
            error_handling("read() error");
        degree=atof(msg);
        // temperature lcd 


        usleep(500 * 100);
    }
    close(clnt_sock);
    close(serv_sock);

    return (0);
}


void *button_action(void *h_sock)
{
    char msg[BUFFER_MAX];
    double degree;
    int clnt_sock=((struct socks *)h_sock)->clnt_sock;

    while (1)
    {
        // button
        // if and ...
        write(clnt_sock, msg, sizeof(msg));


        usleep(500 * 100);
    }


    return (0);
}

int main()
{
    pthread_t p_thread[3];
    int thr_id;
    int status;
    char p1[]="thread_1";
    char p2[]="thread_2";

    struct socks p_sock = create_sock(8888);
    struct socks h_sock = create_sock(9999);

    thr_id=pthread_create(&p_thread[0],NULL, press_sock,(void*)&p_sock);
    if(thr_id<0)
    {
        perror("thread create error : ");
        exit(0);
    }
    thr_id=pthread_create(&p_thread[1], NULL, heat_sock, (void*)&h_sock);
    if(thr_id<0)
    {
        perror("thread create error : ");
        exit(0);
    }

    thr_id=pthread_create(&p_thread[2], NULL, button_action, (void*)&h_sock);
    if(thr_id<0)
    {
        perror("thread create error : ");
        exit(0);
    }

    pthread_join(p_thread[0], (void **)&status);
    pthread_join(p_thread[1], (void **)&status);
    pthread_join(p_thread[2], (void **)&status);

    return 0;
}