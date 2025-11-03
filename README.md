
# ESP-PC-Monitor

A compact system monitor using an **ESP8266MOD** with a **built-in display**, connected to your PC.  
It visualizes **CPU usage**, **RAM usage**, and **temperature**, updating nearly in real-time.  
Perfect for a minimal desktop monitor or embedded setup to watch system performance live.

---

## 🔧 How It Works

- **Python Script (`esp_m.py`)**  
  Runs on your PC and gathers live system data (CPU, RAM, temperature) using `psutil` and `wmi`.  
  It then sends compact binary data packets to the ESP8266 over **wired serial (USB)** for maximum speed.

- **ESP8266 Firmware**  
  Built with **PlatformIO** (ESP8266 Arduino framework).  
  Receives and decodes binary packets, draws a dynamic graph, and displays numeric stats on the OLED or built-in screen.  
  A button toggles between different graph modes (CPU, RAM, Temp).

---

## ⚙️ Features

- 🚀 **Real-time updates** — high-speed serial transmission for instant data refresh  
- 📊 **Dynamic graph display** — smooth transitions and automatic scaling  
- 🧠 **Local smoothing** — averages data samples to reduce flicker  
- 🔘 **Mode switching** — toggle between CPU, RAM, Temp using the built-in button  
- ⚡ **Compact binary protocol** — lower latency and CPU load  
- 🧮 **Optional timestamp** — for latency tracking and synchronization  

---

## 🪫 Connection Options

### 🔌 **Wired Mode (Default & Recommended)**
This is the fastest and most stable setup.

1. Connect the **ESP8266MOD** to your PC via USB.  
2. Ensure the serial port matches your Python script (e.g., `COM3`).  
3. Run the script — updates appear instantly on the display.

> ⚠️ **Tip:** For ultra-smooth refresh, use a short USB cable and disable power-saving for the COM port.

---

### 📶 **Wi-Fi Mode (Optional Upgrade)**
If you prefer wireless operation, the same system can be upgraded:

- Replace the serial transmission with UDP or WebSocket packets.  
- The ESP8266 runs a small TCP or UDP server, and the PC script sends packets over the network.  
- Ideal for setups where the PC is far from the display or when you want multiple ESP clients showing the same stats.

> ⚙️ Java, Rust, or Node.js servers can replace the Python sender for even faster and more efficient packet handling if desired.

---

## 🧠 How to Run (Wired)

1. **Install dependencies:**
   ```bash
   pip install psutil pyserial wmi


2. **Connect your ESP8266 via USB** and note the COM port.

3. **Run the Python monitor:**

   ```bash
   py esp_m.py
   ```

4. Watch your ESP display light up with CPU, RAM, and temperature info.

---



## 🧰 Future Improvements

* ✅ Wi-Fi packet streaming (UDP/WebSocket)
* ✅ Optional sensor expansion (GPU temp, disk IO, fan RPM)
* ⚙️ Switchable themes or graph scaling modes
* ⚡ Rust or Java server version for near-zero latency

---

## 🧾 License

MIT License — free to modify, improve, and use commercially.

---

