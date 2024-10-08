#!/bin/bash

export CYCLES_PORT=50018

cat<<EOF> config.yaml
gameHeight: 500
gameWidth: 500
gameBannerHeight: 100
gridHeight: 100
gridWidth: 100
maxClients: 60
EOF

./build/bin/server &
sleep 1

for i in {1..6}
do
./build/bin/client randomio$i &
done

./build/bin/my_client my_client &
