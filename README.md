# 🛰️ RF Antenna Automation System

A complete, automated solution for measuring antenna radiation patterns using embedded systems, computer vision, and cloud technologies.

## 🎯 Features

- ✅ **Automated Motor Control**: Precision stepper motor rotation (10° increments)
- ✅ **Non-Contact Data Acquisition**: ESP32-CAM with OCR extraction
- ✅ **Real-time Web Interface**: React.js dashboard (Vercel hosted)
- ✅ **Cloud OCR Processing**: Python API on Render for image analysis
- ✅ **Database Integration**: Firebase Realtime Database for measurements
- ✅ **Full Automation**: 360° sweep with 30-second field stabilization
- ✅ **Data Export**: CSV export for further analysis

## 🏗️ System Architecture

```
                    REACT WEBSITE (Vercel)
                    ↕ WiFi HTTP
                    ESP32 MICROCONTROLLER
                    ↕ (Image + Motor Control)
    PYTHON OCR API (Render) ← → FIREBASE DATABASE
```

## 📦 What's Included

```
antenna-system/
├── react-app/              # Frontend (React.js + Vercel)
├── render-api/             # Backend API (Python FastAPI + Render)
├── esp32-firmware/         # Microcontroller code (.ino)
├── firebase-config/        # Firebase setup guide
├── COMPLETE_SETUP_GUIDE.md # Detailed deployment guide
└── README.md              # This file
```

## 🚀 Quick Start

### 1. React Website (5 minutes)

```bash
cd react-app
npm install
npm start                          # Local testing
# Then deploy to Vercel
```

→ Website: `https://your-app.vercel.app`

### 2. Python API (5 minutes)

```bash
cd render-api
# Push to GitHub
# Deploy on Render.com
```

→ API: `https://antenna-ocr-api.onrender.com`

### 3. ESP32 Firmware (10 minutes)

```bash
# Open antenna_esp32.ino in Arduino IDE
# Update WiFi credentials
# Upload to ESP32
# Note the IP address from Serial Monitor
```

→ Access: `http://192.168.1.XXX/`

### 4. Firebase Database (5 minutes)

```
Go to https://console.firebase.google.com
Create project → Set up Realtime Database
Copy credentials to React .env.local
```

→ Database: `https://your-project.firebaseio.com`

## 📋 Requirements

### Hardware
- ESP32 Microcontroller
- A4988 Motor Driver
- NEMA 17 Stepper Motor
- ESP32-CAM Module
- 12V Power Supply
- Breadboard + Jumper Wires
- Meter/Display to measure (with digital readout)

### Software
- Node.js 16+
- Arduino IDE
- Python 3.9+
- Git
- Accounts: GitHub, Vercel, Render, Firebase

## 🔧 Configuration

### ESP32 Pins (Adjustable)
```cpp
#define STEP_PIN 19       // Motor step control
#define DIR_PIN 18        // Motor direction
#define ENABLE_PIN 5      // Motor enable
```

### Motor Settings (Adjustable)
```cpp
#define STEPS_PER_DEGREE 3.33    // 1.8° per step
#define MAX_SPEED 1000            // Speed in microseconds
```

### WiFi (Required Update)
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

## 📊 Data Structure

Firebase stores measurements as:
```json
{
  "measurements": {
    "1695123456789": {
      "angle": 0,
      "current_reading": 14.7871,
      "timestamp": "2024-01-15T10:30:45.123Z"
    },
    "1695123486789": {
      "angle": 10,
      "current_reading": 15.2345,
      "timestamp": "2024-01-15T10:31:15.456Z"
    }
  }
}
```

## 🎮 How to Use

1. **Open Website**: Go to Vercel deployment link
2. **Enter ESP32 IP**: Input your ESP32 IP address
3. **Click Connect**: Verify connection to ESP32
4. **Configure Settings**:
   - Degrees per Move: 10° (or preferred increment)
   - Repeat Count: 36 (for 360° sweep)
5. **Click Start**: Begin automated measurement cycle
6. **Monitor Progress**: Watch real-time angle and progress
7. **View Results**: Data appears in measurements table
8. **Export Data**: Click "Export CSV" for analysis

