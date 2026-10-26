import sys, os
sys.path.insert(0, r"C:\Users\Harish\river-cleaning-boat\ai\dataset_tools")
import build_dataset as b
b.ensure()
log = open(r"C:\Users\Harish\river-cleaning-boat\logs\dataset_build.log","a")
sys.stdout = log; sys.stderr = log
b.build_water(max_imgs=600)
b.report("final")
