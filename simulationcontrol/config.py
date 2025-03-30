import os

HERE = os.path.dirname(os.path.abspath(__file__))
SNIPER = os.path.dirname(HERE)

RESULTS_FOLDER = os.path.join(SNIPER, 'results')
NUMBER_CORES = 64
SNIPER_CONFIG = 'kingscross-nuca'
ENABLE_HEARTBEATS = True
# SCRIPTS = ['magic_timestamp', 'magic_perforation_rate:0,0,0'] # for running with heartbeats and no perforation active. 
SCRIPTS = ['magic_timestamp']
