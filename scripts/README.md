# EUDAQ Starting Scripts

### Usage
 - run main file "STARTRUN.py \<conf>  (-t)"
 - all settings provided by the config files in the config directory
 - main config is the file without the dash
 - subconfig behind the dash overwrites the setting from the main file
 - if flag "-t" is set, it runs in test mode without starting the program

### Settings File
 
 - [DEVICE]
    - cmstel = False --> Start analogue telescope
    - cmsdut = False --> digital telescope (DUT)
    - digtel1 = False --> additional digital telescope
    - digtel2 = False --> additional digital telescope
    - caen = False --> CAEN producer
    - clock = False --> clock generator
    - drs4 = False --> DRS4 Producer
    - drsgui = False --> DRS4 oscilloscope gui
    - wbc = Falsee --> wbc scan interface
 - [MISC]
    - inet device = 'eth0' --> name of device in ifconfig which is connected to the internet
 - [DIR]
    - data = sdvlp/eudaq-drs4 --> data directory on the data PC (only relevant if data PC differs from main PC)
 - [PORT]
    - RC = 44000 --> EUDAQ RC port number
 - [HOST]
    - beam = pim1 --> Host of the PC in the beam area (None if same PC)
    - data = rapidshare --> Host of the PC where data is collected (None if same PC)
 - [WINDOW]
    - monitor number = 1 --> Number of the monitor where the windows should be
    - spacing = 10 --> spacing in pixels between the xwindows
    - height = 17 --> height in rows of the xwindow