#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <boost/algorithm/string.hpp>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <iostream>
#include <vector>
#include <sys/types.h>
#include <thread>
using namespace std;
struct User
{
    User(int l,int c, string n){
        c = connfd;
        l = listenfd;
    }
    int listenfd;
    int connfd;

    string channel;
    string username;
};
vector<User> connections;
int listenfd = 0,connfd = 0;
char sendBuff[1025];
  int connc = 0;
  struct sockaddr_in serv_addr;
 // struct sock_addr_storage remoteaddr;
  void AcceptConnections();
  fd_set users;
  int fdmax;
  fd_set selector;
int main(void)
{





  int numrv;
  srand(time(NULL));
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int g = 1;
  FD_ZERO(&users);
  FD_ZERO(&selector);
  setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&g,sizeof(int));
  //fcntl(listenfd, F_SETFL, O_NONBLOCK);
  printf("socket retrieve success\n");

  memset(&serv_addr, '0', sizeof(serv_addr));
  memset(sendBuff, '0', sizeof(sendBuff));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(6667);

  bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));

  if(listen(listenfd, 10) == -1){
      printf("Failed to listen\n");
      return -1;
  }
  FD_SET(listenfd,&users);
  fdmax = listenfd;
  thread t(AcceptConnections);
  t.detach();


  char data[1024];
  char data2[1024];
  int datarecv;
  bool kicker = false;
  bool dontkick = true;
  bool intpong = false;
  string username;
  string sessionid;
  bool userisauthed = false;
  string channel;
  while(1)
    {



         // strcpy(sendBuff,"NOTICE AUTH:*** Ayy lmao\r\n");
          //cout << sendBuff << endl;
          memset(&data2,'0',1024);
          memset(&data,'0',1024);
          for(int i =0;i < 1024;i++)
          {
          datarecv = recv(connfd,data,1,MSG_DONTWAIT);
          //cout << data[0] << endl;
          cout << datarecv << endl;
          if(data[0] == NULL)
          {
              break;
          }
          if(datarecv == -1)
          {
              break;
          }

          data2[i] = data[0];
          }
          vector<string> s;
          s.erase(s.begin(),s.end());
          boost::algorithm::split(s,data2,boost::is_any_of(" \n"),boost::token_compress_on);
          for(int i =0;i < s.size();i++)
          {

              if(s[i] == "USER")
              {

              }
              if(s[i] == "PONG" && userisauthed && kicker)
              {
                  dontkick = true;
              }
              if(s[i] == "PONG" && !userisauthed)
              {

                  strcpy(sendBuff,string(":tinyirc 001 " + username + " :Hello!" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 002 " + username + " :This server is running TinyIRC pre-alpha!" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 003 " + username + " :This server doesn't have date tracking." + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 004 " + username + " tinyirc " + " tinyirc(0.0.1) " + "CDGNRSUWagilopqrswxyz" + " BCIMNORSabcehiklmnopqstvz" + " Iabehkloqv" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 005 " + username + " CALLERID CASEMAPPING=rfc1459 DEAF=D KICKLEN=180 MODES=4 PREFIX=(qaohv)~&@%+ STATUSMSG=~&@%+ EXCEPTS=e INVEX=I NICKLEN=30 NETWORK=tinyirc MAXLIST=beI:250 MAXTARGETS=4 :are supported by this server\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 005 " + username + " CHANTYPES=# CHANLIMIT=#:500 CHANNELLEN=50 TOPICLEN=390 CHANMODES=beI,k,l,BCMNORScimnpstz AWAYLEN=180 WATCH=60 NAMESX UHNAMES KNOCK ELIST=CMNTU SAFELIST :are supported by this server\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 251 " + username + " :LUSERS is unimplemented." + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  userisauthed = true;

                  dontkick = true;
              }
              if(s[i] == "NICK")
                  username = s[i+1];
                  boost::remove_erase_if(username, boost::is_any_of(".,#\n\r"));
                  strcpy(sendBuff,string("PING :" + username + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));

              }
              if(s[i] == "JOIN")
              {
                  channel = s[i+1];

                  boost::remove_erase_if(channel, boost::is_any_of(".,\n\r"));
                  strcpy(sendBuff,string(":"+ username + " JOIN :" + channel + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc MODE :" + channel + " +n" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  //strcpy(sendBuff,string(":tinyirc 332 " + username + " " + channel + " :Test" + "\r\n").c_str());
                  //write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 353 " + username + " = " + channel + " :@" + username + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 366 " + username + " " + channel + " :Sucessfully joined channel." +"\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));



              }
              if(s[i] == "PRIVMSG")
              {
                  //send privmsg to all other users in channel

              }
              if(s[i] == "MODE")
              {
                  strcpy(sendBuff,string(":tinyirc 324 " + username + " " + channel + " +n" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 329 " + username + " " + channel + " 0 0" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
              }
              if(s[i] == "WHO")
              {
                  strcpy(sendBuff,string(":tinyirc 352 " + username + " " + channel + " 0 0" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 315 " + username + " " + channel + " :End of /WHO list." + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
              }
              if(s[i] == "PROTOCTL")
              {


                  strcpy(sendBuff,string(":tinyirc 252 " + username + " 0 :IRC Operators online" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 253 " + username + " 0 :unknown connections" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 254 " + username + " 0 :LUSERS is unimplmented" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 255 " + username + " :LUSERS is unimplmented" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 265 " + username + " 1 1 :LUSERS is unimplmented" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 266 " + username + " 1 1 :LUSERS is unimplmented" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 375 " + username + " 1 1 :Welcome to tinyirc pre-alpha!" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 372 " + username + " :Padding call" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 376 " + username + " :Ended" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":" + username + " MODE " + username + " :+i" + "\r\n").c_str());
                  write(connfd,(void*)&sendBuff,strlen(sendBuff));
              }
          }
          if(dontkick == false)
          {
              close(connfd);
              dontkick = true;
          }
          if(rand() % 60 == 42)
          {
          strcpy(sendBuff,string("PING :" + username + "\r\n").c_str());
          write(connfd,(void*)&sendBuff,strlen(sendBuff));
          kicker = true;
          dontkick = false;
          }
          cout << username << endl;
         // cout << "Proccessing connections" << endl;
        }

          //write(connfd,(void*)&sendBuff,strlen(sendBuff));


      sleep(1);
    }


  return 0;
}
void AcceptConnections()
{
    while(1)
    {


    connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL); // accept awaiting request

    strcpy(sendBuff,":tinyirc NOTICE * :*** Checking Ident\r\n");
    write(connfd,(void*)&sendBuff,strlen(sendBuff));



    //connections.push_back(User(connfd,"Anon"));
    }
}
