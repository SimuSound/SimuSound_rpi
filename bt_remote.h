#ifndef BT_REMOTE_H
#define BT_REMOTE_H

#include <queue>

#include <pthread.h>

class BTRemote
{
public:
    int setup_remote();

    void *bt_thread();

    static void *start_bt_thread_helper(void *context);

    int send_data(std::string data);
    std::vector<std::string> get_data();

    int bt_remote_fd;

    pthread_t t;

    std::vector<std::string> data_in;
    pthread_mutex_t data_in_mutex;
};

#endif /* BT_REMOTE_H */
