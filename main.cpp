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
    enum class ConnectStatus { CONNECTED, NICKSET, READY };
    ConnectStatus status = ConnectStatus::CONNECTED;
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
        if(strchr(delim.c_str(), c[i]) == nullptr)
        {
            output += c[i];
        }
    }
    return output;
}

void chop_newline_off(string &in)
{
    auto last = in.end() - 1;
    if (*last != '\n') return;
    auto secondlast = last - 1;
    if (*secondlast == '\r')
        in.erase(secondlast, in.end());
    else
        in.erase(last, in.end());
}

vector<string> parse_irc_command(string const &command)
{
    vector<string> out;
    size_t pos = 0, end = command.size();

    while (pos < end)
    {
        while (pos < end && command[pos] == ' ') pos++; // Skip spaces
        if (pos >= end) break;
        if (command[pos] == ':')
        {
            if (out.empty()) // Origin prefix, ignore
                while (pos < end && command[pos] != ' ') pos++;
            else // Long parameter
                out.emplace_back(command.substr(pos + 1));
            break;
        }
        else
        {
            size_t copyfrom = pos;
            while (pos < end && command[pos] != ' ') pos++;
            out.emplace_back(command.substr(copyfrom, pos - copyfrom));
        }
    }
    return out;
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

          string command_str(data2);
          chop_newline_off(command_str);
          vector<string> command = parse_irc_command(command_str);
          if (!command.empty())
          {
              if(command[0] == "USER")
              {
                 //stub incase anyone wants to implement authentication
              }
              else if(command[0] == "PING")
              {
                  connections[z].write(":tinyirc PONG :" + command[1] + "\r\n");
              }
              else if(command[0] == "PONG" && connections[z].status != User::ConnectStatus::NICKSET)
              {
                  //reset anti-drop
                  connections[z].dontkick = true;
                  connections[z].rticks = 0;
              }
              else if(command[0] == "PONG" && connections[z].status == User::ConnectStatus::NICKSET)
              {
                  //oh nice, you accepted our PING, welcome to the party
                  connections[z].write(":tinyirc 001 " + connections[z].username + " :Hello!" + "\r\n");
                  connections[z].write(":tinyirc 002 " + connections[z].username + " :This server is running TinyIRC pre-alpha!" + "\r\n");
                  connections[z].write(":tinyirc 003 " + connections[z].username + " :This server doesn't have date tracking." + "\r\n");
                  connections[z].write(":tinyirc 004 " + connections[z].username + " tinyirc " + " tinyirc(0.0.1) " + "CDGNRSUWagilopqrswxyz" + " BCIMNORSabcehiklmnopqstvz" + " Iabehkloqv" + "\r\n");
                  connections[z].write(":tinyirc 005 " + connections[z].username + " CALLERID CASEMAPPING=rfc1459 DEAF=D KICKLEN=180 MODES=4 PREFIX=(qaohv)~&@%+ STATUSMSG=~&@%+ EXCEPTS=e INVEX=I NICKLEN=30 NETWORK=tinyirc MAXLIST=beI:250 MAXTARGETS=4 :are supported by this server\r\n");
                  connections[z].write(":tinyirc 005 " + connections[z].username + " CHANTYPES=# CHANLIMIT=#:500 CHANNELLEN=50 TOPICLEN=390 CHANMODES=beI,k,l,BCMNORScimnpstz AWAYLEN=180 WATCH=60 NAMESX UHNAMES KNOCK ELIST=CMNTU SAFELIST :are supported by this server\r\n");
                  connections[z].write(":tinyirc 251 " + connections[z].username + " :LUSERS is unimplemented." + "\r\n");
                  connections[z].status = User::ConnectStatus::READY;

                  connections[z].dontkick = true;
              }
              else if(command[0] == "NICK")
              {
                  string new_nick = remove_erase_if(command[1], ".,#\r");
                  if(new_nick.size() > 225 || new_nick.size() == 0)
                      new_nick = "FAGGOT" + to_string(rand() % 9000);

                  bool inuse = false;
                  for(int k =0; k < connections.size();k++)
                      if(new_nick == connections[k].username)
                          inuse = true;

                  //if not authed, set username and PING, else set username
                  if(connections[z].status == User::ConnectStatus::CONNECTED)
                  {
                      connections[z].username = new_nick;
                      if(inuse)
                      {
                          connections[z--].kill("Nick already in use");
                          continue;
                      }
                      connections[z].write("PING :" + connections[z].username + "\r\n");
                      connections[z].status = User::ConnectStatus::NICKSET;
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
              }
              else if(command[0] == "JOIN")
              {
                  string channame = command[1];
                  // TODO handle comma separated chan joins?
                  channame = remove_erase_if(channame,":,. \r");
                  if(channame [0] != '#')
                     channame.insert(0,"#");
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
              }
              else if(command[0] == "TOPIC")
              {
                 for(int t = 0;t < channels.size();t++)
                     if(channels[t].name == command[1])
                     {
                         if (command.size() > 2)
                         {
                             channels[t].topic = command[2];
                             channels[t].broadcast(":" + connections[z].username + " TOPIC " + command[1] + " :" + command[2] + "\r\n");
                         }
                         else
                         {
                             connections[z].write(":tinyirc 332 " + connections[z].username + " " + command[1] + " :" + channels[t].topic + "\r\n");
                         }
                         break;
                     }
              }
              else if(command[0] == "PRIVMSG")
              {
                  //send privmsg to all other users in channel
                  // TODO: warning if channel/user doesn't exist
                  string recip = command[1];
                  string msg = command[2];
                  if (recip.size() == 0) break;

                  string buf(":" + connections[z].username + " PRIVMSG " + recip + " :" + msg + "\r\n");
                  if (recip[0] == '#')
                  {
                      for (User &observer : connections)
                          if(&observer != &connections[z])
                              if (observer.channel.find(command[1]) != observer.channel.end())
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
              }
              else if(command[0] == "MODE")
              {
                  //set user mode, required for some irc clients to think you're fully connected(im looking at you xchat)
                  //+i means no messages from people outside the channel and that mode reflects how the server works
                  string const& channel = command[1];
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
              }
              else if(command[0] == "WHO")
              {
                  int channelindex = 0;
                  string p = command[1];
                  for(int l = 0;l < channels.size();l++)
                      if(channels[l].name == p)
                          channelindex = l;
                  connections[z].write(":tinyirc 352 " + connections[z].username + " " + p + " tinyirc " + connections[z].username + "\r\n");
                  for(string const &chanuser : channels[channelindex].users)
                  {
                      connections[z].write(":tinyirc 352 " + connections[z].username + " " + p + " tinyirc " + chanuser + "\r\n");
                  }
                  connections[z].write(":tinyirc 315 " + connections[z].username + " " + channels[channelindex].name + " :End of /WHO list." + "\r\n");
              }
              else if(command[0] == "QUIT")
              {
                  connections[z--].kill("Quit");
              }
              else if(command[0] == "PART")
              {
                  vector<string> ctol;
                  split_string(string(command[1]),",",ctol);
                  string reason = command.size() > 2 ? command[2] : "Leaving";

                  for (string &chantopart : ctol)
                  {
                      auto channeliter = std::find_if(channels.begin(), channels.end(),
                          [&](Channel const& c) { return c.name == chantopart; }
                      );

                      if (channeliter == channels.end())
                          continue; // Don't part a channel that doesn't exist

                      channeliter->notify_part(connections[z], reason);
                      channeliter->remove_user(connections[z]);
                  }
              }
              else if(command[0] == "PROTOCTL")
              {
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
              else
              {
                  cerr << "User " << connections[z].username << " gave us an unknown command " << command_str << endl;
              }
          }

          if(!connections[z].dontkick)
              connections[z].rticks++;
          if(!connections[z].dontkick && connections[z].rticks == 192)
          {
              connections[z--].kill("Ping timed out");
              continue;
          }
          if(rand() % 480 == 42 && connections[z].status == User::ConnectStatus::READY)
          {
              string buf = "PING :" + connections[z].username + "\r\n";
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
    while(true)
    {
        vector<string> cmd;
        string action;
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
