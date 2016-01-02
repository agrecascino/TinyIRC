#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sstream>
#include <vector>
#include <sys/types.h>
#include <thread>
using namespace std;
string remove_erase_if(string c, string delim);
struct Channel
{
    string name;
    string topic = "No topic set.";
    vector<string> users;
    int numusers = 0;
    Channel(string n)
    {
      name = n;
      name = remove_erase_if(name," \n\r");
      //raise(SIGABRT);
    }

   
};

struct User
{
    bool userisauthed = false;
    int connfd;
    bool dontkick = true;
    bool detectautisticclient = false;
    int rticks = 0;
    vector<string> channel;
    string username;
    User(int c){
        connfd = c;

    }

    ssize_t write(std::string data)
    {
        return ::write(connfd, data.c_str(), data.size());
    }
};
string remove_erase_if(string c, string delim)
{
    string output;
    for(unsigned int i = 0; i < c.size();i++)
    {
        bool pass = true;
        for(int k = 0;k < delim.size();k++)
        {
        if(c[i] == delim[k])
        {
            pass = false;
        }
        }
        if(pass)
        {
            output += c[i];
        }
    }
    return output;
}
string cleanup_chars_after_delim(string k , char delim)
{
    string output;
    for(int i = 0;i < k.size();i++)
    {
        if(k[i] == delim)
        {
            break;
        }
        output += k[i];
    }
    return output;
}

void split_string(string k , string delim, vector<string> &output)
{
    int getdelimcount = 0;
    for(unsigned int i =0;i < k.size();i++)
    {
        for(int o = 0;o < delim.size();o++)
        {
        if(k[i] == delim[o])
        {
            getdelimcount++;
        }
        }
    }
    for(int z = 0;z < k.size();z++ )
    {
        for(int i = 1;i < delim.size();i++)
        {
            if(k[z] == delim[i])
            {
                k[z] = delim[0];
            }
        }
    }
    for(int i =0;i <= getdelimcount;i++)
    {
       int delimiter = k.find(delim[0]);
          output.push_back(k.substr(0, delimiter));
          k = k.substr(delimiter + 1, k.size());
    }
}


