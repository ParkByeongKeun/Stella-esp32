
if [ -z $1 ] ; then
	echo "   "
	echo "   	Spedify port num to listen"
	echo ""
	exit
fi

python3 pytest_simple_ota.py . $1 ./server_certs/
