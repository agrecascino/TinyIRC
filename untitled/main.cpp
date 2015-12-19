#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sys/types.h>
#include <thread>
using namespace std;

sockaddr_in serv_addr;
void TCPAccept();
char sendBuff[1025];

int sockt = 0;
struct l
{
    l(int c)
    {
       k = c;
    }

    int k;
};
vector<l> OpenFDS;

int main()
{
    cout << "e" << endl;
    sockt = socket(AF_INET,SOCK_STREAM,0);
    int g = 1;
    setsockopt(sockt,SOL_SOCKET,SO_REUSEADDR,&g,sizeof(int));
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(6667);
    bind(sockt, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
    if(listen(sockt, 10) == -1){
        printf("Failed to listen\n");
        return -1;
    }
    thread t(TCPAccept);
    t.detach();
    int x =0;
    while(true)
    {
      for(int l =0;l < OpenFDS.size();l++)
      {
          x++;
          strcpy(sendBuff,":tinyirc NOTICE *:*** Checking Ident\r\n");
          write(OpenFDS[l].k,(void*)&sendBuff,strlen(sendBuff));
      }
      sleep(1);
    }
}
void TCPAccept()
{
    while(true)
    {
    OpenFDS.push_back(l(accept(sockt, (struct sockaddr*)NULL ,NULL)));
    }
}

