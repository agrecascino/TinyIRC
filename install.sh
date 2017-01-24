#!/bin/bash

g++ main.cpp -lpthread -o tinyircd
mv tinyircd /usr/local/bin/tinyircd
mv tinyirc.service /etc/systemd/system/tinyirc.service 

echo "Run, systemctl start tinyirc.service"