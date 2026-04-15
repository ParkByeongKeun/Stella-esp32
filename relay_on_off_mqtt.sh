#!/bin/bash

TOPIC="iotech/SEMS/IoTech-Router-00123aff0019/20A-1P-dcda0c-4bd2b8/test"


count=0
while [ 1 ] ; do
	## mosquitto_pub -t "iotech/SEMS/IoTech-Router-00123aff0019/20A-1P-dcda0c-4bd2b8/cont" -m '{"relay": "on"}' -h 172.168.10.167
	#mosquitto_pub -t $TOPIC -m '{"relay": "on"}' -h 172.168.10.167
	mosquitto_pub -t "iotech/SEMS/IoTech-Router-00123aff0019/20A-1P-dcda0c-4bd2b8/cont" -m '{"relay": "on"}' -h 172.168.10.167
	echo " on:$count"
	sleep 5
	# # mosquitto_pub -t "iotech/SEMS/IoTech-Router-00123aff0019/20A-1P-dcda0c-4bd2b8/cont" -m '{"relay": "off"}' -h 172.168.10.167
	# mosquitto_pub -t $TOPIC -m '{"relay": "off"}' -h 172.168.10.167
	mosquitto_pub -t "iotech/SEMS/IoTech-Router-00123aff0019/20A-1P-dcda0c-4bd2b8/cont" -m '{"relay": "off"}' -h 172.168.10.167
	echo "off:$count"
	echo "--------------------------"
	sleep 5
	count=$((count+1))
done
