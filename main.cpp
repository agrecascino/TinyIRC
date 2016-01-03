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
    void broadcast(string const &message);
};

struct User
{
    bool userisauthed = false;
    int connfd;
    bool dontkick = true;
    bool detectautisticclient = false;
    int rticks = 0;
    set<string> channel;
    string username = "<connecting>";
    User(int c) : connfd(c) {}

    ssize_t write(std::string data)
    {
        return ::write(connfd, data.c_str(), data.size());
    }

    void kill(string const &reason);
    void broadcast(string const &message);
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
                  connections[z--].kill("Connection error");
                  continue;
              }
              break;
          }

          if(datarecv == 0)
          {
              connections[z--].kill("Connection error");
              continue;
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
          split_string(data2c," \n",s);
          //split packet by whitespace
          for(int i =0;i < s.size();i++)
          {
              if(s[i] == "USER")
              {
                 //stub incase anyone wants to implement authentication
                 break;
              }
              if(s[i] == "PING")
              {
                  string cheese = remove_erase_if(s[i+1],"\r\n ");
                  connections[z].write(":tinyirc PONG " + cheese + "\r\n");
                  break;
              }

              if(s[i] == "PONG" && connections[z].userisauthed)
              {
                  //reset anti-drop
                  connections[z].dontkick = true;
                  connections[z].rticks = 0;
                  break;
              }
              if(s[i] == "PONG" && !connections[z].userisauthed)
              {
                  //oh nice, you accepted our PING, welcome to the party
                  if(connections[z].username.size() > 0)
                  {
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
                  break;
              }
              if(s[i] == "NICK")
              {
                  string new_nick = string(s[i+1]).substr(0,string(s[i+1]).find("\r"));
                  new_nick = remove_erase_if(new_nick, ".,#\n\r");
                  if(new_nick.size() > 225 || new_nick.size() == 0)
                      new_nick = "FAGGOT" + to_string(rand() % 9000);

                  bool inuse = false;
                  for(int k =0; k < connections.size();k++)
                      if(new_nick == connections[k].username)
                          inuse = true;

                  //if not authed, set username and PING, else set username
                  if(!connections[z].userisauthed)
                  {
                      connections[z].username = new_nick;
                      if(inuse)
                      {
                          connections[z--].kill("Nick already in use");
                          continue;
                      }
                      connections[z].write("PING :" + connections[z].username + "\r\n");
                  }
                  else
                  {
                      if(inuse)
                          connections[z].write(":tinyirc " "NOTICE :*** Name already in use..." "\r\n");
                      else
                      {
                          connections[z].broadcast(":" + connections[z].username + " NICK " + new_nick + "\r\n");
                          connections[z].username = new_nick;
                      }
                  }
                  break;
              }
              if(s[i] == "JOIN")
              {
                  string channame = s[i+1];
                  // TODO handle comma separated chan joins?
                  channame = remove_erase_if(channame,":,. \r\n");
                  connections[z].channel.insert(channame);
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
                  channels[channelindex].broadcast(":" + connections[z].username + " JOIN " + channame + "\r\n");
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
                  break;
              }

              if(s[i] == "TOPIC")
              {
                 string msg = string(data2).substr(string(data2).find(":"));
                 for(int t = 0;t < channels.size();t++)
                     if(channels[t].name == s[i+1])
                     {
                         msg = msg.substr(1, msg.find("\r") - 1);
                         channels[t].topic = msg;
                         channels[t].broadcast(":" + connections[z].username + " TOPIC " + s[i+1] + " " + msg + "\r\n");
                         break;
                     }
                 break;
              }
              if(s[i] == "PRIVMSG")
              {
                  string msg = string(data2).substr(string(data2).find(":"));
                  msg = remove_erase_if(msg, "\r\n");
                  //send privmsg to all other users in channel
                  // TODO: warning if channel/user doesn't exist
                  string recip = s[i+1];
                  if (recip.size() == 0) break;

                  string buf(":" + connections[z].username + " PRIVMSG " + recip + " " + msg + "\r\n");
                  if (recip[0] == '#')
                  {
                      for (User &observer : connections)
                          if(&observer != &connections[z])
                              if (observer.channel.find(s[i+1]) != observer.channel.end())
                                  observer.write(buf);
                  }
                  else
                  {
                      for (User &user : connections)
                          if (user.username == recip)
                          {
                              user.write(buf);
                              break;
                          }
                  }
                  break;
              }
              if(s[i] == "MODE")
              {
                  //set user mode, required for some irc clients to think you're fully connected(im looking at you xchat)
                  //+i means no messages from people outside the channel and that mode reflects how the server works
                  string buf;
                  string channel = remove_erase_if(s[i+1],"\n\r ");
                  if(connections[z].channel.size() != 0)
                  {
                      connections[z].write(":tinyirc 324 " + connections[z].username + " " + channel + " +n" + "\r\n");
                      connections[z].write(":tinyirc 329 " + connections[z].username + " " + channel + " 0 0" + "\r\n");
                  }
                  else
                  {
                      connections[z].detectautisticclient = true;
                      connections[z].write(":" + connections[z].username + " MODE " + connections[z].username + " :+i" + "\r\n");
                  }
                   break;
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
                   break;
              }
              if(s[i] == "QUIT")
              {
                  connections[z--].kill("Quit");
                  continue;
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

                      channeliter->notify_part(connections[z], "Leaving"); // TODO: use client's reason
                      channeliter->remove_user(connections[z]);
                  }
                  break;
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
               break;
          }
          if(!connections[z].dontkick)
              connections[z].rticks++;
          if(!connections[z].dontkick && connections[z].rticks == 192)
          {
              connections[z--].kill("Ping timed out");
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

    user.channel.erase(name);
}

void Channel::notify_part(User &user, string const& reason)
{
    broadcast(":" + user.username + " PART " + name + " " + reason + "\r\n");
}

void ControlServer()
{
    string action;
    vector<string> cmd;
    while(true)
    {
        cmd.erase(cmd.begin(),cmd.end());
        getline(cin,action);
        if (*(action.end() - 1) == '\n')
            action.erase(action.end() - 1, action.end());
        split_string(action," ",cmd);
        if(cmd[0] == "list")
        {
            mutex_guard lock(connections_mutex);
            for(int i = 0;i < connections.size();i++)
                cout << "User: " << connections[i].username << endl;
        }
        else if(cmd[0] == "kick")
        {
            if (cmd.size() < 2)
            {
                cout << "Need a username to kick" << endl;
                continue;
            }
            mutex_guard lock(connections_mutex);
            string name = cmd[1];
            auto userit = std::find_if(connections.begin(), connections.end(),
                [&](User const &u) { return u.username == name; }
            );
            if (userit != connections.end())
            {
                userit->kill("Kicked by OP");
                cout << "Kicked \"" << name << "\"!" << endl;
            }
            else
                cout << "Couldn't find a user called \"" << name << "\"" << endl;
        }
        else if (cmd[0] == "say")
        {
            mutex_guard lock(connections_mutex);
            size_t space_pos = action.find(' ');
            if (space_pos != string::npos)
            {
                action.erase(0, space_pos + 1);
                for (User &user : connections)
                    user.write(":tinyirc NOTICE " + user.username + " :" + action + "\r\n");
            }
            cout << "Broadcast sent to " << connections.size() << " users." << endl;
        }
        else if (action.size() != 0)
        {
            cout << "Unknown command \"" << cmd[0] << "\"" << endl;
        }
    }
}

void User::kill(string const &reason)
{
    string const quitbroadcast = ":" + username + " QUIT :" + reason + "\r\n";
    broadcast(quitbroadcast);
    close(connfd);
    for (string const &userchannel : channel)
    {
        auto channeliter = std::find_if(channels.begin(), channels.end(),
                [&](Channel const& c) { return c.name == userchannel; }
                );
        // Assume channel exists. Otherwise, we fucked up elsewhere and will break here.
        channeliter->remove_user(*this);
    }
    connections.erase(std::remove_if(connections.begin(), connections.end(),
            [&](User const &x) { return &x == this; }
        ), connections.end());
}

void Channel::broadcast(string const &message)
{
    // TODO: Get a map, do lookup the opposite way around. Needs rethinking the vector of users.
    for (User &connection : connections)
        if (connection.channel.find(name) != connection.channel.end())
            connection.write(message);
}

void User::broadcast(string const &message)
{
    // Broadcast a message to everyone interested in this user
    set<User*> users;
    for (User &observer : connections)
        for (string const &mychan : channel)
            if (observer.channel.find(mychan) != observer.channel.end())
            {
                users.insert(&observer);
                break;
            }

    for (User *user : users)
        user->write(message);
}
