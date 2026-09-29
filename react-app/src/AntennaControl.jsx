import React, { useState, useEffect, useRef } from 'react';
import { db } from './firebase-config';
import { ref, onValue, set, remove } from 'firebase/database';
import './AntennaControl.css';

const AntennaControl = () => {
  // ═══════════════════════════════════════════════════════════════════
  // STATIC IP ADDRESSES
  // ═══════════════════════════════════════════════════════════════════
  const MAIN_ESP32_IP = '10.135.98.50';        // Main ESP32 (Motor)
  const ESP32_CAM_IP = '10.135.98.51';         // ESP32-CAM (Camera)
  
  // State Management
  const [isConnected, setIsConnected] = useState(false);
  const [isRunning, setIsRunning] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  const [currentAngle, setCurrentAngle] = useState(0);
  const [degreesPerMove, setDegreesPerMove] = useState(10);
  const [repeatCount, setRepeatCount] = useState(36);
  const [measurements, setMeasurements] = useState([]);
  const [status, setStatus] = useState('Ready to connect');
  const [progress, setProgress] = useState(0);
  const [currentMeasurementIndex, setCurrentMeasurementIndex] = useState(0);
  
  // Ref for instantly stopping the loop
  const isRunningRef = useRef(false);

  // Firebase: Listen to measurements
  useEffect(() => {
    const measurementsRef = ref(db, 'measurements');
    const unsubscribe = onValue(measurementsRef, (snapshot) => {
      if (snapshot.exists()) {
        const data = snapshot.val();
        const measurementsArray = Object.keys(data).map((key) => ({
          id: key,
          ...data[key],
        }));
        setMeasurements(measurementsArray.sort((a, b) => a.angle - b.angle));
      } else {
        setMeasurements([]);
      }
    });
    return () => unsubscribe();
  }, []);

  // ═══════════════════════════════════════════════════════════════════
  // INTERRUPTIBLE WAIT (Fixes the Pause Button)
  // ═══════════════════════════════════════════════════════════════════
  const waitWithProgress = (seconds, angle) => {
    return new Promise((resolve) => {
      let remaining = seconds;
      const interval = setInterval(() => {
        // Instantly break the timer if user clicked Pause
        if (!isRunningRef.current) {
          clearInterval(interval);
          resolve('PAUSED');
          return;
        }
        
        remaining--;
        setStatus(`⏳ Waiting ${remaining}s at ${angle}° for stabilization...`);
        
        if (remaining <= 0) {
          clearInterval(interval);
          resolve('DONE');
        }
      }, 1000);
    });
  };

  // ═══════════════════════════════════════════════════════════════════
  // VALIDATE DEVICE CONNECTION
  // ═══════════════════════════════════════════════════════════════════
  const validateMotor = async () => {
    try {
      const res = await fetch(`http://${MAIN_ESP32_IP}/ping`, { signal: AbortSignal.timeout(5000) });
      return res.ok;
    } catch { return false; }
  };

  const validateCamera = async () => {
    try {
      const res = await fetch(`http://${ESP32_CAM_IP}/ping`, { signal: AbortSignal.timeout(5000) });
      return res.ok;
    } catch { return false; }
  };

  // ═══════════════════════════════════════════════════════════════════
  // 📡 CONNECTION
  // ═══════════════════════════════════════════════════════════════════
  const handleConnect = async () => {
    try {
      setStatus('Connecting to 10.135.98.50...');
      const response = await fetch(`http://${MAIN_ESP32_IP}/status`, { signal: AbortSignal.timeout(5000) });
      
      if (response.ok) {
        setIsConnected(true);
        setStatus('✓ Connected! Ready to start.');
      } else {
        throw new Error('No response from Motor ESP32');
      }
    } catch (error) {
      setStatus('✗ Connection Failed');
      alert(`Error: ${error.message}\n\nMake sure ESP32s are powered on and you are on the same WiFi.`);
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // MOTOR CONTROL
  // ═══════════════════════════════════════════════════════════════════
  const sendCommand = async (command, angle = null) => {
    try {
      let url = `http://${MAIN_ESP32_IP}/rotate?angle=${angle}`;
      if (command === 'reset') url = `http://${MAIN_ESP32_IP}/rotate?angle=0`;
      
      const response = await fetch(url, { signal: AbortSignal.timeout(15000) });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      return true;
    } catch (error) {
      console.error(`[MOTOR] Error:`, error.message);
      return false;
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // CAPTURE & UPLOAD (Laptop acts as the secure middleman)
  // ═══════════════════════════════════════════════════════════════════
  const captureAndExtract = async () => {
    try {
      // 1. Download image from Local Camera
      setStatus(`📷 Capturing image from Camera...`);
      const captureResponse = await fetch(`http://${ESP32_CAM_IP}/capture`, {
        method: 'GET',
        signal: AbortSignal.timeout(15000) 
      });
      if (!captureResponse.ok) throw new Error('Camera Capture Failed');
      
      const imageBlob = await captureResponse.blob();

      // 2. Upload image to Render API (Internet)
      setStatus(`📤 Uploading to Render API...`);
      const formData = new FormData();
      formData.append('file', imageBlob, 'meter.jpg');
      
      const extractResponse = await fetch('https://antenna-ocr-api.onrender.com/extract-ocr', {
        method: 'POST',
        body: formData,
        signal: AbortSignal.timeout(80000) // 80s timeout allows Render to wake up
      });
      
      const data = await extractResponse.json();
      if (!data.success) throw new Error(`OCR Error: ${data.error || 'Failed'}`);
      
      return parseFloat(data.extractedValue);
      
    } catch (error) {
      console.error('[ERROR] Capture/Extract failed:', error.message);
      return null; // Return null so the loop doesn't crash
    }
  };

  const saveMeasurement = async (angle, value) => {
    try {
      const measurementRef = ref(db, `measurements/${Date.now()}`);
      await set(measurementRef, {
        angle,
        current_reading: value,
        timestamp: new Date().toISOString(),
      });
    } catch (error) { console.error('[FIREBASE] Save error:', error); }
  };

  // ═══════════════════════════════════════════════════════════════════
  // MAIN MEASUREMENT LOOP
  // ═══════════════════════════════════════════════════════════════════
  const runMeasurementCycle = async (startIndex = 0) => {
    for (let i = startIndex; i < repeatCount; i++) {
      // 1. Check if user paused before moving
      if (!isRunningRef.current) {
        setCurrentMeasurementIndex(i);
        setStatus('⏸ Paused');
        return;
      }

      const angle = (i * degreesPerMove) % 360;
      setCurrentAngle(angle);
      setProgress(Math.round((i / repeatCount) * 100));

      // 2. Move Motor
      setStatus(`🔄 Rotating to ${angle}°...`);
      const moved = await sendCommand('rotate', angle);
      if (!moved) {
        setStatus(`⚠️ Motor failed to reach ${angle}°`);
        continue;
      }

      // 3. Wait for stabilization (Interruptible)
      const waitResult = await waitWithProgress(30, angle);
      if (waitResult === 'PAUSED') {
        setCurrentMeasurementIndex(i);
        setStatus('⏸ Paused during wait');
        return;
      }

      // 4. Capture and Extract
      if (!isRunningRef.current) {
        setCurrentMeasurementIndex(i);
        return;
      }
      
      const extractedValue = await captureAndExtract();
      
      if (extractedValue === null) {
        console.warn(`[WARNING] Failed at ${angle}°. Recording 0 mA to continue test.`);
        await saveMeasurement(angle, 0);
      } else {
        await saveMeasurement(angle, extractedValue);
        setStatus(`✅ Angle ${angle}°: ${extractedValue} mA`);
      }
    }

    // Finished
    setStatus('✨ Measurement cycle complete!');
    setProgress(100);
    isRunningRef.current = false;
    setIsRunning(false);
    setIsPaused(false);
    setCurrentMeasurementIndex(0);
  };

  // ═══════════════════════════════════════════════════════════════════
  // BUTTON HANDLERS
  // ═══════════════════════════════════════════════════════════════════
  const handleStart = async () => {
    if (!isConnected) return;
    setStatus('🔍 Validating devices...');
    
    if (!(await validateMotor())) { alert('Motor ESP32 offline'); return; }
    if (!(await validateCamera())) { alert('Camera ESP32 offline'); return; }

    isRunningRef.current = true;
    setIsRunning(true);
    setIsPaused(false);
    setCurrentMeasurementIndex(0);
    setProgress(0);
    
    // Clear old data automatically on fresh start
    await remove(ref(db, 'measurements'));
    setMeasurements([]);
    
    await runMeasurementCycle(0);
  };

  const handlePause = () => {
    isRunningRef.current = false; // Instantly tells the loop to stop
    setIsRunning(false);
    setIsPaused(true);
    setStatus('⏸ Pausing...'); // Will update to "Paused" once loop exits
  };

  const handleResume = async () => {
    if (isRunningRef.current) return;
    isRunningRef.current = true;
    setIsRunning(true);
    setIsPaused(false);
    setStatus('▶ Resuming...');
    await runMeasurementCycle(currentMeasurementIndex);
  };

  const handleReset = async () => {
    setCurrentAngle(0);
    setCurrentMeasurementIndex(0);
    setProgress(0);
    setIsPaused(false);
    setStatus('↺ Resetting motor to 0°...');
    const success = await sendCommand('reset');
    if (success) setStatus('↺ Reset to 0° ✓');
  };

  // ═══════════════════════════════════════════════════════════════════
  // DATA MANAGEMENT
  // ═══════════════════════════════════════════════════════════════════
  const handleClearData = async () => {
    if (window.confirm('Clear all measurements?')) {
      await remove(ref(db, 'measurements'));
      setMeasurements([]);
      setStatus('✓ Data cleared');
    }
  };

  const handleExportCSV = () => {
    if (measurements.length === 0) return alert('No data to export');
    const csv = [
      ['Angle (°)', 'Current (mA)', 'Timestamp'],
      ...measurements.map(m => [m.angle, m.current_reading, m.timestamp])
    ].map(row => row.join(',')).join('\n');

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `antenna_measurements_${Date.now()}.csv`;
    a.click();
  };

  // ═══════════════════════════════════════════════════════════════════
  // RENDER UI
  // ═══════════════════════════════════════════════════════════════════
  return (
    <div className="antenna-container">
      <header className="antenna-header">
        <h1>🛰️ RF Antenna Automation System</h1>
        <p>Automated Antenna Radiation Pattern Measurement</p>
      </header>

      <div className="main-content">
        <div className="control-panel">
          
          <section className="card">
            <h2>📡 ESP32 Connection</h2>
            <div className="input-group">
              <input type="text" value="Auto-Configured IPs" disabled style={{backgroundColor: '#f0f0f0', cursor: 'not-allowed'}} />
              <button 
                onClick={handleConnect} 
                disabled={isConnected || isRunning}
                className={`btn btn-primary ${isConnected ? 'connected' : ''}`}
              >
                {isConnected ? '✓ Connected' : 'Connect'}
              </button>
            </div>
            <p className="status-text">{status}</p>
          </section>

          <section className="card">
            <h2>⚙️ Settings</h2>
            <div className="setting-group">
              <label>Degrees per Move:
                <input type="number" value={degreesPerMove} onChange={(e) => setDegreesPerMove(Number(e.target.value))} disabled={isRunning || isPaused} />
              </label>
            </div>
            <div className="setting-group">
              <label>Repeat Count:
                <input type="number" value={repeatCount} onChange={(e) => setRepeatCount(Number(e.target.value))} disabled={isRunning || isPaused} />
              </label>
            </div>
          </section>

          <section className="card">
            <h2>🎮 Control Panel</h2>
            <div className="button-group">
              <button onClick={handleStart} disabled={!isConnected || isRunning || isPaused} className="btn btn-success">
                ▶ Start
              </button>
              <button onClick={handlePause} disabled={!isRunning} className="btn btn-warning">
                ⏸ Pause
              </button>
              <button onClick={handleResume} disabled={!isPaused} className="btn btn-info">
                ▶ Resume
              </button>
            </div>
            <div className="button-group">
              <button onClick={handleReset} disabled={!isConnected || isRunning} className="btn btn-danger">
                ↺ Reset Motor to 0°
              </button>
            </div>
          </section>

          <section className="card">
            <h2>📊 Progress</h2>
            <div className="current-angle">
              <span className="angle-value">{currentAngle}°</span>
            </div>
            <div className="progress-bar">
              <div className="progress-fill" style={{ width: `${progress}%` }}></div>
            </div>
            <p className="progress-text">{progress}% Complete</p>
          </section>
        </div>

        <div className="data-panel">
          <section className="card">
            <h2>📈 Measurements</h2>
            <div className="data-actions">
              <button onClick={handleExportCSV} className="btn btn-small btn-primary">📥 Export CSV</button>
              <button onClick={handleClearData} disabled={isRunning} className="btn btn-small btn-danger">🗑 Clear</button>
            </div>

            {measurements.length === 0 ? (
              <p className="no-data">No measurements yet. Click Start!</p>
            ) : (
              <div className="data-table">
                <table>
                  <thead>
                    <tr><th>Angle (°)</th><th>Current (mA)</th><th>Time</th></tr>
                  </thead>
                  <tbody>
                    {measurements.map((m) => (
                      <tr key={m.id}>
                        <td>{m.angle}</td>
                        <td>{m.current_reading !== 0 ? m.current_reading.toFixed(4) : "Failed"}</td>
                        <td>{new Date(m.timestamp).toLocaleTimeString()}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </section>
        </div>
      </div>
    </div>
  );
};

export default AntennaControl;