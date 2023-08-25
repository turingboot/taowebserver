// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <error.h>
// #include <fcntl.h>      
// #include <sys/epoll.h>
// #include <signal.h>
// #include <assert.h>


#include "../include/server/webserver.h"

int main(){
    
    // TaoTaoWebserver::addsig(SIGPIPE, SIG_IGN);
    TaoWebserver tao(10000, 5, 60000, false, 12);
    tao.run();
    return 0;
}