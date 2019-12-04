#!/usr/bin/env python
# --------------------------------------------------------
#       PYTHON SCRIPT TO KILL ALL RUNNING EUDAQ PROCESSES
#       created in 2018 by M. Reichmann (remichae@phys.ethz.ch)
# -------------------------------------------------------

from os import kill, system
from signal import SIGTERM
from utils import *
from argparse import ArgumentParser


class EudaqKill:

    def __init__(self, beam_pc, data_pc):
        self.BeamPC = beam_pc
        self.DataPC = data_pc
        self.Processes = ['TUProducer', 'euLog', 'euRun', 'TestDataCollector.exe', 'EUDET TLU', 'CMSREF', 'CMSDUT', 'ONLINE MONITOR', 'EUDET TELESCOPE']
        self.Screens = ['DRS4Screen', 'CMSPixelScreen', 'CAENScreen', 'CMSPixelScreenDIG1', 'CMSPixelScreenDIG2', 'CMSPixelScreenDUT']
        self.XTerms = ['DataCollector', 'TU', 'CMS Pixel Telescope', 'CMS Pixel DUT', 'Clock Generator', 'WBC Scan', 'DRS4 Producer', 'DRS4 Osci', 'CAEN Producer', 'Max Pos']

    def all(self):
        warning('\nSTART to kill all EUDAQ processes...')
        self.main_processes()
        self.beam_processes()
        self.data_collector()
        self.xterms()
        finished('\nKILLRUN complete')

    @staticmethod
    def process(process, pid):
        try:
            kill(int(pid), SIGTERM)
            print '  killed {} with pid {} ...'.format(process, pid)
        except OSError:
            pass
        except Exception as err:
            print err, type(err)

    def screen(self, screen, pid, hostname=None):
        try:
            if hostname is None:
                self.process(screen, pid)
            else:
                system('ssh -tY {} screen -XS {} kill 2>/dev/null'.format(hostname, pid))
                print '  killed {} with pid {}'.format(screen, pid)
        except Exception as err:
            print err

    def xterms(self):
        for xterm in self.XTerms:
            for pid in get_pids(xterm):
                self.process('xterm', pid)

    def main_processes(self):
        """ Kill all processes on the pc running the DAQ main window. """
        warning('\nCleaning up DAQ computer ...')
        for process in self.Processes:
            for pid in get_pids(process):
                self.process(process, pid)
        finished('Killing EUDAQ on DAQ PC complete')

    def beam_processes(self):
        """ Kill all EUDAQ processes on computer in the beam area. """
        warning('\nCleaning up the beam computer "{}"'.format(self.BeamPC))
        eudaq_screens = ['DRS4Screen', 'CMSPixelScreen', 'CAENScreen', 'CMSPixelScreenDIG1', 'CMSPixelScreenDIG2', 'CMSPixelScreenDUT', 'CMSPixelScreenDIG']
        for pid, screen in get_screens(self.BeamPC).iteritems():
            if screen in eudaq_screens:
                self.screen(screen, pid, self.BeamPC)
        finished('Killing screens on beam computer complete')

    def data_collector(self):
        """ Kill the data collector if running on seperate PC. """
        warning('\nCleaning up the data computer "{}"'.format(self.DataPC))
        for pid, screen in get_screens(self.DataPC).iteritems():
            if screen == 'DataCollectorScreen':
                self.screen(screen, pid, self.DataPC)
        finished('Killing the data collector on the data pc complete')


if __name__ == '__main__':

    p = ArgumentParser()
    p.add_argument('beampc', nargs='?', default=None)
    p.add_argument('datapc', nargs='?', default=None)
    args = p.parse_args()

    z = EudaqKill(args.beampc, args.datapc)
    z.all()
