#!/usr/bin/env python
# --------------------------------------------------------
#       small script to start the EUDAQ Online Monitor
# created on May 15th 2017 by M. Reichmann (remichae@phys.ethz.ch)
# --------------------------------------------------------

from argparse import ArgumentParser
from ConfigParser import ConfigParser
from os import system
from os.path import join, dirname, realpath, basename
from glob import glob
from utils import warning
from json import loads

parser = ArgumentParser()
parser.add_argument('-tc', nargs='?', default='201908')
parser.add_argument('-x', nargs='?', default='0')
parser.add_argument('run', nargs='?', default=None, type=int)
args = parser.parse_args()

# Directories
place = 'psi'
raw_dir = 'setup'
eudaq_dir = dirname(dirname(realpath(__file__)))
data_dir = join(dirname(eudaq_dir), 'data')
conf = 'desy'

# Configuration
config = ConfigParser()
config.read(join(eudaq_dir, 'scripts', 'config', '{}.ini'.format(conf)))

runs = glob(join(data_dir, 'run*.raw'))
try:
    run_file = max(runs) if args.run is None else next(run for run in runs if basename(run).startswith('run{:06d}'.format(args.run)))
except StopIteration:
    run_file = max(runs)
    warning('Did not find run {} --> taking the last one ({}).'.format(args.run, run_file))

warning('Starting Online Monitor for {}'.format(basename(run_file)))

conf_file = join(eudaq_dir, *loads(config.get('MISC', 'config file')))

prog = join(eudaq_dir, 'bin', config.get('MISC', 'online monitor'))
cmd = '{} -d {p} -c {c} -rf '.format(prog, p=run_file, c=conf_file)
print '{} -d {p} -c {c} -rf '.format(basename(prog), p=basename(run_file), c=basename(conf_file))
print
system(cmd)
