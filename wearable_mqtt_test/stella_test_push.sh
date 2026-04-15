#Message="\"S_0_0\":0 ,\"S_0_4\":0"
#Message="{\"S_0_0\":0}"

Message0="{\"S_0_0\":1111,\"S_0_4\":1223}" # OK
Message1="{\"S_0_0\":1,\"S_0_1\":5,\"S_0_11\":9,\"S_0_15\":8,\"S_0_16\":4,\"S_0_2\":6,\"S_0_3\":3,\"S_0_4\":2,\"S_0_9\":7,\"latitude\":10.2,\"longitude\":36.4}"
#mosquitto_pub -t "sensors/temperature" -m "{\"location\":\"living_room\", \"value\":$VALUE, \"unit\":\"celsius\"}"


# "{"+
# "\"S_0_0\":" + Const.vMap.get("CH2O") +
# ", \"S_0_4\":" + Const.vMap.get("CO2") +
# ", \"S_0_3\":" + Const.vMap.get("TVOC") +
# ", \"S_0_16\":" + Const.vMap.get("PM1.0") +
# ", \"S_0_1\":" + Const.vMap.get("PM2.5") +
# ", \"S_0_2\":" + Const.vMap.get("PM10") +
# ", \"S_0_9\":" + Const.vMap.get("CO") +
# ", \"S_0_15\":" + Const.vMap.get("O3") +
# ", \"S_0_11\":" + Const.vMap.get("NO2") +
# ", \"latitude\":" + Const.vMap.get("Latitude") +
# ", \"longitude\":" + Const.vMap.get("Longitude") +
# "}";
echo ""
echo "Message0=${Message0}"
echo "Message1=${Message1}"
echo ""

mosquitto_pub -d -q 0 -h "210.117.143.37" -p 10061 \
              -t "v1/devices/me/telemetry" \
			  -u "71bxx34hPM1iyw8eoqfa" \
			  -m ${Message0}
exit
mosquitto_pub -d -q 0 -h "210.117.143.37" -p 10061 \
              -t "v1/devices/me/telemetry" \
			  -u "71bxx34hPM1iyw8eoqfa" \
			  -m ${Message1}
date
#			  -u "7VLX0WqeTs4P0bn880TO" \



#			  -m "{ ${Message} }"
#
#			  -m {"S_0_0":0}  #OK
#
#			  -m { ${Message} }
#			  -m { {\"S_0_0\":"0,\"S_0_4\":"0,\"S_0_3\":0,\"S_0_16\":0,\"S_0_1\":0,\"S_0_2\":0,\"S_0_9\":0,\"S_0_15\":0,\"S_0_11\":0,\"latitude\":0,\"longitude\":0}
#			  -m {"S_0_0":0} 



