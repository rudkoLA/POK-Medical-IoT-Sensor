# POK-Medical-IoT-Sensor
## Authors (team): <br>
Maxim Holovin(https://github.com/Lloydwqe23), <br>
Viktor Syrotiuk(https://github.com/KOTAYE), <br>
Adam Rudko(https://github.com/rudkoLA), <br>
Anton Deputat(https://github.com/antondep)
<br>

### Usage
```bash
idf.py fullclean
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### Results

In this project, we developed a compact health-monitoring prototype using the ESP32-S3 Zero microcontroller and the MAX30102 optical sensor. Our main goal was to understand how real biomedical signals are collected, processed, and converted into meaningful values such as heart rate and blood oxygen saturation (SpO₂). Throughout the project, we learned how hardware drivers, embedded tasks, and signal-processing algorithms must work together to produce stable and reliable measurements.

### 1. MAX30102 Driver Adaptation
Most available MAX30102 libraries are built for Arduino, so we could not use them directly with ESP-IDF. Because of this, we adapted the driver manually. This allowed the sensor to operate correctly on the ESP32-S3 platform and ensured stable real-time data acquisition.

### 2. Biosignal Processing
The MAX30102 outputs raw infrared and red-light measurements that contain both noise and physiological information.  
To extract a clean pulse signal, we:
- separated DC and AC components,
- removed slow variations,
- applied smoothing and filtering,
- normalized the AC waveform.

This preprocessing step was essential for stable heartbeat detection and accurate SpO₂ estimation.

### 3. Finger Detection
To avoid false readings, we implemented a simple and effective finger-detection algorithm.  
It uses:
- the IR DC level,
- the AC/DC ratio,
- a basic signal-quality estimate.

This allows the device to understand whether the user’s finger is on the sensor and whether the data is reliable.

### 4. Adaptive LED Control
Different users have different finger thickness, skin color, and pressure on the sensor.  
To handle these variations, we implemented adaptive LED control based on the DC level.  

The system automatically increases or decreases the LED brightness to keep the signal in the optimal range.  
This improves stability, reduces noise, and makes the measurement more consistent.

### 5. Heart-Rate Calculation (Peak Detection)
Once the AC signal is cleaned, we detect pulse peaks to estimate heart rate.  
We:
- locate waveform peaks,
- measure the time between them,
- compute BPM using the average interval.

This provides a real-time heart-rate reading based on actual biological pulses.

### 6. SpO₂ Estimation
To estimate blood oxygen saturation, we used the standard ratio-of-ratios method:

R = (AC_red / DC_red) / (AC_IR / DC_IR)  
SpO₂ ≈ 104 − 17 × R

Although simplified, this method demonstrates the principle behind optical oximetry and gives reasonable results for a prototype device.
