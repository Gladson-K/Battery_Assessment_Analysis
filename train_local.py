### ✅ FILE 1: train_local.py
# This script trains the model from your Excel file and saves it as a .pkl

import pandas as pd
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.ensemble import RandomForestClassifier
import joblib

df = pd.read_csv("battery_dataset_real_world_mixed.csv")


# Encode labels
le = LabelEncoder()
df['Label'] = le.fit_transform(df['Label'])

# Split features and labels
X = df.drop('Label', axis=1)
y = df['Label']

# Scale features
scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

# Train model
clf = RandomForestClassifier(n_estimators=100, max_depth=10, random_state=42)
clf.fit(X_scaled, y)

# Save model
joblib.dump((scaler, clf, le), "battery_model_real_world.pkl")
print("✅ Model saved locally")