## 🔄 Measurement Cycle

```
START
  ↓
ROTATE to 0°
  ↓
WAIT 30 seconds (field stabilization)
  ↓
CAPTURE image with ESP32-CAM
  ↓
SEND image to Python API (OCR extraction)
  ↓
RECEIVE extracted value (e.g., 14.7871 mA)
  ↓
STORE in Firebase (angle + value + timestamp)
  ↓
DISPLAY in website (table + graph)
  ↓
ROTATE to 10° → REPEAT
  ↓
CONTINUE for full 360° sweep
  ↓
END
```

## 📖 Detailed Setup

For step-by-step instructions with screenshots, see:
→ **`COMPLETE_SETUP_GUIDE.md`**

## 🐛 Troubleshooting

### Can't connect to ESP32?
- Verify ESP32 IP address (check Serial Monitor)
- Ensure devices are on same WiFi network
- Check ESP32 firmware uploaded successfully

### OCR not working?
- Check image quality from camera
- Verify Python API is deployed on Render
- Test API health: `https://antenna-ocr-api.onrender.com/health`

### Data not saving?
- Confirm Firebase credentials in `.env.local`
- Check Firebase Realtime Database rules are correct
- Verify database URL format

## 🔐 Security

**Development Mode** (Current):
- Firebase runs in Test Mode (no authentication)
- ESP32 accessible on local network
- Python API open to all

**For Production**:
- Enable Firebase Authentication
- Implement API key verification
- Use HTTPS for all connections
- Rate limit the OCR API
- Restrict ESP32 access to local network only

## 📈 Future Enhancements

- [ ] 3D spherical radiation pattern mapping
- [ ] AI-enhanced OCR for universal meter support
- [ ] Multi-frequency SDR integration
- [ ] Web-based data visualization
- [ ] Mobile app for iOS/Android
- [ ] Automatic report generation
- [ ] Machine learning for antenna optimization

## 📝 Documentation

- **Setup Guide**: See `COMPLETE_SETUP_GUIDE.md`
- **Firebase**: See `firebase-config/FIREBASE_SETUP.md`
- **API Docs**: See `render-api/app.py` or visit `/` endpoint
- **Arduino Docs**: See `esp32-firmware/antenna_esp32.ino`

## 🤝 Contributing

This is an educational project. Feel free to:
- Fork and modify
- Add new features
- Report bugs
- Share improvements

## 📄 License

Open source - feel free to use and modify

## 🎓 Educational Value

Perfect for learning:
- Embedded Systems (ESP32)
- IoT Communication (WiFi, HTTP)
- Backend APIs (FastAPI, Python)
- Frontend Development (React.js)
- Cloud Deployment (Vercel, Render)
- Database Management (Firebase)
- Computer Vision (OpenCV, OCR)
- Full-stack Web Development

## 👨‍💻 Technical Stack

| Component | Technology | Platform |
|-----------|-----------|----------|
| Frontend | React.js 18 | Vercel |
| Backend API | Python FastAPI | Render |
| Microcontroller | ESP32 C++ | Arduino IDE |
| Database | Firebase Realtime DB | Google Cloud |
| OCR Engine | Pytesseract | Python |
| Version Control | Git | GitHub |

## 📞 Support

**For Issues:**
1. Check `COMPLETE_SETUP_GUIDE.md` troubleshooting section
2. Review service logs (Vercel, Render dashboards)
3. Check browser console (F12) for errors
4. Check Arduino Serial Monitor for ESP32 status

## 🎉 Success Checklist

- ✅ React website loads
- ✅ Can connect to ESP32
- ✅ Motor rotates on command
- ✅ Camera captures images
- ✅ OCR extracts values
- ✅ Data saves to Firebase
- ✅ Website displays measurements
- ✅ Can export CSV data

If all checks pass, your antenna system is ready! 🛰️

---

**Version**: 1.0.0  
**Last Updated**: 2024-01-15  
**Status**: Production Ready ✅