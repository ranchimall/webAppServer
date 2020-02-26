#!/bin/sh

current_date=$(date)
echo "$current_date : Starting webApp" >> logs/app.log

#Read configurations
IFS="="
while read -r var value
do
export "$var"="${value}"
done < .config

#Start the app
echo $current_date >> logs/server.log
app/webSocketServer.bin $PORT $SERVER_PWD >> logs/server.log &
echo $current_date >> logs/browser.log
$BROWSER "http://localhost:$PORT/$SERVER_PAGE#$SERVER_PWD" >> logs/browser.log &
wait

