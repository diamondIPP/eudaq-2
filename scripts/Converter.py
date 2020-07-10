#!/usr/bin/env python
# --------------------------------------------------------
#       Script to convert binary eudaq files from the beam test
# created on August October 4th 2016 by M. Reichmann (remichae@phys.ethz.ch)
# --------------------------------------------------------

from argparse import ArgumentParser
from os.path import join, realpath, dirname, expanduser, basename
from glob import glob
from subprocess import check_call
from datetime import datetime

parser = ArgumentParser()
parser.add_argument('run', nargs='?', default=None, type=int)
parser.add_argument('-tc', nargs='?', default=None)
parser.add_argument('-ip', action='store_true')
parser.add_argument('-m', nargs='?', default=0, type=int)

args = parser.parse_args()

location = 'desy'
raw_dir = 'raw'
data_dir = expanduser(join('~', 'data'))
tc = datetime.strptime(args.tc, '%Y%m') if args.tc is not None else None

data_path = join(data_dir, tc.strftime('%Y-%m')) if args.tc is not None else max(glob(join(data_dir, location, '*'.format(location))))
raw_path = join(data_path, raw_dir)

eudaq_dir = dirname(dirname(realpath(__file__)))

run_str = glob(join(raw_path, 'run*{:04d}*.raw'.format(args.run)))[0] if args.run is not None else max(glob(join(raw_path, 'run*.raw')))
print('Converting run {0}'.format(basename(run_str)))
out = 'run{:04d}.root'.format(args.run if args.run is not None else int(basename(run_str).split('_')[0].strip('run')))

max_events = ['-m', str(args.m)] if args.m else []
cmd_list = [join(eudaq_dir, 'bin', 'euCliConverter'), '-i', run_str, '-o', out] + (['-ip'] if args.ip else []) + max_events
print('executing:', ' '.join(cmd_list))
check_call(cmd_list)
