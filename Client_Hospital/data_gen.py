import random

# 저장할 파일 경로 (C++ client가 읽을 위치)
OUTPUT_FILE = "raw_data.txt"

def normalize_data(data):
    """
    MinMaxScaler를 사용하여 0.0~1.0 사이로 정규화
    train_model.py와 동일한 범위 사용
    """
    # 원본 데이터 범위 (min, max)
    ranges = [
        (29, 77),      # age: 29-77
        (94, 200),     # trestbps: 94-200
        (126, 564),    # chol: 126-564
        (71, 202),     # thalach: 71-202
    ]
    
    normalized = []
    for i, val in enumerate(data):
        min_val, max_val = ranges[i]
        # MinMaxScaler 공식: (x - min) / (max - min)
        normalized_val = (val - min_val) / (max_val - min_val)
        normalized.append(normalized_val)
    
    return normalized

def generate_data():
    print("--- [python] Generate Patient Data ... ---")
    
    # 가상 데이터: [나이, 수축기혈압, 콜레스테롤, 심박수]
    # heart_cleveland.csv 데이터 범위를 참고하여 랜덤 생성
    # age: 29-77, trestbps: 94-200, chol: 126-564, thalach: 71-202
    raw_data = [
        random.randint(29, 77),      # 나이 (age)
        random.randint(94, 200),     # 수축기혈압 (trestbps)
        random.randint(126, 564),    # 콜레스테롤 (chol)
        random.randint(71, 202),     # 최대 심박수 (thalach)
    ]
    
    # 0.0~1.0 사이로 정규화
    normalized_data = normalize_data(raw_data)
    
    with open(OUTPUT_FILE, "w") as f:
        for val in normalized_data:
            f.write(f"{val:.6f}\n")
            
    print(f"--- [python] Raw Data: {raw_data} ---")
    print(f"--- [python] Normalized Data (0.0~1.0): {[f'{v:.6f}' for v in normalized_data]} ---")
    print(f"--- [python] Data Saved Completed ---")
    print(f"Saved to {OUTPUT_FILE}")
    
if __name__ == "__main__":
    generate_data()