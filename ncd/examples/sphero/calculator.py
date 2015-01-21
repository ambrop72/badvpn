from __future__ import print_function
import sys
import argparse
import math

def main():
    parser = argparse.ArgumentParser()
    args = parser.parse_args()
    
    while True:
        comps = sys.stdin.readline().rstrip('\n').split(' ')
        assert len(comps) == 6
        numbers = [float(x) for x in comps]
        
        x_val = numbers[0]
        x_min = numbers[1]
        x_max = numbers[2]
        y_val = numbers[3]
        y_min = numbers[4]
        y_max = numbers[5]
        
        x_rel = 2.0 * ((x_val - x_min) / (x_max - x_min)) - 1.0
        y_rel = 2.0 * ((y_val - y_min) / (y_max - y_min)) - 1.0
        
        angle = math.atan2(y_rel, x_rel)
        length = math.sqrt(x_rel**2 + y_rel**2)
        
        angle_fixed = int(round(math.degrees(angle))) % 360
        length_fixed = max(0, min(255, int(round(255.0 * length))))
        
        sys.stdout.write('{} {}\n'.format(angle_fixed, length_fixed))
        sys.stdout.flush()
        
main()
