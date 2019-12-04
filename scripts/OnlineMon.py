#!/usr/bin/env python
# --------------------------------------------------------
#       small script to start the EUDAQ Online Monitor
# created on May 15th 2017 by M. Reichmann (remichae@phys.ethz.ch)
# --------------------------------------------------------

from argparse import ArgumentParser
from os import system
from os.path import join, isfile

parser = ArgumentParser()
parser.add_argument('-tc', nargs='?', default='201908')
parser.add_argument('-x', nargs='?', default='0')
parser.add_argument('run')
args = parser.parse_args()

place = 'psi'
raw_dir = 'setup'
data_dir = '/data'
eudaq_dir = join('~', 'eudaq-drs4')

print 'Converting run {r} of test campaign {tc} in {d}'.format(r=args.run, tc=args.tc, d=raw_dir)

run_dir = join(data_dir, '{p}_{y}_{m}'.format(p=place, y=args.tc[:4], m=args.tc[4:]), raw_dir)
run_file = 'run{n}.raw'.format(n=args.run.zfill(6))
conf_file = join(eudaq_dir, 'conf', 'converter_waveform_integrals.conf')
run_file_path = join(run_dir, run_file)
if not isfile(run_file_path):
    print 'the file in {} does not exist!'.format(run_file_path)
    exit()

prog = join(eudaq_dir, 'bin', 'OnlineMon.exe')
cmd = '{} -sc 1 -u 500 -f {p} -c {c} -x {x}'.format(prog, p=run_file_path, c=conf_file, x=args.x)
print cmd
print
system(cmd)
