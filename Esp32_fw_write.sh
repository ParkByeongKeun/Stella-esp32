#!/bin/bash

# partitions_ota.csv 기준:
#   otadata 0x1d000, size 0x2000 (8KB)
#   phy_init 0x1f000
#   factory  0x20000
# ota_data_initial.bin 은 otadata 파티션 크기(8KB)를 넘기면 안 됨 (그 다음은 phy_init).

if [ -z $1 ] ; then
	echo ""
	echo "	specify fw_image name ( like build/stella_firmware.bin )"
	echo ""
	exit
fi

filename=$1

OTA_ADDR=0x1d000
# otadata 파티션 크기와 동일 (partitions_ota.csv 의 otadata, Size 0x2000)
OTA_MAX=8192
OTA_FILE="build/ota_data_initial.bin"
OTA_ARG=""
if [ -f "$OTA_FILE" ]; then
	OTA_SZ=$(wc -c < "$OTA_FILE")
	if [ "$OTA_SZ" -le "$OTA_MAX" ]; then
		OTA_ARG="$OTA_ADDR $OTA_FILE"
	else
		echo ""
		echo "	WARNING: $OTA_FILE is ${OTA_SZ} bytes (max ${OTA_MAX} = otadata partition 8KB)."
		echo "	Skipping OTA data flash. Regenerate with idf.py build (expect 8KB) or fix partition table."
		echo ""
	fi
fi

echo ""
echo "	UART1 set to GPIO for ESP32 Programming'"
echo "			raspi-gpio set 4,5 a0"
echo ""
raspi-gpio set 4,5 ip
raspi-gpio get 4,5 
esptool.py -p /dev/ttyUSB0 \
            -b 460800 \
            --before default_reset \
            --after hard_reset \
            --chip esp32s3 \
            write_flash \
            --flash_mode dio \
            --flash_size detect \
            --flash_freq 40m \
            0x0000  build/bootloader/bootloader.bin \
            0x8000  build/partition_table/partition-table.bin \
            $OTA_ARG \
            0x20000 $filename

echo ""
echo "	UART1 set to UART again'"
echo "			raspi-gpio set 4,5 a4"
echo ""
raspi-gpio set 4,5 a4
raspi-gpio get 4,5
