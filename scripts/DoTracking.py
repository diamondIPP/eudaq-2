#!/usr/bin/env python
# --------------------------------------------------------
#       small script to start the EUDAQ Online Monitor
# created on May 15th 2017 by M. Reichmann (remichae@phys.ethz.ch)
# --------------------------------------------------------

from os import system, chdir
from argparse import ArgumentParser
from os.path import join, expanduser, dirname, realpath, isfile

parser = ArgumentParser()
parser.add_argument('-tc', nargs='?', default=None)
parser.add_argument('run')
parser.add_argument('-v', action='store_true')
args = parser.parse_args()

tel = 49

eudaq_dir = dirname(dirname(realpath(__file__)))
converter = join(eudaq_dir, 'scripts', 'Converter.py')
track_dir = expanduser('~/software/TrackingTelescope')
root_file = join(track_dir, 'test{}.root'.format(args.run.zfill(6)))
chdir(track_dir)

cmd = '{c} -t telescopetree {r}'.format(c=converter, r=args.run)
cmd2 = '{d}/TrackingTelescope {f} 0 {t}'.format(f=root_file, d=track_dir, t=tel)
html_file = join(track_dir, 'plots', '{r}'.format(r=args.run), 'index.html')
cmd3 = 'google-chrome {f}'.format(f=html_file)

if not args.v:
    if not isfile(root_file):
        system(cmd)
    system(cmd2)
system(cmd3)
