
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cstdint>

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../bt_remote.h"

void print_str_as_hex(std::string str)
{
    for (const uint8_t &c: str)
    {
        if(c >= 0x21 && c <= 0x7e)
        {
            printf(" %c ", c);
        }
        else
        {
            printf("%2x", c);
        }
    }
    printf("\n");
}

int main(int argc, char* argv[])
{
    int ret;
    BTRemote bt_remote;
    ret = bt_remote.setup_remote();
    if(ret)
    {
        printf("BT thread failed to start\n");
        return 1;
    }


    // std::string in_str;
    // while(true)
    // {
    //     std::getline(std::cin, in_str);
    //     std::cout << "Got: " << in_str << std::endl;
    // }
    std::string str_in = "";
    char buf[2049];
    char c;
    int num_read;
    fcntl(0, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK); // make stdin nonblocking so we don't need another thread
    while(true)
    {
        std::vector<std::string> data = bt_remote.get_data();
        for(std::string &s: data)
        {
            printf("got: %s\n", s.c_str());
        }
        memset(buf, 0, 2049);
        num_read = read(STDIN_FILENO, buf, 2048);
        for(int i = 0; i < num_read; ++i)
        {
            // TODO: more efficient
            c = buf[i];
            if(c == '\n')
            {
                printf("sending %s\n", str_in.c_str());
                str_in.push_back('\n');
                bt_remote.send_data(str_in);
                str_in = "";
            }
            else if(c == '\0')
            {
                break;
            }
            else if(c >= 0x20 && c < 0x7f)  // only take printable characters
            {
                str_in.push_back(c);
            }
        }
        sleep(1);
    }

    return 0;
}