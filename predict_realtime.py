import serial
import time
import joblib
import pandas as pd
from flask import Flask, jsonify
import threading

# Load model
scaler, clf, le = joblib.load("battery_model_real_world.pkl")

# Open serial port
ser = serial.Serial('COM4', 9600, timeout=1)
time.sleep(2)

app = Flask(__name__)
latest_prediction = {}

# Background serial reading
def read_serial():
    global latest_prediction
    counter = 0
    while True:
        try:
            # Inject manual Safe data every 10th cycle
            counter += 1
            if counter % 10 == 0:
                print("[MANUAL] Injecting test Safe data")
                features = [35]*8 + [6.2, 0.3, 450, 1]
                X_df = pd.DataFrame([features], columns=[
                    'T1','T2','T3','T4','T5','T6','T7','T8',
                    'Voltage','Current','Rain_A','Rain_D'
                ])
                X_scaled = scaler.transform(X_df)
                pred = clf.predict(X_scaled)
                label = le.inverse_transform(pred)[0]

                latest_prediction = {
                    "T1": 35, "T2": 35, "T3": 35, "T4": 35,
                    "T5": 35, "T6": 35, "T7": 35, "T8": 35,
                    "Voltage": 6.2, "Current": 0.3,
                    "Rain_Analog": 450, "Rain_Digital": 1,
                    "Label": label
                }
                print(f"🔍 Predicted Class (Manual): {label}")
                ser.write((label + "\n").encode())
                time.sleep(1)
                continue

            # 🔄 Read real sensor data
            line = ser.readline().decode('utf-8').strip()
            print(f"[SERIAL RAW] {line}")

            values = line.split(',')
            if len(values) == 12:
                values = list(map(float, values))
                temps = values[0:8]
                voltage = values[8]
                current = values[9]
                rain_analog = int(values[10])
                rain_digital = int(values[11])

                features = temps + [voltage, current, rain_analog, rain_digital]
                columns = ['T1','T2','T3','T4','T5','T6','T7','T8','Voltage','Current','Rain_A','Rain_D']
                X_df = pd.DataFrame([features], columns=columns)
                X_scaled = scaler.transform(X_df)

                pred = clf.predict(X_scaled)
                label = le.inverse_transform(pred)[0]

                latest_prediction = {
                    "T1": temps[0], "T2": temps[1], "T3": temps[2], "T4": temps[3],
                    "T5": temps[4], "T6": temps[5], "T7": temps[6], "T8": temps[7],
                    "Voltage": voltage, "Current": current,
                    "Rain_Analog": rain_analog, "Rain_Digital": rain_digital,
                    "Label": label
                }

                print(f"🔍 Predicted Class: {label}")
                ser.write((label + "\n").encode())

        except Exception as e:
            print(f"[ERROR] {e}")

@app.route('/')
def index():
    return "🔋 Battery Prediction API is Running!"

@app.route('/latest')
def get_latest():
    return jsonify(latest_prediction)

# Start serial thread
threading.Thread(target=read_serial, daemon=True).start()

if __name__ == '__main__':
    print("✅ Listening for Arduino data...")
    app.run(host='0.0.0.0', port=5000)
