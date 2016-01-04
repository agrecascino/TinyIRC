#include <iostream>
#include <mutex>
#include <set>
#include <map>
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
#include <poll.h>
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
    bool dead = false;
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
    string try_read();
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
vector<User*> connections;
map<string, User*> usersbyname; // username → user
map<string, Channel> channels; // channel name → channel
int listenfd = 0;
struct sockaddr_in serv_addr;
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
    thread z(ControlServer);
    z.detach();
    //again  because we spawned a thread
    signal(SIGPIPE, SIG_IGN);

    while(true)
    {
        {
            mutex_guard lock(connections_mutex);

            auto dead_connections = std::remove_if(connections.begin(), connections.end(),
                        [&](User const *u){ return u->dead; });
            for (auto it = dead_connections; it != connections.end(); it++)
                delete *it;
            connections.erase(dead_connections, connections.end());

            for (User *puser : connections)
            {
                User &user = *puser;
                string command_str = user.try_read();
                if (user.dead) continue;

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
                        user.write(":tinyirc PONG :" + command[1] + "\r\n");
                    }
                    else if(command[0] == "PONG" && user.status != User::ConnectStatus::NICKSET)
                    {
                        //reset anti-drop
                        user.dontkick = true;
                        user.rticks = 0;
                    }
                    else if(command[0] == "PONG" && user.status == User::ConnectStatus::NICKSET)
                    {
                        //oh nice, you accepted our PING, welcome to the party
                        user.write(":tinyirc 001 " + user.username + " :Hello!" + "\r\n");
                        user.write(":tinyirc 002 " + user.username + " :This server is running TinyIRC pre-alpha!" + "\r\n");
                        user.write(":tinyirc 003 " + user.username + " :This server doesn't have date tracking." + "\r\n");
                        user.write(":tinyirc 004 " + user.username + " tinyirc " + " tinyirc(0.0.1) " + "CDGNRSUWagilopqrswxyz" + " BCIMNORSabcehiklmnopqstvz" + " Iabehkloqv" + "\r\n");
                        user.write(":tinyirc 005 " + user.username + " CALLERID CASEMAPPING=rfc1459 DEAF=D KICKLEN=180 MODES=4 PREFIX=(qaohv)~&@%+ STATUSMSG=~&@%+ EXCEPTS=e INVEX=I NICKLEN=30 NETWORK=tinyirc MAXLIST=beI:250 MAXTARGETS=4 :are supported by this server\r\n");
                        user.write(":tinyirc 005 " + user.username + " CHANTYPES=# CHANLIMIT=#:500 CHANNELLEN=50 TOPICLEN=390 CHANMODES=beI,k,l,BCMNORScimnpstz AWAYLEN=180 WATCH=60 NAMESX UHNAMES KNOCK ELIST=CMNTU SAFELIST :are supported by this server\r\n");
                        user.write(":tinyirc 251 " + user.username + " :LUSERS is unimplemented." + "\r\n");
                        user.status = User::ConnectStatus::READY;

                        user.dontkick = true;
                    }
                    else if(command[0] == "NICK")
                    {
                        string new_nick = remove_erase_if(command[1], ".,#\r");
                        if(new_nick.size() > 225 || new_nick.size() == 0)
                            new_nick = "FAGGOT" + to_string(rand() % 9000);

                        bool inuse = false;
                        if (usersbyname.find(new_nick) != usersbyname.end())
                            inuse = true;

                        //if not authed, set username and PING, else set username
                        if(user.status == User::ConnectStatus::CONNECTED)
                        {
                            user.username = new_nick;
                            if(inuse)
                            {
                                user.kill("Nick already in use");
                                continue;
                            }
                            user.write("PING :" + user.username + "\r\n");
                            user.status = User::ConnectStatus::NICKSET;
                            usersbyname[new_nick] = &user;
                        }
                        else
                        {
                            if(inuse)
                                user.write(":tinyirc " "NOTICE :*** Name already in use..." "\r\n");
                            else
                            {
                                // Update nick in all channels
                                for (string const &channelname : user.channel)
                                {
                                    Channel &channel = channels.at(channelname);
                                    channel.users.erase(user.username);
                                    channel.users.insert(new_nick);
                                }
                                usersbyname[new_nick] = &user;
                                usersbyname.erase(user.username);
                                user.broadcast(":" + user.username + " NICK " + new_nick + "\r\n");
                                user.username = new_nick;
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

                        Channel &channel = channels.emplace(piecewise_construct, forward_as_tuple(channame), forward_as_tuple(channame)).first->second;
                        user.channel.insert(channame);
                        channel.users.insert(user.username);
                        channel.broadcast(":" + user.username + " JOIN " + channame + "\r\n");
                        user.write(":tinyirc MODE :" + channame + " +n" + "\r\n");
                        user.write(":tinyirc 332 " + user.username + " " + channame +  " :" + channel.topic + "\r\n");
                        string msgf(":tinyirc 353 " + user.username + " = " + channame + " :");
                        for(string const &chanuser : channel.users)
                        {
                            msgf += chanuser;
                            msgf += " ";
                        }
                        msgf.erase(msgf.end() - 1);
                        msgf += "\r\n";
                        user.write(msgf);
                        user.write(":tinyirc 366 " + user.username + " " + channame + " :Sucessfully joined channel." +"\r\n");
                    }
                    else if(command[0] == "TOPIC")
                    {
                        auto channel_iter = channels.find(command[1]);
                        if (channel_iter == channels.end())
                        {
                            user.write(":tinyirc 403 " + user.username + " " + command[1] + " :No such channel\r\n");
                        }
                        else
                        {
                            Channel &channel = channel_iter->second;
                            if (command.size() > 2)
                            {
                                channel.topic = command[2];
                                channel.broadcast(":" + user.username + " TOPIC " + command[1] + " :" + command[2] + "\r\n");
                            }
                            else
                            {
                                user.write(":tinyirc 332 " + user.username + " " + command[1] + " :" + channel.topic + "\r\n");
                            }
                        }
                    }
                    else if(command[0] == "PRIVMSG")
                    {
                        //send privmsg to all other users in channel
                        // TODO: warning if channel/user doesn't exist
                        string recip = command[1];
                        string msg = command[2];
                        if (recip.size() == 0) break;

                        string buf(":" + user.username + " PRIVMSG " + recip + " :" + msg + "\r\n");
                        try
                        {
                            if (recip[0] == '#')
                                for (string username : channels.at(recip).users)
                                {
                                    User *chanuser = usersbyname.at(username);
                                    if (&user != chanuser)
                                        usersbyname.at(username)->write(buf);
                                }
                            else
                                usersbyname.at(recip)->write(buf);
                        }
                        catch (out_of_range e)
                        {
                           user.write(":tinyirc 401 " + user.username + " " + recip + " :No such nick/channel\r\n");
                        }
                    }
                    else if(command[0] == "MODE")
                    {
                        //set user mode, required for some irc clients to think you're fully connected(im looking at you xchat)
                        //+i means no messages from people outside the channel and that mode reflects how the server works
                        string const& channel = command[1];
                        if(user.channel.size() != 0)
                        {
                            user.write(":tinyirc 324 " + user.username + " " + channel + " +n" + "\r\n");
                            user.write(":tinyirc 329 " + user.username + " " + channel + " 0 0" + "\r\n");
                        }
                        else
                        {
                            user.detectautisticclient = true;
                            user.write(":" + user.username + " MODE " + user.username + " :+i" + "\r\n");
                        }
                    }
                    else if(command[0] == "WHO")
                    {
                        auto const channel_iter = channels.find(command[1]);
                        if (channel_iter != channels.end())
                            for(string const &chanuser : channel_iter->second.users)
                                user.write(":tinyirc 352 " + user.username + " " + command[1] + " tinyirc " + chanuser + "\r\n");
                        user.write(":tinyirc 315 " + user.username + " " + command[1] + " :End of /WHO list.\r\n");
                    }
                    else if(command[0] == "QUIT")
                    {
                        user.kill("Quit");
                    }
                    else if(command[0] == "PART")
                    {
                        vector<string> ctol;
                        split_string(string(command[1]),",",ctol);
                        string reason = command.size() > 2 ? command[2] : "Leaving";

                        for (string &chantopart : ctol)
                        {
                            auto channeliter = channels.find(chantopart);
                            if (channeliter == channels.end())
                                continue; // Don't part a channel that doesn't exist

                            Channel &channel = channeliter->second;
                            channel.notify_part(user, reason);
                            channel.remove_user(user);
                        }
                    }
                    else if(command[0] == "PROTOCTL")
                    {
                        //gives capabilities of the server, some irc clients dont send one for some reason (im looking at you two irssi and weechat)
                        user.write(":tinyirc 252 " + user.username + " 0 :IRC Operators online" + "\r\n");
                        user.write(":tinyirc 253 " + user.username + " 0 :unknown connections" + "\r\n");
                        user.write(":tinyirc 254 " + user.username + " 0 :LUSERS is unimplmented" + "\r\n");
                        user.write(":tinyirc 255 " + user.username + " :LUSERS is unimplmented" + "\r\n");
                        user.write(":tinyirc 265 " + user.username + " 1 1 :LUSERS is unimplmented" + "\r\n");
                        user.write(":tinyirc 266 " + user.username + " 1 1 :LUSERS is unimplmented" + "\r\n");
                        user.write(":tinyirc 375 " + user.username + " 1 1 :Welcome to tinyirc pre-alpha!" + "\r\n");
                        user.write(":tinyirc 372 " + user.username + " :Padding call" + "\r\n");
                        user.write(":tinyirc 376 " + user.username + " :Ended" + "\r\n");
                        if(!user.detectautisticclient)
                            user.write(":" + user.username + " MODE " + user.username + " :+i" + "\r\n");
                    }
                    else
                    {
                        user.write(":tinyirc 421 " + user.username + " " + command[0] + " :Unknown command\r\n");
                    }
                }

                if(!user.dontkick)
                    user.rticks++;
                if(!user.dontkick && user.rticks == 192)
                {
                    user.kill("Ping timed out");
                    continue;
                }
                if(rand() % 480 == 42 && user.status == User::ConnectStatus::READY)
                {
                    string buf = "PING :" + user.username + "\r\n";
                    send(user.connfd,buf.c_str(),buf.size(),MSG_NOSIGNAL);
                    user.dontkick = false;
                }
            }
        }
        connections_mutex.lock();
        struct pollfd pollfds[connections.size() + 1] = {};
        pollfds[0].fd = listenfd;
        pollfds[0].events = POLLIN;
        int i = 1;
        for (User *user : connections)
        {
            pollfds[i].fd = user->connfd;
            pollfds[i].events = POLLIN;
            i++;
        }
        connections_mutex.unlock();

        poll(pollfds, i, 30000);

        if (pollfds[0].revents & POLLIN)
        {
            int connfd = accept(listenfd, nullptr, nullptr);
            if(connfd != -1)
            {
                mutex_guard lock(connections_mutex);
                connections.push_back(new User(connfd)); // accept awaiting request
            }
        }

    }
    return 0;
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
            for (User *user : connections)
                cout << "User: " << user->username << endl;
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
            auto userit = usersbyname.find(name);
            if (userit != usersbyname.end())
            {
                userit->second->kill("Kicked by OP");
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
                for (User *user : connections)
                    user->write(":tinyirc NOTICE " + user->username + " :" + action + "\r\n");
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
    write(":tinyirc KILL " + username + " :" + reason + "\r\n");
    broadcast(quitbroadcast);
    close(connfd);
    for (string const &userchannel : channel)
        // Assume channel exists. Otherwise, we fucked up elsewhere and will break here.
        channels.at(userchannel).remove_user(*this);
    dead = true;
    auto userit = usersbyname.find(username);
    if (userit != usersbyname.end() && userit->second == this)
        usersbyname.erase(userit);
}

void Channel::broadcast(string const &message)
{
    for (string const &username : users)
        usersbyname.at(username)->write(message);
}

void User::broadcast(string const &message)
{
    // Broadcast a message to everyone interested in this user
    set<User*> users;
    for (string const &channame : channel)
        for (string const &username : channels.at(channame).users)
            users.insert(usersbyname.at(username));
    users.insert(this); // Just in case we're not in a channel

    for (User *user : users)
        user->write(message);
}

string User::try_read()
{
    string line;
    char data = 0; // Reading one at a time, hack!
    while (data != '\n')
    {
        ssize_t datarecv = recv(connfd, &data, 1, MSG_DONTWAIT);
        if(datarecv == -1)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                kill("Connection error");
            break;
        }

        if(datarecv == 0)
        {
            kill("Connection error");
            break;
        }

        line += data;
        if (line.size() > 2048)
        {
            kill("Line too long");
            break;
        }
    }
    return line;
}
