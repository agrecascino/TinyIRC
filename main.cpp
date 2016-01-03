#include <iostream>
#include <mutex>
#include <set>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <sstream>
#include <vector>
#include <thread>
extern "C" {
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <err.h>
}
using namespace std;
string remove_erase_if(string c, string delim);

void ControlServer();

struct User;
struct Channel
{
    string const name;
    string topic = "No topic set.";
    set<string> users;

    Channel(string name) : name(name) {}

    void remove_user(User& user);
    void notify_part(User &user, string const& reason);
};

struct User
{
    bool userisauthed = false;
    int connfd;
    bool kickedbyop = false;
    bool dontkick = true;
    bool detectautisticclient = false;
    int rticks = 0;
    vector<string> channel;
    string username;
    User(int c) : connfd(c) {}

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

void split_string(string const &k, string const &delim, vector<string> &output)
{
    // Due to the use of strpbrk here, this will stop copying at a null char. This is in fact desirable.
    char const *last_ptr = k.c_str(), *next_ptr;
    while ((next_ptr = strpbrk(last_ptr, delim.c_str())) != nullptr)
    {
        output.emplace_back(last_ptr, next_ptr - last_ptr);
        last_ptr = next_ptr + 1;
    }
    output.emplace_back(last_ptr);
}

typedef lock_guard<mutex> mutex_guard;
mutex connections_mutex; // XXX SLOW HACK to stop the server dying randomly
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
  if (listenfd == -1)
      err(1, "socket");
  int g = 1;

