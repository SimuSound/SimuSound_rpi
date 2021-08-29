
#include <iostream>
#include <cstdio>

#include <pthread.h>
#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "bt_remote.h"

int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void set_mincount(int fd, int mcount)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error tcgetattr: %s\n", strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;        /* half second timer */

    if (tcsetattr(fd, TCSANOW, &tty) < 0)
        printf("Error tcsetattr: %s\n", strerror(errno));
}




#define BT_REMOTE_CONFIG_FILE "../bt_remote.txt"
// TODO: use config file
#define BT_SERIAL_PORT "/dev/rfcomm0"

int BTRemote::setup_remote()
{
    FILE *fp = popen("/usr/bin/stty -F /dev/rfcomm0", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }
    int c;
    while (true) 
    {
        c = fgetc(fp);
        if (c == EOF)
        {
            break;
        }
        printf("%c", (char)c);
    }
    pclose(fp);


    if (pthread_mutex_init(&(this->data_in_mutex), NULL) != 0)
    {
        printf("data_in_mutex init failed\n");
        return 1;
    }

    int ret;
    int fd = open(BT_SERIAL_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        printf("Error opening %s: %s\n", BT_SERIAL_PORT, strerror(errno));
        return -1;
    }
    ret = set_interface_attribs(fd, B115200);
    if (ret)
    {
        printf("Error setting interface attributes on %s: %s\n", BT_SERIAL_PORT, strerror(errno));
        return -1;
    }
    this->bt_remote_fd = fd;

    printf("starting BT thread");

    ret = pthread_create(&(this->t), NULL, &(BTRemote::start_bt_thread_helper), this);
    if (ret)
    {
        printf("Error creating BT remote thread: %d\n", ret);
        return -1;
    }


    return 0;
}

void* BTRemote::bt_thread()
{
    char buf[2049] = {0};
    char c;
    int read_bytes = 0;
    std::string str = "";
    std::vector<std::string> temp_strs;
    while(true)
    {
        memset(buf, 0, 2049);
        read_bytes = read(this->bt_remote_fd, buf, 2048);
        for(int i = 0; i < read_bytes; ++i)
        {
            // TODO: more efficient
            c = buf[i];
            if(c == '\n')
            {
                temp_strs.push_back(str);
                str = "";
            }
            else if(c == '\0')
            {
                break;
            }
            else if(c >= 0x20 && c < 0x7f)  // only take printable characters
            {
                str.push_back(c);
            }
        }
        
        this->data_in.insert(this->data_in.end(), temp_strs.begin(), temp_strs.end());
        temp_strs.clear();

        sleep(1);
    }
}

// class static
void* BTRemote::start_bt_thread_helper(void *context)
{
    return ((BTRemote *)context)->bt_thread();
}

int BTRemote::send_data(std::string data)
{
    int ret;
    ret = write(this->bt_remote_fd, data.c_str(), data.length());
    if(ret != data.length())
    {
        printf("send_data did not write all of data");
        return 1;
    }
    return 0;
}

std::vector<std::string> BTRemote::get_data()
{
    pthread_mutex_lock(&(this->data_in_mutex));
    std::vector<std::string> data(this->data_in);  // copies vector
    pthread_mutex_unlock(&(this->data_in_mutex));
    this->data_in.clear();
    return data;
}