export RUNCONTROLIP=127.0.0.1
export TLUIP=127.0.0.1
export NIIP=127.0.0.1
RPCPORT=44000

echo "Check pathes in telescope.ini and in the conf-files"

# eudaq2 binaries
export EUDAQ=/home/teleuser/ETH/eudaq-2/
export PATH=$PATH:$EUDAQ/bin

# Start Run Control
xterm -T "Run Control" -e 'euRun' &
sleep 2 

# Start Logger
xterm -T "Log Collector" -e 'euLog -r tcp://${RUNCONTROLIP}' &
sleep 1

# Start one DataCollector
# name (-t) in conf file
xterm -T "Data Collector TLU" -e 'euCliCollector -n TriggerIDSyncDataCollector -t one_dc -r tcp://${RUNCONTROLIP}:${RPCPORT}' &
sleep 1

# Start CMSpixel DataCollector NOT NEEDED
# name (-t) in conf file
# xterm -T "Data Collector TLU" -e 'euCliCollector -n TriggerIDSyncDataCollector -t cmspix_dc -r tcp://${RUNCONTROLIP}:${RPCPORT}' &
# sleep 1

# Start TLU Producer
xterm -T "EUDET TLU Producer" -e "euCliProducer -n EudetTluProducer -t eudet_tlu -r tcp://${RUNCONTROLIP}:${RPCPORT}" &

# Start NI Producer locally connect to LV via TCP/IP
xterm -T "NI/Mimosa Producer" -e 'euCliProducer -n NiProducer -t ni_mimosa -r tcp://${NIIP}:${RPCPORT}' &
sleep 1

# Start CMSPixel Producer
xterm -T "CMSPixelProducer" -e 'euCliProducer -n CMSPixelProducerREF -t CMSREF -r tcp://:${RPCPORT}' &
sleep 1

# OnlineMonitor
xterm -T "Online Monitor" -e 'StdEventMonitor -t StdEventMonitor -r tcp://${RUNCONTROLIP}' & 