  signal(SIGPIPE, SIG_IGN);
  //ignore sigpipes
  setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&g,sizeof(int));
  //force drop other applications on the same port
  //fcntl(listenfd, F_SETFL, O_NONBLOCK);

  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(6667);

  if (bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1)
      err(1, "bind to port %d", ntohs(serv_addr.sin_port));
  
  if(listen(listenfd, 10) == -1){
      printf("Failed to listen\n");
      return -1;
  }
  //spawn second thread to accept connections, so main loop doesn't get blocked
  thread t(AcceptConnections);
  t.detach();
  thread z(ControlServer);
  z.detach();
  //again  because we spawned a thread
  signal(SIGPIPE, SIG_IGN);
  char data[1024];
  char data2[1024];
  int datarecv;

  while(true)
  {
    {
         mutex_guard lock(connections_mutex);

         for(int z = 0;z < connections.size();z++)
          {
             if(0)
             {
                 killconn:
                 close(connections[z].connfd);
                 for (User &observer : connections)
                 {
                     for (string const &observerchan : observer.channel)
                     {
                         bool sentmsg = false;
                         for (string const &disconnecterchan : connections[z].channel)
                         {
                             if(disconnecterchan == observerchan)
                             {
                                 observer.write(":" + connections[z].username + " QUIT " + (connections[z].kickedbyop ? string(":Kicked by OP") : string(":Socket killer.") ) + "\r\n");
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
                 for (string const &userchannel : connections[z].channel)
                 {
                     auto channeliter = std::find_if(channels.begin(), channels.end(),
                         [&](Channel const& c) { return c.name == userchannel; }
                     );
                     // Assume channel exists. Otherwise, we fucked up elsewhere and will break here.
                     channeliter->remove_user(connections[z]);
                 }
                  connections.erase(connections.begin() + z);
                  continue;
                 //hack to keep variables in scope
             }
         // strcpy(sendBuff,"NOTICE AUTH:*** Ayy lmao\r\n");
          //cout << sendBuff << endl;
          memset(&data2, 0, sizeof data2);
          memset(&data, 0, sizeof data);
          for(int i =0;i < sizeof data2;i++)
          {
          datarecv = recv(connections[z].connfd,data,1,MSG_DONTWAIT);
          //hack to get past recv inconsistencies
          //cout << data[0] << endl;
          //cout << datarecv << endl;

          if(datarecv == -1)
          {
              if (errno != EAGAIN && errno != EWOULDBLOCK)
              {
                  goto killconn;
              }
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
          if(connections[z].kickedbyop)
          {
              goto killconn;
          }

          vector<string> s;
          s.erase(s.begin(),s.end());
          string data2c = data2;
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
                          sfisdk = "FAGGOT" + to_string(rand() % 9000);
                          sfisdk = remove_erase_if(sfisdk, ".,#\n\r");
                          if(sfisdk == connections[k].username)
                              inuse = true;
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
                  string channame = s[i+1];
                  // TODO handle comma separated chan joins?
                  channame = remove_erase_if(channame,":,. \r\n");
                  connections[z].channel.push_back(channame);
                  bool channelexists = false;
                  int channelindex = 0;
                  for(int l = 0;l < channels.size();l++)
                  {
                      if(channels[l].name == channame)
                      {
                          channelindex = l;
                          channelexists = true;
                      }
                  }
                  if(!channelexists)
                  {
                      channels.push_back(Channel(channame));
                      channelindex = channels.size() - 1;
                  }
                  string buf;
                  channels[channelindex].users.insert(connections[z].username);
                  connections[z].write(":"+ connections[z].username + " JOIN :" + channame + "\r\n");
                  connections[z].write(":tinyirc MODE :" + channame + " +n" + "\r\n");
                  connections[z].write(":tinyirc 332 " + connections[z].username + " " + channame +  " :" + channels[channelindex].topic + "\r\n");
                  string msgf(":tinyirc 353 " + connections[z].username + " = " + channame + " :" + connections[z].username);
                  for(string const &user : channels[channelindex].users)
                  {
                      msgf += " ";
                      msgf += user;
                  }
                  msgf += "\r\n";
                  connections[z].write(msgf);
                  connections[z].write(":tinyirc 366 " + connections[z].username + " " + channame + " :Sucessfully joined channel." +"\r\n");
                  for (User &observer : connections)
                  {
                      if (&observer == &connections[z]) continue;

                      for (string const &channel : observer.channel)
                          if(channel == channels[channelindex].name)
                              observer.write(":" + connections[z].username + " JOIN " + channels[channelindex].name + "\r\n");
                  }
              }

              if(s[i] == "TOPIC")
              {
                 string msg = string(data2).substr(string(data2).find(":"));
                 for(int t = 0;t < channels.size();t++)
                     if(channels[t].name == s[i+1])
                     {
                         msg = msg.substr(1 ,msg.find("\r"));
                         channels[t].topic = msg;
                     }
                 string kd(":" + connections[z].username + " TOPIC " + s[i+1] + " " + msg + "\r\n");
                 for (User &observer : connections)
                     for (string const &channel : observer.channel)
                         if(channel == s[i+1])
                             observer.write(kd);
              }
              if(s[i] == "PRIVMSG")
              {
                   string msg = string(data2).substr(string(data2).find(":"),string(data2).find("\r"));
                  //send privmsg to all other users in channel
                  // TODO: user-to-user private messaging
                  // TODO: warning if channel/user doesn't exist
                  string buf(":" + connections[z].username + " PRIVMSG " + s[i+1] + " " + msg +"\r\n");
                  for (User &observer : connections)
                      if(&observer != &connections[z])
                          for (string const &channel : observer.channel)
                              if(channel == s[i+1])
                                  observer.write(buf);
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
                          channelindex = l;
                  }
                  string buf(":tinyirc 352 " + connections[z].username + " " + p + " tinyirc " + connections[z].username + "\r\n");
                  connections[z].write(buf);
                  for(string const &chanuser : channels[channelindex].users)
                  {
                      connections[z].write(":tinyirc 352 " + connections[z].username + " " + p + " tinyirc " + chanuser + "\r\n");
                  }
                  connections[z].write(":tinyirc 315 " + connections[z].username + " " + channels[channelindex].name + " :End of /WHO list." + "\r\n");
              }
              if(s[i] == "QUIT")
              {
                  connections[z].dontkick = false;
                  for (User &observer : connections)
                    for (string const &observerchan : observer.channel)
                    {
                        bool sentmsg = false;
                        for (string const &quitterchan : connections[z].channel)
                            if(quitterchan == observerchan)
                            {
                                string kd = string(":" + connections[z].username + " QUIT " + ":Quit" + "\r\n");

                                send(observer.connfd,kd.c_str(),kd.size(), MSG_NOSIGNAL);
                                sentmsg = true;
                                break;
                            }
                        if(sentmsg) break;
                    }
                  for (string const &userchannel : connections[z].channel)
                  {
                      auto channeliter = std::find_if(channels.begin(), channels.end(),
                          [&](Channel const& c) { return c.name == userchannel; }
                      );
                      // Assume channel exists. Otherwise, we fucked up elsewhere and will break here.
                      channeliter->remove_user(connections[z]);
                  }
                  connections.erase(connections.begin() + z);
                  break;
              }
              if(s[i] == "PART")
              {
                  vector<string> ctol;
                  split_string(string(s[i+1]),",",ctol);

                  for (string &chantopart : ctol)
                  {
                      chantopart = remove_erase_if(chantopart," \r\n");
                      auto channeliter = std::find_if(channels.begin(), channels.end(),
                          [&](Channel const& c) { return c.name == chantopart; }
                      );

                      if (channeliter == channels.end())
                          continue; // Don't part a channel that doesn't exist

                      channeliter->remove_user(connections[z]);
                      channeliter->notify_part(connections[z], "Leaving"); // TODO: use client's reason
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
                      connections[z].write(":" + connections[z].username + " MODE " + connections[z].username + " :+i" + "\r\n");
              }
          }
          if(!connections[z].dontkick)
              connections[z].rticks++;
          if(!connections[z].dontkick && connections[z].rticks == 192)
          {
              close(connections[z].connfd);
              for (User &observer : connections)
              {
                for (string const &observerchan : observer.channel)
                {
                    bool sentmsg = false;
                    for (string const &kickedchan : connections[z].channel)
                        if(kickedchan == observerchan)
                        {
                            observer.write(":" + connections[z].username + " QUIT " + ":Ping timed out" + "\r\n");
                            sentmsg = true;
                            break;
                        }
                    if(sentmsg)
                        break;
                }
              }
              for (Channel &channel : channels)
                  channel.remove_user(connections[z]);
              connections.erase(connections.begin() + z);
              continue;
          }
          if(rand() % 480 == 42 && connections[z].userisauthed)
          {
              string buf;
              buf ="PING :" + connections[z].username + "\r\n";
              send(connections[z].connfd,buf.c_str(),buf.size(),MSG_NOSIGNAL);
              connections[z].dontkick = false;
          }
        }
    }
    this_thread::sleep_for(chrono::milliseconds(50));
  }
  return 0;
}
void AcceptConnections()
{
    while(1)
    {
        int connfd = accept(listenfd, nullptr, nullptr);
        if(connfd != -1)
        {
            mutex_guard lock(connections_mutex);
            connections.push_back(User(connfd)); // accept awaiting request
        }
    }
}

void Channel::remove_user(User& user)
{
    // Does NOT notify any clients that the user is removed!
    // Use notify_part for that (or send out quits)
    users.erase(user.username);

    auto userchannels = user.channel;
    std::remove(userchannels.begin(), userchannels.end(), name);
}

void Channel::notify_part(User &user, string const& reason)
{
    string partmsg = string(":" + user.username + " PART " + name + " " + reason + "\r\n");
    user.write(partmsg);

    // TODO pending rewrite when sensible data structures are used
    for (User &connection : connections)
        for (string const &channel : connection.channel)
            if (channel == name)
                connection.write(partmsg);
}

void ControlServer()
{
    string action;
    vector<string> cmd;
    while(true)
    {
    cmd.erase(cmd.begin(),cmd.end());
    getline(cin,action);
    split_string(action," ",cmd);
    for(int z = 0;z < cmd.size();z++)
    {
    if(cmd[z] == "list")
    {
        mutex_guard lock(connections_mutex);
        for(int i = 0;i < connections.size();i++)
            cout << "User: " << connections[i].username << endl;
    }
    if(cmd[z] == "kick")
    {
        mutex_guard lock(connections_mutex);
        string nametest = string(cmd[z+1]).substr(0,string(cmd[z+1]).find("\n"));;
        for(int o = 0;o < connections.size();o++)
        {

            if(connections[o].username == nametest)
            {
                connections[o].kickedbyop = true;
                break;
            }
        }
    }
    }
    }
}
