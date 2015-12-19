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
#include <chrono>
#include <signal.h>
#include <boost/algorithm/string.hpp>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <sys/types.h>
#include <thread>
using namespace std;
struct Channel
{
    Channel(string n)
    {
      name = n;
      boost::remove_erase_if(name,boost::is_any_of(" \n\r"));
    }

    string name;
    string topic = "No topic set.";
    vector<string> users;
};

struct User
{
    User(int c){
        connfd = c;

    }
    bool userisauthed = false;
    int connfd;
    bool kicker = false;
    bool dontkick = true;
    bool detectautisticclient = false;
    int rticks = 0;
    vector<string> channel;
    string username;
};
vector<User> connections;
vector<Channel> channels;
int listenfd = 0,connfd = 0;
char sendBuff[1025];
  int connc = 0;
  struct sockaddr_in serv_addr;
  void AcceptConnections();
int main(void)
{





  int numrv;
  srand(time(NULL));
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int g = 1;

  signal(SIGPIPE, SIG_IGN);
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

  thread t(AcceptConnections);
  t.detach();


  char data[1024];
  char data2[1024];
  int datarecv;

  bool piperip = false;



  while(1)
    {

         for(int z = 0;z < connections.size();z++)
          {
         // strcpy(sendBuff,"NOTICE AUTH:*** Ayy lmao\r\n");
          //cout << sendBuff << endl;
          memset(&data2,'0',1024);
          memset(&data,'0',1024);
          for(int i =0;i < 1024;i++)
          {
          datarecv = recv(connections[z].connfd,data,1,MSG_DONTWAIT);
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

          if(datarecv == 0)
          {
              break;
          }

          data2[i] = data[0];
          if(data[0] == '\n')
          {
              break;
          }
          }
          if(piperip)
          {
              piperip = false;
              connections.erase(connections.begin() + z);
              continue;
          }
          vector<string> s;
          s.erase(s.begin(),s.end());
          boost::algorithm::split(s,data2,boost::is_any_of(" \n"),boost::token_compress_on);
          for(int i =0;i < s.size();i++)
          {
              if(s[i] == "USER")
              {

              }

              if(s[i] == "PONG" && connections[z].userisauthed && connections[z].kicker)
              {
                  connections[z].dontkick = true;
                  connections[z].rticks = 0;
              }
              if(s[i] == "PONG" && !connections[z].userisauthed)
              {

                  strcpy(sendBuff,string(":tinyirc 001 " + connections[z].username + " :Hello!" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 002 " + connections[z].username + " :This server is running TinyIRC pre-alpha!" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 003 " + connections[z].username + " :This server doesn't have date tracking." + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 004 " + connections[z].username + " tinyirc " + " tinyirc(0.0.1) " + "CDGNRSUWagilopqrswxyz" + " BCIMNORSabcehiklmnopqstvz" + " Iabehkloqv" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 005 " + connections[z].username + " CALLERID CASEMAPPING=rfc1459 DEAF=D KICKLEN=180 MODES=4 PREFIX=(qaohv)~&@%+ STATUSMSG=~&@%+ EXCEPTS=e INVEX=I NICKLEN=30 NETWORK=tinyirc MAXLIST=beI:250 MAXTARGETS=4 :are supported by this server\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 005 " + connections[z].username + " CHANTYPES=# CHANLIMIT=#:500 CHANNELLEN=50 TOPICLEN=390 CHANMODES=beI,k,l,BCMNORScimnpstz AWAYLEN=180 WATCH=60 NAMESX UHNAMES KNOCK ELIST=CMNTU SAFELIST :are supported by this server\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 251 " + connections[z].username + " :LUSERS is unimplemented." + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  connections[z].userisauthed = true;

                  connections[z].dontkick = true;
              }
              if(s[i] == "NICK")
              {

                  if(!connections[z].userisauthed)
                  {
                  connections[z].username = s[i+1];
                  boost::remove_erase_if(connections[z].username, boost::is_any_of(".,#\n\r"));
                  strcpy(sendBuff,string("PING :" + connections[z].username + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  }
                  else
                  {
                      strcpy(sendBuff,string(":" + connections[z].username + " NICK " + s[i+1] + "\r\n").c_str());
                      write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                      connections[z].username = s[i+1];
                  }

              }
              if(s[i] == "JOIN")
              {
                  connections[z].channel.push_back(s[i+1]);
                  bool channelexists = false;
                  int channelindex = 0;
                  for(int l = 0;l < channels.size();l++)
                  {
                      string p = s[i+1];
                      boost::remove_erase_if(p,boost::is_any_of(" \r\n"));
                      if(channels[l].name == p)
                      {
                          channelindex = l;
                          channelexists = true;
                      }
                  }
                  if(!channelexists)
                  {
                      channels.push_back(Channel(s[i+1]));
                      channelindex = channels.size() - 1;
                      this_thread::sleep_for(chrono::seconds(2));
                  }
                  channels[channelindex].users.push_back(connections[z].username);
                  boost::remove_erase_if(connections[z].channel[connections[z].channel.size() -1], boost::is_any_of(".,\n\r"));
                  strcpy(sendBuff,string(":"+ connections[z].username + " JOIN :" + connections[z].channel[connections[z].channel.size() - 1] + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc MODE :" + connections[z].channel[connections[z].channel.size() -1] + " +n" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 332 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] +  " :" + channels[channelindex].topic + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 353 " + connections[z].username + " = " + connections[z].channel[connections[z].channel.size() -1] + " :@" + connections[z].username + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 366 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] + " :Sucessfully joined channel." +"\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));



              }
              if(s[i] == "TOPIC")
              {
                 string msg = string(data2).substr(string(data2).find(":"));
                 for(int t = 0;t < channels.size();t++)
                 {
                     if(channels[t].name == s[i+1])
                     {
                         channels[t].topic = msg;
                     }
                 }
                 for(int p = 0;p < connections.size(); p++)
                 {
                     for(int m = 0;m < connections[p].channel.size();m++)
                     {
                         if(connections[p].channel[m] == s[i+1])
                         {
                             string kd = string(":" + connections[z].username + " TOPIC " + s[i+1] + " " + msg +"\r\n");

                             write(connections[p].connfd,kd.c_str(),kd.size());
                         }
                     }

                 }


              }
              if(s[i] == "PRIVMSG")
              {
                   string msg = string(data2).substr(string(data2).find(":"));

                  //send privmsg to all other users in channel
                  for(int p = 0;p < connections.size(); p++)
                  {
                      if(p != z)
                      {
                      for(int m = 0;m < connections[p].channel.size();m++)
                      {
                          if(connections[p].channel[m] == s[i+1])
                          {

                              strcpy(sendBuff,string(":" + connections[z].username + " PRIVMSG " + s[i+1] + " " + msg +"\r\n").c_str());
                              write(connections[p].connfd,sendBuff,strlen(sendBuff));
                          }
                      }
                      }
                  }
              }
              if(s[i] == "MODE")
              {
                  if(connections[z].channel.size() != 0)
                  {
                  strcpy(sendBuff,string(":tinyirc 324 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] + " +n" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 329 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] + " 0 0" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  }
                  else
                  {
                      connections[z].detectautisticclient = true;
                      strcpy(sendBuff,string(":" + connections[z].username + " MODE " + connections[z].username + " :+i" + "\r\n").c_str());
                      write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  }
              }
              if(s[i] == "WHO")
              {
                  strcpy(sendBuff,string(":tinyirc 352 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] + " 0 0" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 315 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] + " :End of /WHO list." + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
              }
              if(s[i] == "QUIT")
              {
                  connections[z].dontkick = false;
                  break;
              }
              if(s[i] == "PART")
              {
                  vector<string> ctol;

                  boost::algorithm::split(ctol,s[i+1],boost::is_any_of(","),boost::token_compress_on);
                  for(int f =0;f < ctol.size();f++)
                  {
                      for(int q =0;q < connections[z].channel.size();q++)
                      {
                          if(connections[z].channel[q] == ctol[f])
                          {
                              connections[z].channel.erase(connections[z].channel.begin() + q);
                          }
                      }
                  }
              }
              if(s[i] == "PROTOCTL")
              {


                  strcpy(sendBuff,string(":tinyirc 252 " + connections[z].username + " 0 :IRC Operators online" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 253 " + connections[z].username + " 0 :unknown connections" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 254 " + connections[z].username + " 0 :LUSERS is unimplmented" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 255 " + connections[z].username + " :LUSERS is unimplmented" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 265 " + connections[z].username + " 1 1 :LUSERS is unimplmented" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 266 " + connections[z].username + " 1 1 :LUSERS is unimplmented" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 375 " + connections[z].username + " 1 1 :Welcome to tinyirc pre-alpha!" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 372 " + connections[z].username + " :Padding call" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  strcpy(sendBuff,string(":tinyirc 376 " + connections[z].username + " :Ended" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  if(!connections[z].detectautisticclient)
                  {
                  strcpy(sendBuff,string(":" + connections[z].username + " MODE " + connections[z].username + " :+i" + "\r\n").c_str());
                  write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));
                  }
              }
          }
          if(!connections[z].dontkick)
          {
              connections[z].rticks++;
          }
          if(!connections[z].dontkick && connections[z].rticks == 192)
          {
              close(connections[z].connfd);
               connections.erase(connections.begin() + z);
               continue;
          }
          if(rand() % 480 == 42 && connections[z].userisauthed)
          {
          cout << "Hit random" << endl;
          strcpy(sendBuff,string("PING :" + connections[z].username + "\r\n").c_str());
          send(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff),MSG_NOSIGNAL);
          connections[z].kicker = true;
          connections[z].dontkick = false;
          }
          cout << connections[z].username << endl;
         // cout << "Proccessing connections" << endl;
         this_thread::sleep_for(chrono::milliseconds(50));
          //write(connections[z].connfd,(void*)&sendBuff,strlen(sendBuff));

         }
      }




  return 0;
}
void AcceptConnections()
{
    while(1)
    {


    connections.push_back(User(accept(listenfd, (struct sockaddr*)NULL ,NULL))); // accept awaiting request
    if(connfd != -1)
    {
        connc++;
    }





    }
}
