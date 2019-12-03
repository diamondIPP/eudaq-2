# IP variables for start scripts
export RUNCONTROLIP=127.0.0.1
export TLUIP=127.0.0.1
export NIIP=127.0.0.1

echo "Check pathes in telescope.ini and in the conf-files"

# eudaq2 binaries
export EUDAQ=/home/teleuser/ETH/eudaq-2/
export PATH=$PATH:$EUDAQ/bin
