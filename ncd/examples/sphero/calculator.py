import sys
import math

def compute_rel_signed(val, minval, maxval):
    return max(-1.0, min(1.0, (2.0 * ((val - minval) / (maxval - minval)) - 1.0)))

def compute_rel_unsigned(val, minval, maxval):
    return max(0.0, min(1.0, ((val - minval) / (maxval - minval))))

def main():
    while True:
        comps = sys.stdin.readline().rstrip('\n').split(' ')
        numbers = [float(x) for x in comps]
        
        x_val, x_min, x_max, y_val, y_min, y_max, rz_val, rz_min, rz_max, rz_degrees, throttle_pos, throttle_min, throttle_max, turbo, adjust = numbers
        
        x_rel  = compute_rel_signed(x_val, x_min, x_max)
        y_rel  = compute_rel_signed(y_val, y_min, y_max)
        rz_rel = compute_rel_signed(rz_val, rz_min, rz_max)
        throttle_rel = compute_rel_unsigned(throttle_pos, throttle_min, throttle_max)
        
        angle = math.atan2(y_rel, x_rel) + (rz_rel * math.radians(rz_degrees))
        length = math.sqrt(x_rel**2 + y_rel**2) * throttle_rel
        
        angle_fixed = (int(round(math.degrees(angle))) + 90) % 360
        speed_fixed = 0 if adjust else 255 if turbo else max(0, min(255, int(round(255.0 * length))))
        
        sys.stdout.write('{} {}\n'.format(angle_fixed, speed_fixed))
        sys.stdout.flush()
        
main()
