screen -X -S  "mylittlescreen" quit
echo "Starting new screen"
screen -S "mylittlescreen" -d -m
echo "Starting Collector in screen"
screen -r "mylittlescreen" -X stuff $'Collector\n'
echo "And attach"
screen -r "mylittlescreen"
