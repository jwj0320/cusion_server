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

#include <wiringPiI2C.h>
#include <wiringPi.h>

#include <mcp3004.h>
#include <softTone.h>


#define DIRECTION_MAX 45
#define BUFFER_MAX 128
#define VALUE_MAX 256

#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1
#define MOTOROUT 14
#define BUTTONPIN 20
#define BUTTONOUT 21

#define BASE 100
#define SPI_CHAN 0

#define BuzzPin 1
#define CM3 330
#define CM5 393

int song_1[] = {CM3,CM3};

int beat_1[] = {2,2};

// Define some device parameters
#define I2C_ADDR   0x27 // I2C device address

// Define some device constants
#define LCD_CHR  1 // Mode - Sending data
#define LCD_CMD  0 // Mode - Sending command

#define LINE1  0x80 // 1st line
#define LINE2  0xC0 // 2nd line
#define LINE3  0x94 // 3rd line


#define LCD_BACKLIGHT   0x08  // On
// LCD_BACKLIGHT = 0x00  # Off

#define ENABLE  0b00000100 // Enable bit

typedef struct socks{
    int serv_sock;
    int clnt_sock;
} socks;

int fd;
char target_degree[BUFFER_MAX] = {30};
char currunt_degree[BUFFER_MAX] ={0};
int mode=0;//vibration=0 / speaker=1

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void lcd_toggle_enable(int bits)   {
  // Toggle enable pin on LCD display
  delayMicroseconds(500);
  wiringPiI2CReadReg8(fd, (bits | ENABLE));
  delayMicroseconds(500);
  wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
  delayMicroseconds(500);
}

void lcd_byte(int bits, int mode)   {

    int bits_high;
    int bits_low;
  
    bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT ;
    bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT ;

    wiringPiI2CReadReg8(fd, bits_high);
    lcd_toggle_enable(bits_high);

    wiringPiI2CReadReg8(fd, bits_low);
    lcd_toggle_enable(bits_low);
}

void typeln(const char *s)   {

    while ( *s ) lcd_byte(*(s++), LCD_CHR);

}

void ClrLcd(void)   {
    lcd_byte(0x01, LCD_CMD);
    lcd_byte(0x02, LCD_CMD);
}

void lcdLoc(int line)   {
    lcd_byte(line, LCD_CMD);
}

void lcd_init()   {
    lcd_byte(0x33, LCD_CMD);
    lcd_byte(0x32, LCD_CMD);
    lcd_byte(0x06, LCD_CMD);
    lcd_byte(0x0C, LCD_CMD);
    lcd_byte(0x28, LCD_CMD);
    lcd_byte(0x01, LCD_CMD);
    delayMicroseconds(500);
}



void lcd_action(){
    char* cD = "Currunt : ";
    char* tD = "Target : ";
    char* md = "Mode : ";

    strcat(cD, currunt_degree);
    strcat(tD, target_degree);
    if(mode == 0){
        strcat(md, "Viberate");
    }
    else{
        strcat(md, "Sound");
    }

    ClrLcd();
    lcdLoc(LINE1);
    typeln(cD);
    lcdLoc(LINE2);
    typeln(tD);
    lcdLoc(LINE3);
    typeln(md);
}

static int GPIOExport(int pin){
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if(-1 == fd){
        fprintf(stderr, "Failed to open export for writing!\n");
        return -1;
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return 0;
}

static int GPIOUnexport(int pin){
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport",O_WRONLY);
    if(-1 == fd){
        fprintf(stderr, "Failed to open unexport for writing!\n");
        return -1;
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return 0;
}

static int GPIODirection(int pin, int dir){
    static const char s_directions_str[] = "in\0out";

    char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    fd = open(path, O_WRONLY);
    if(-1 == fd){
        fprintf(stderr, "Failed to open gpio direction for writng!\n");
        return -1;
    }

    if(-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2: 3)){
        fprintf(stderr, "Failed to set direction!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;

}

static int GPIORead(int pin){
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if(-1 == fd){
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return(-1);
    }

    if(-1 == read(fd, value_str, 3)){
        fprintf(stderr, "Failed to read value!\n");
        close(fd);
        return(-1);
    }

    close(fd);

    return(atoi(value_str));
}

static int GPIOWrite(int pin, int value){
    static const char s_values_str[] = "01";

    char path[VALUE_MAX];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if(-1 == fd){
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return(-1);
    }

    if(1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)){
        fprintf(stderr, "Failed to write value!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(0);
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

    if(-1 == GPIOExport(MOTOROUT))
        return NULL;
    if(-1 == GPIODirection(MOTOROUT, OUT))
        return NULL;
    
    mcp3004Setup(BASE,SPI_CHAN);

    while (1)
    {
        // read and alert

        str_len=read(clnt_sock, msg, sizeof(msg));
        if (str_len == -1)
            error_handling("read() error");
        state=atoi(msg);
        // vibration speeker
        if (state > 0)
        {
            if (mode == 0)
            {
                for (int repeat = 6; repeat < 0; repeat--)
                {
                    if (-1 == GPIOWrite(MOTOROUT, repeat % 2))
                        return NULL;
                    usleep(500 * 1000);
                }
            }
            else
            {
                if (softToneCreate(BuzzPin) == -1)
                {
                    printf("Soft Tone Failed !");
                    return NULL;
                }
                // printf("Sound is generated...\n");
                for (int i = 0; i < sizeof(song_1) / 4; i++)
                {
                    softToneWrite(BuzzPin, song_1[i]);
                    delay(beat_1[i] * 500);
                }
                softToneStop(BuzzPin);
            }
        }

        usleep(500 * 100);
    }
    close(clnt_sock);
    close(serv_sock);

    return NULL;
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
        // temperature lcd
        strcpy(currunt_degree, msg);
        lcd_action();

        usleep(500 * 100);
    }
    close(clnt_sock);
    close(serv_sock);

    return NULL;
}


void *button_action(void *h_sock)
{
    char msg[BUFFER_MAX];
    int clnt_sock=((struct socks *)h_sock)->clnt_sock;
    char* target[3] = {"30", "35", "40"};
    int time_check = 0;
    int t = 0;

    if(-1 == GPIOExport(BUTTONOUT) || -1 == GPIOExport(BUTTONPIN))
        return NULL;

    if(-1 == GPIODirection(BUTTONOUT, OUT) || -1 == GPIODirection(BUTTONPIN, IN))
        return NULL;

    while (1)
    {
        if(GPIORead(BUTTONPIN) == 1 && time_check < 3){
            time_check++;
        }
        else if(GPIORead(BUTTONPIN) == 1 && time_check == 3){
            mode += 1;
            mode = mode % 2;

            lcd_action();

            time_check = 0;
        }
        else if(GPIORead(BUTTONPIN) == 0 && time_check != 0){
            t++;
            t = t % 3;
            
            strcpy(target_degree, target[t]);
            strcpy(msg, target[t]);

            lcd_action();

            write(clnt_sock, msg, sizeof(msg));

            time_check = 0;
        }

        usleep(500 * 100);
    }


    return NULL;
}

int main()
{
    pthread_t p_thread[3];
    int thr_id;
    int status;
    char p1[]="thread_1";
    char p2[]="thread_2";

    if (wiringPiSetup () == -1) exit (1);

    fd = wiringPiI2CSetup(I2C_ADDR);
    
    lcdLoc(LINE1);
    typeln("Hello World!");
    lcdLoc(LINE2);
    typeln("-CUSION-");

    lcd_init(); // setup LCD

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