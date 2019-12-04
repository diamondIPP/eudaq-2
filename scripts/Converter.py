#!/usr/bin/env python
# --------------------------------------------------------
#       Script to convert binary eudaq files from the beam test
# created on August October 4th 2016 by M. Reichmann (remichae@phys.ethz.ch)
# --------------------------------------------------------

from argparse import ArgumentParser
from os.path import join, realpath, dirname
from glob import glob
from subprocess import check_call, CalledProcessError

parser = ArgumentParser()
parser.add_argument('run', nargs='?', default=None)
parser.add_argument('-tc', nargs='?', default=None)
parser.add_argument('-s', nargs='?', default='1')
parser.add_argument('-t', nargs='?', default='telescopetree')
parser.add_argument('-o', action='store_true')
parser.add_argument('-p', nargs='?', default=None)

place = 'psi'
raw_dir = 'raw'
data_dir = '/scratch2/psi'

args = parser.parse_args()
trees = ['caentree', 'drs4tree', 'telescopetree', 'waveformtree']
if args.t not in trees:
    print 'wrong tree {0}! It has to be in {1}'.format(args.t, trees)
    exit()

if args.run is None and args.p is None:
    print 'YOU DID NOT ENTER A RUN! -> exiting'
    exit()

data_paths = glob(join(data_dir, '{}*'.format(place)))
print data_paths
data_path = '/data/{p}_{0}_{1}'.format(args.tc[:4], args.tc[-2:], p=place) if args.tc is not None else max(data_paths)
data_path += '-{0}'.format(args.s) if args.s != '1' else ''
raw_path = join(data_path, raw_dir) if args.p is None else ''

eudaq_dir = dirname(dirname(realpath(__file__)))
conf_dir = join(eudaq_dir, 'conf')

run_str = 'run{0}.raw'.format(args.run.zfill(6)) if not args.o else 'run{0}{1}{2}.raw'.format(args.tc[2:4], args.tc[-2:], args.run.zfill(5))
run_str = args.p if args.p is not None else run_str
print 'Converting run {0}'.format(run_str)

cmd_list = [join(eudaq_dir, 'bin', 'Converter.exe'), '-t', args.t, '-c', join(conf_dir, 'converter_waveform_integrals.conf'), join(raw_path, run_str)]
# cmd = '{eudaq}/bin/Converter.exe -t {tree} -c {conf}/converter_waveform_integrals.conf {raw}/{file}'.format(eudaq=eudaq_dir, conf=conf_dir, tree=args.t, raw=raw_path, file=run_str)
print 'executing:', ' '.join(cmd_list)
max_tries = 10
tries = 0
while tries < max_tries:  # the command crashes randomly...
    try:
        check_call(cmd_list)
        break
    except CalledProcessError:
        tries += 1
