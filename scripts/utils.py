#!/usr/bin/env python
# --------------------------------------------------------
#       Utility methods for the EUDAQ Scripts
#       created on April 11th 2019 by M. Reichmann (remichae@phys.ethz.ch)
# -------------------------------------------------------
from commands import getstatusoutput
from time import sleep


RED = '\033[91m'
GREEN = '\033[92m'
BOLD = '\033[1m'
ENDC = '\033[0m'


def warning(txt):
    print '{}{}{}{}'.format(BOLD, RED, txt, ENDC)


def finished(txt):
    print '{}{}{}{}'.format(BOLD, GREEN, txt, ENDC)


def get_output(command, process=''):
    return [word.strip('\r\t') for word in getstatusoutput('{} {}'.format(command, process))[-1].split('\n')]


def get_pids(process):
    pids = [int(word) for word in get_output('pgrep -f 2>/dev/null', '"{}"'.format(process))]
    return pids


def get_screens(host=None):
    host = 'ssh -tY {} '.format(host) if host is not None else ''
    screens = dict([tuple(word.split('\t')[0].split('.')) for word in get_output('{}screen -ls 2>/dev/null'.format(host))[1:-1]])
    return screens


def get_x(name):
    while True:
        try:
            return int(get_output('xwininfo -name "{}" | grep "Absolute upper-left X"'.format(name))[0].split(':')[-1])
        except ValueError:
            sleep(.1)


def get_width(name):
    return int(get_output('xwininfo -name "{}" | grep "Width"'.format(name))[0].split(':')[-1])


def get_height(name):
    return int(get_output('xwininfo -name "{}" | grep "Height"'.format(name))[0].split(':')[-1])


def get_user(host):
    return get_output('ssh -tY {} whoami'.format(host))[0]