vector<User> connections;
vector<Channel> channels;
int listenfd = 0;
struct sockaddr_in serv_addr;
void AcceptConnections();
int main(int argc,char *argv[])
{
  //Seed RNG
  srand(time(NULL));
  //get socket
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int g = 1;

  signal(SIGPIPE, SIG_IGN);
  //ignore sigpipes
  setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&g,sizeof(int));
  //force drop other applications on the same port
  //fcntl(listenfd, F_SETFL, O_NONBLOCK);
  printf("socket retrieve success\n");

  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(6667);

  bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
  
  if(listen(listenfd, 10) == -1){
      printf("Failed to listen\n");
      return -1;
  }
  //spawn second thread to accept connections, so main loop doesn't get blocked
  thread t(AcceptConnections);
  t.detach();
  //again  because we spawned a thread
  signal(SIGPIPE, SIG_IGN);
  char data[1024];
  char data2[1024];
  int datarecv;

  while(true)
    {

         for(int z = 0;z < connections.size();z++)
          {
             if(0)
             {
                 killconn:
                 close(connections[z].connfd);
                 for(int o = 0;o < connections[z].channel.size();o++)
                 {
                     for(int l = 0; l < connections.size();l++)
                     {
                       for(int u =0;u < connections[l].channel.size();u++)
                       {
                           bool sentmsg = false;
                           for(int as =0; as < connections[z].channel.size();as++)
                           {
                               if(connections[z].channel[as] == connections[l].channel[u])
                               {
                                   connections[l].write(":" + connections[z].username + " QUIT " + ":Socket killer." + "\r\n");
                                   sentmsg = true;
                                   break;
                               }
                           }
                           if(sentmsg)
                           {
                               break;
                           }
                       }
                     }
                 }
                 for(int f =0;f < channels.size();f++)
                 {
                     int channelindex = 0;
                     for(int l = 0;l < channels.size();l++)
                     {
                         string p = channels[f].name;
                         p = remove_erase_if(p," \r\n");
                         if(channels[l].name == p)
                         {
                             channelindex = l;
                             channels[channelindex].numusers--;
                         }

                     }
                     for(int o = 0;o < channels[channelindex].users.size();o++)
                     {
                       if(channels[channelindex].users[o] == connections[z].username)
                       {
                           channels[channelindex].users.erase(channels[channelindex].users.begin() + o);
                       }
                     }
                 }
                  connections.erase(connections.begin() + z);
                  continue;
                 //hack to keep variables in scope
             }
         // strcpy(sendBuff,"NOTICE AUTH:*** Ayy lmao\r\n");
          //cout << sendBuff << endl;
          memset(&data2,'0',1024);
          memset(&data,'0',1024);
          for(int i =0;i < 1024;i++)
          {
          datarecv = recv(connections[z].connfd,data,1,MSG_DONTWAIT);
          //hack to get past recv inconsistencies
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
              goto killconn;
          }

          data2[i] = data[0];
          if(data[0] == '\n')
          {
              break;
          }
          }
          

          vector<string> s;
          s.erase(s.begin(),s.end());
          string data2c = data2;
          data2c = cleanup_chars_after_delim(data2c,'\0');
          split_string(data2c," \n",s);
          //split packet by whitespace
          for(int i =0;i < s.size();i++)
          {
              if(s[i] == "USER")
              {
                 //stub incase anyone wants to implement authentication
              }

              if(s[i] == "PONG" && connections[z].userisauthed)
              {
                  //reset anti-drop
                  connections[z].dontkick = true;
                  connections[z].rticks = 0;
              }
              if(s[i] == "PONG" && !connections[z].userisauthed)
              {
                  //oh nice, you accepted our PING, welcome to the party
                  connections[z].write(":tinyirc 001 " + connections[z].username + " :Hello!" + "\r\n");
                  connections[z].write(":tinyirc 002 " + connections[z].username + " :This server is running TinyIRC pre-alpha!" + "\r\n");
                  connections[z].write(":tinyirc 003 " + connections[z].username + " :This server doesn't have date tracking." + "\r\n");
                  connections[z].write(":tinyirc 004 " + connections[z].username + " tinyirc " + " tinyirc(0.0.1) " + "CDGNRSUWagilopqrswxyz" + " BCIMNORSabcehiklmnopqstvz" + " Iabehkloqv" + "\r\n");
                  connections[z].write(":tinyirc 005 " + connections[z].username + " CALLERID CASEMAPPING=rfc1459 DEAF=D KICKLEN=180 MODES=4 PREFIX=(qaohv)~&@%+ STATUSMSG=~&@%+ EXCEPTS=e INVEX=I NICKLEN=30 NETWORK=tinyirc MAXLIST=beI:250 MAXTARGETS=4 :are supported by this server\r\n");
                  connections[z].write(":tinyirc 005 " + connections[z].username + " CHANTYPES=# CHANLIMIT=#:500 CHANNELLEN=50 TOPICLEN=390 CHANMODES=beI,k,l,BCMNORScimnpstz AWAYLEN=180 WATCH=60 NAMESX UHNAMES KNOCK ELIST=CMNTU SAFELIST :are supported by this server\r\n");
                  connections[z].write(":tinyirc 251 " + connections[z].username + " :LUSERS is unimplemented." + "\r\n");
                  connections[z].userisauthed = true;

                  connections[z].dontkick = true;
              }
              if(s[i] == "NICK")
              {
                  //if not authed, set username and PING, else set username
                  if(!connections[z].userisauthed)
                  {
                      bool killconn = false;
                  for(int k =0; k < connections.size();k++)
                  {
                      string sfisdk = string(s[i+1]).substr(0,string(s[i+1]).find("\r"));
                      if(sfisdk.size() > 225)
                      {
                          sfisdk = "FAGGOT" + to_string(rand() % 9000);
                      }
                      sfisdk = remove_erase_if(sfisdk, ".,#\n\r");
                      if(sfisdk == connections[k].username)
                      {
                          killconn = true;
                      }
                  }
                  if(killconn)
                  {
                      //CS major meme killconn == true
                      connections[z].write("NOTICE :*** Name already in use... Killing connection.\r\n");
                      connections[z].dontkick = false;
                      connections[z].rticks = 192;
                  }
                  else
                  {
                  string sfisdk = string(s[i+1]).substr(0,string(s[i+1]).find("\r"));;
                  if(sfisdk.size() > 225)
                  {
                      sfisdk = "FAGGOT" + to_string(rand() % 9000);
                  }
                  connections[z].username = sfisdk;
                  sfisdk = remove_erase_if(connections[z].username, ".,#\n\r");
                  connections[z].write("PING :" + connections[z].username + "\r\n");
                  }
                  }
                  else
                  {

                          bool inuse = false;
                      for(int k =0; k < connections.size();k++)
                      {
                          string sfisdk = string(s[i+1]).substr(0,string(s[i+1]).find("\r"));
                          if(sfisdk.size() > 225)
                          {
                          sfisdk = "FAGGOT" + to_string(rand() % 9000);
                          }
                          sfisdk = remove_erase_if(sfisdk, ".,#\n\r");
                          if(sfisdk == connections[k].username)
                          {
                              inuse = true;
                          }
                      }
                      if(!inuse)
                      {
                      string sfisdk = string(s[i+1]).substr(0,string(s[i+1]).find("\r"));
                      if(sfisdk.size() > 225)
                      {
                      sfisdk = "FAGGOT" + to_string(rand() % 9000);
                      }
                      sfisdk = remove_erase_if(sfisdk, ".,#\n\r");
                          connections[z].write(":" + connections[z].username + " NICK " + sfisdk + "\r\n");
                      }
                      else
                      {
                          connections[z].write(":tinyirc " "NOTICE :*** Name already in use..." "\r\n");
                      }
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
                      p = remove_erase_if(p," \r\n");
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
                  }
                  string buf;
                  channels[channelindex].users.push_back(connections[z].username);
                  connections[z].channel[connections[z].channel.size() -1] = remove_erase_if(connections[z].channel[connections[z].channel.size() -1], ".,\n\r");
                  connections[z].write(":"+ connections[z].username + " JOIN :" + connections[z].channel[connections[z].channel.size() - 1] + "\r\n");
                  connections[z].write(":tinyirc MODE :" + connections[z].channel[connections[z].channel.size() -1] + " +n" + "\r\n");
                  connections[z].write(":tinyirc 332 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] +  " :" + channels[channelindex].topic + "\r\n");
                  string msgf(":tinyirc 353 " + connections[z].username + " = " + connections[z].channel[connections[z].channel.size() -1] + " :" + connections[z].username);
                  for(int yf = 0;yf < channels[channelindex].users.size();yf++)
                  {
                      msgf += string(" ");
                      msgf += channels[channelindex].users[yf];
                  }
                  msgf += "\r\n";
                  connections[z].write(msgf);
                  connections[z].write(":tinyirc 366 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] + " :Sucessfully joined channel." +"\r\n");
                  channels[channelindex].numusers++;
                  for(int p = 0;p < connections.size(); p++)
                  {
                      for(int m = 0;m < connections[p].channel.size();m++)

                          if(connections[p].channel[m] == channels[channelindex].name)
                          {
                              if(p != z)
                              {
                              string ch = s[i+1];
                              ch = remove_erase_if(ch,":");
                              connections[p].write(":" + connections[z].username + " JOIN " + channels[channelindex].name + "\r\n");
                              }
                              }
                      }

                  }




              if(s[i] == "TOPIC")
              {
                 string msg = string(data2).substr(string(data2).find(":"));
                 for(int t = 0;t < channels.size();t++)
                 {
                     if(channels[t].name == s[i+1])
                     {
                         msg = msg.substr(1 ,msg.find("\r"));
                         channels[t].topic = msg;
                     }
                 }
                 string kd(":" + connections[z].username + " TOPIC " + s[i+1] + " " + msg + "\r\n");
                 for(int p = 0;p < connections.size(); p++)
                 {
                     for(int m = 0;m < connections[p].channel.size();m++)
                     {
                         if(connections[p].channel[m] == s[i+1])
                         {
                             connections[p].write(kd);
                         }
                     }

                 }


              }
              if(s[i] == "PRIVMSG")
              {
                   string msg = string(data2).substr(string(data2).find(":"),string(data2).find("\r"));

                  //send privmsg to all other users in channel
                  string buf(":" + connections[z].username + " PRIVMSG " + s[i+1] + " " + msg +"\r\n");
                  for(int p = 0;p < connections.size(); p++)
                  {
                      if(p != z)
                      {
                      for(int m = 0;m < connections[p].channel.size();m++)
                      {
                          if(connections[p].channel[m] == s[i+1])
                          {
                              connections[p].write(buf);
                          }
                      }
                      }
                  }
              }
              if(s[i] == "MODE")
              {
                  //set user mode, required for some irc clients to think you're fully connected(im looking at you xchat)
                  //+i means no messages from people outside the channel and that mode reflects how the server works
                  string buf;
                  if(connections[z].channel.size() != 0)
                  {
                      connections[z].write(":tinyirc 324 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] + " +n" + "\r\n");
                      connections[z].write(":tinyirc 329 " + connections[z].username + " " + connections[z].channel[connections[z].channel.size() -1] + " 0 0" + "\r\n");
                  }
                  else
                  {
                      connections[z].detectautisticclient = true;
                      connections[z].write(":" + connections[z].username + " MODE " + connections[z].username + " :+i" + "\r\n");
                  }
              }
              if(s[i] == "WHO")
              {
                  int channelindex = 0;
                  string p;
                  for(int l = 0;l < channels.size();l++)
                  {
                      p = s[i+1];
                      p = remove_erase_if(p," \r\n");
                      if(channels[l].name == p)
                      {
                          channelindex = l;
                      }
                  }
                  string buf(":tinyirc 352 " + connections[z].username + " " + p + " tinyirc " + connections[z].username + "\r\n");
                  connections[z].write(buf);
                  for(int h =0;h < channels[channelindex].users.size();h++)
                  {
                      connections[z].write(":tinyirc 352 " + connections[z].username + " " + p + " tinyirc " + channels[channelindex].users[h] + "\r\n");
                  }
                  connections[z].write(":tinyirc 315 " + connections[z].username + " " + channels[channelindex].name + " :End of /WHO list." + "\r\n");
              }
              if(s[i] == "QUIT")
              {
                  connections[z].dontkick = false;
                  for(int o = 0;o < connections[z].channel.size();o++)
                  {
                      for(int l = 0; l < connections.size();l++)
                      {
                        for(int u =0;u < connections[l].channel.size();u++)
                        {
                            bool sentmsg = false;
                            for(int as =0; as < connections[z].channel.size();as++)
                            {
                                if(connections[z].channel[as] == connections[l].channel[u])
                                {
                                    string kd = string(":" + connections[z].username + " QUIT " + ":Quit" + "\r\n");

                                    send(connections[l].connfd,kd.c_str(),kd.size(), MSG_NOSIGNAL);
                                    sentmsg = true;
                                    break;
                                }
                            }
                            if(sentmsg)
                            {
                                break;
                            }
                        }
                      }
                  }
                  for(int f =0;f < channels.size();f++)
                  {
                      int channelindex = 0;
                      for(int l = 0;l < channels.size();l++)
                      {
                          string p = channels[f].name;
                          p = remove_erase_if(p," \r\n");
                          if(channels[l].name == p)
                          {
                              channelindex = l;
                              channels[channelindex].numusers--;
                          }

                      }
                      for(int o = 0;o < channels[channelindex].users.size();o++)
                      {
                        if(channels[channelindex].users[o] == connections[z].username)
                        {
                            channels[channelindex].users.erase(channels[channelindex].users.begin() + o);
                        }
                      }
                  }
                  break;
              }
              if(s[i] == "PART")
              {
                  vector<string> ctol;

                  split_string(string(s[i+1]),",",ctol);

                  for(int f =0;f < ctol.size();f++)
                  {
                      int channelindex = 0;
                      for(int l = 0;l < channels.size();l++)
                      {
                          string p = ctol[f];
                          p = remove_erase_if(p," \r\n");
                          //raise(SIGABRT);
                          if(channels[l].name == p)
                          {
                              channelindex = l;
                              channels[channelindex].numusers--;
                          }

                      }
                      for(int o = 0;o < channels[channelindex].users.size();o++)
                      {
                        if(channels[channelindex].users[o] == connections[z].username)
                        {

                            channels[channelindex].users.erase(channels[channelindex].users.begin() + o);
                        }
                      }
                  }
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
                  for(int p = 0;p < connections.size(); p++)
                  {
                      for(int m = 0;m < connections[p].channel.size();m++)
                      {
                          for(int gh = 0;gh < ctol.size();gh++)
                          {
                              string kdr = string(":" + connections[z].username + " PART " + ctol[gh] + "\r\n");

                              connections[z].write(kdr);
                          if(connections[p].channel[m] == ctol[gh])
                          {

                              string ch = s[i+1];
                              ch = remove_erase_if(ch,":");
                              string kd = string(":" + connections[z].username + " PART " + ctol[gh] + "\r\n");

                              connections[p].write(kd);

                           }
                          }
                      }

                  }
              }
              if(s[i] == "PROTOCTL")
              {
                 string buf;
                  //gives capabilities of the server, some irc clients dont send one for some reason (im looking at you two irssi and weechat)
                  connections[z].write(":tinyirc 252 " + connections[z].username + " 0 :IRC Operators online" + "\r\n");
                  connections[z].write(":tinyirc 253 " + connections[z].username + " 0 :unknown connections" + "\r\n");
                  connections[z].write(":tinyirc 254 " + connections[z].username + " 0 :LUSERS is unimplmented" + "\r\n");
                  connections[z].write(":tinyirc 255 " + connections[z].username + " :LUSERS is unimplmented" + "\r\n");
                  connections[z].write(":tinyirc 265 " + connections[z].username + " 1 1 :LUSERS is unimplmented" + "\r\n");
                  connections[z].write(":tinyirc 266 " + connections[z].username + " 1 1 :LUSERS is unimplmented" + "\r\n");
                  connections[z].write(":tinyirc 375 " + connections[z].username + " 1 1 :Welcome to tinyirc pre-alpha!" + "\r\n");
                  connections[z].write(":tinyirc 372 " + connections[z].username + " :Padding call" + "\r\n");
                  connections[z].write(":tinyirc 376 " + connections[z].username + " :Ended" + "\r\n");
                  if(!connections[z].detectautisticclient)
                  {
                      connections[z].write(":" + connections[z].username + " MODE " + connections[z].username + " :+i" + "\r\n");
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
              for(int o = 0;o < connections[z].channel.size();o++)
              {
                  for(int l = 0; l < connections.size();l++)
                  {
                    for(int u =0;u < connections[l].channel.size();u++)
                    {
                        bool sentmsg = false;
                        for(int as =0; as < connections[z].channel.size();as++)
                        {
                            if(connections[z].channel[as] == connections[l].channel[u])
                            {
                                connections[l].write(":" + connections[z].username + " QUIT " + ":Ping timed out" + "\r\n");
                                sentmsg = true;
                                break;
                            }
                        }
                        if(sentmsg)
                        {
                            break;
                        }
                    }
                  }
              }
              for(int f =0;f < channels.size();f++)
              {
                  int channelindex = 0;
                  for(int l = 0;l < channels.size();l++)
                  {
                      string p = channels[f].name;
                      p = remove_erase_if(p," \r\n");
                      if(channels[l].name == p)
                      {
                          channelindex = l;
                          channels[channelindex].numusers--;
                      }

                  }
                  for(int o = 0;o < channels[channelindex].users.size();o++)
                  {
                    if(channels[channelindex].users[o] == connections[z].username)
                    {
                        channels[channelindex].users.erase(channels[channelindex].users.begin() + o);
                    }
                  }
              }
               connections.erase(connections.begin() + z);
               continue;
          }
          if(rand() % 480 == 42 && connections[z].userisauthed)
          {
          
          cout << "Hit random" << endl;
          string buf;
          buf ="PING :" + connections[z].username + "\r\n";
          send(connections[z].connfd,buf.c_str(),buf.size(),MSG_NOSIGNAL);
          connections[z].dontkick = false;
          }
          cout << connections[z].username << endl;
         // cout << "Proccessing connections" << endl;
         this_thread::sleep_for(chrono::milliseconds(50));
         }
      }




  return 0;
}
void AcceptConnections()
{
    while(1)
    {
        int connfd = accept(listenfd, nullptr, nullptr);
        if(connfd != -1)
            connections.push_back(User(connfd)); // accept awaiting request
    }
}
