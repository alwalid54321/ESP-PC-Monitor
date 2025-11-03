# esp_m.py  -- Serial sender for ESP (wired)
# Sends framed binary: [0xAA][cpu:f][ram:f][temp:f][ts_ms:f][seq:U32]
# Requires: pip install pyserial psutil wmi   (wmi optional)

import struct
import time
import psutil
import platform
import sys

try:
    import serial
except Exception as e:
    print("pyserial not found. Install with: pip install pyserial")
    raise

# Optional WMI (only used as a fallback for temps on Windows)
try:
    import wmi
except Exception:
    wmi = None

# ------ CONFIG ------
COM_PORT = "COM6"        # <- change this to your board COM (check Device Manager)
BAUD = 115200
INTERVAL = 0.5          # seconds between sends (20 Hz)
START_BYTE = 0xAA
FMT = "<ffffI"           # cpu, ram, temp, ts_ms (float), seq (uint32)
PAYLOAD_LEN = struct.calcsize(FMT)
# --------------------

# Fix for the SyntaxWarning: use escaped backslashes or raw string properly when using WMI namespace
LHM_NAMESPACE = "root\\OpenHardwareMonitor"   # safe string (double backslash)

def get_cpu_temp():
    """Try multiple ways to get a sensible CPU/GPU temp on Windows. Return float degC or 0.0."""
    # 1) psutil (may not be implemented on Windows)
    try:
        if hasattr(psutil, "sensors_temperatures"):
            temps = psutil.sensors_temperatures()
            if temps:
                for k, entries in temps.items():
                    if entries:
                        e = entries[0]
                        if hasattr(e, "current") and e.current:
                            return float(e.current)
    except Exception:
        pass

    # 2) WMI via Libre/OpenHardwareMonitor namespace (if wmi and monitor running)
    if wmi:
        try:
            # use double-backslash namespace string to avoid escape issues
            w = wmi.WMI(namespace=LHM_NAMESPACE)
            # query sensors (OpenHardwareMonitor exposes 'Sensor' class)
            sensors = w.Sensor()
            for s in sensors:
                try:
                    # look for temperature sensors and prioritize ones with cpu/gpu keywords
                    if s.SensorType == u'Temperature':
                        name = str(getattr(s, "Name", "")).lower()
                        if "cpu" in name or "gpu" in name or "package" in name or "core" in name:
                            return float(s.Value)
                except Exception:
                    continue
        except Exception:
            pass

    # 3) fallback = 0.0
    return 0.0

def open_serial(port, baud):
    try:
        ser = serial.Serial(port, baud, timeout=0)
        # small pause to allow device to settle
        time.sleep(0.1)
        return ser
    except Exception as e:
        print(f"Failed to open {port} at {baud} baud: {e}")
        raise

def main():
    print("Starting serial sender")
    print("Make sure PlatformIO Serial Monitor is closed and the correct COM port is set.")
    try:
        ser = open_serial(COM_PORT, BAUD)
    except Exception:
        sys.exit(1)

    # warm up psutil
    psutil.cpu_percent(interval=0.1)

    seq = 0
    t0 = time.perf_counter()
    try:
        while True:
            # timestamp in ms (monotonic)
            ts_ms = (time.perf_counter() - t0) * 1000.0

            cpu = psutil.cpu_percent(interval=None)   # non-blocking
            ram = psutil.virtual_memory().percent

            # temp (best effort)
            temp = get_cpu_temp()

            # pack payload and send with start marker
            payload = struct.pack(FMT, float(cpu), float(ram), float(temp), float(ts_ms), int(seq))
            frame = bytes([START_BYTE]) + payload
            try:
                ser.write(frame)
            except Exception as e:
                print("Serial write error:", e)
                # try to reopen once
                try:
                    ser.close()
                    ser = open_serial(COM_PORT, BAUD)
                except Exception:
                    pass

            # debug print (so you can see values locally)
            print(f"Sent seq={seq} CPU={cpu:.1f}% RAM={ram:.1f}% TEMP={temp:.1f}C ts={ts_ms:.1f}ms")

            seq = (seq + 1) & 0xFFFFFFFF
            time.sleep(INTERVAL)

    except KeyboardInterrupt:
        print("Stopped by user")

    finally:
        try:
            ser.close()
        except Exception:
            pass

if __name__ == "__main__":
    main()
