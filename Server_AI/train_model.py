import sys
import io

sys.stdout = io.TextIOWrapper(sys.stdout.detach(), encoding = 'utf-8')
sys.stderr = io.TextIOWrapper(sys.stderr.detach(), encoding = 'utf-8')

import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.linear_model import LinearRegression
from sklearn.preprocessing import MinMaxScaler
from sklearn.metrics import mean_squared_error

# 1. 데이터 로드
try:
    df = pd.read_csv('./Server_AI/heart_cleveland.csv')
    print("데이터 로드 성공")
except FileNotFoundError:
    print("데이터 파일이 없습니다.")
    sys.exit(1)
    

# 2. 학습에 사용할 특성(Feature) 선택
# 나이, 혈압, 콜레스테롤, 최대심박수
features = ['age', 'trestbps', 'chol', 'thalach']
x = df[features]
y = df['condition']

# 3. 데이터 정규화 (Normalization)
# 0~1 사이로 정규화
scaler = MinMaxScaler()
X_scaled = scaler.fit_transform(x)

# 4. 데이터 분할 (학습용 / 테스트용)
X_train, X_test, Y_train, Y_test = train_test_split(X_scaled, y, test_size=0.2, random_state=42)

# 5. 모델 학습 (Linear Regression)
model = LinearRegression()
model.fit(X_train, Y_train)

weights = model.coef_
bias = model.intercept_

print("\n==학습완료==")
print(f"선택된 feature: {features}")
print(f"학습된 가중치(W): {weights}")
print(f"학습된 편형(b): {bias}")

# 예측 테스트
score = model.score(X_test, Y_test)
print(f"테스트 세트 정확도: {score:.4f}")

# 파일로 저장
np.savetxt('weights.txt', weights, fmt='%.6f', newline=' ')

with open('bias.txt', 'w') as f:
    f.write(f"{bias:.6f}")

print("file saved.")