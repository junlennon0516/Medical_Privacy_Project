import streamlit as st
import subprocess
import os
import time
import numpy as np
from PIL import Image
import math

CLIENT_EXE_PATH = r"x64\Release\Client_Hospital.exe" 

RAW_DATA_PATH = "raw_data.txt"
RESULT_PATH = r"Client_Hospital\result.txt"
SHARED_PATH = "Shared_Channel"

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


def visualize_ciphertext_binary(binary_path, width=256, height=256):
    """
    암호문 바이너리 파일을 이미지로 변환
    바이너리 데이터를 픽셀 값으로 매핑하여 시각화
    """
    try:
        with open(binary_path, 'rb') as f:
            binary_data = f.read()
        
        if len(binary_data) == 0:
            return None
        
        # 바이너리 데이터를 numpy 배열로 변환
        data_array = np.frombuffer(binary_data, dtype=np.uint8)
        
        # 이미지 크기에 맞게 조정
        total_pixels = width * height
        if len(data_array) < total_pixels:
            # 데이터가 부족하면 반복
            repeat_count = (total_pixels // len(data_array)) + 1
            data_array = np.tile(data_array, repeat_count)
        
        # 크기 조정
        data_array = data_array[:total_pixels]
        
        # 2D 배열로 변환
        image_data = data_array.reshape((height, width))
        
        # 이미지 생성
        img = Image.fromarray(image_data, mode='L')
        
        # 컬러맵 적용 (더 시각적으로 보기 좋게)
        img_color = img.convert('RGB')
        img_array = np.array(img_color)
        
        # 히트맵 스타일로 변환 (파란색 → 빨간색)
        normalized = image_data.astype(np.float32) / 255.0
        
        # RGB 채널 생성
        r_channel = (normalized * 255).astype(np.uint8)
        g_channel = ((1 - normalized) * 255).astype(np.uint8)
        b_channel = (128 * np.ones_like(normalized)).astype(np.uint8)
        
        img_colored = np.stack([r_channel, g_channel, b_channel], axis=2)
        img_colored = Image.fromarray(img_colored, mode='RGB')
        
        return img_colored
    
    except Exception as e:
        st.error(f"이미지 변환 오류: {e}")
        return None


def load_ciphertext_info():
    """암호문 정보 파일 읽기"""
    info_path = os.path.join(SHARED_PATH, "ciphertext_info.txt")
    size_path = os.path.join(SHARED_PATH, "ciphertext_size.txt")
    binary_path = os.path.join(SHARED_PATH, "ciphertext_binary.dat")
    
    info = {}
    
    # 디버깅: 파일 존재 여부 확인
    info['debug'] = {
        'info_exists': os.path.exists(info_path),
        'size_exists': os.path.exists(size_path),
        'binary_exists': os.path.exists(binary_path),
        'shared_channel_exists': os.path.exists(SHARED_PATH)
    }
    
    # Shared_Channel 디렉토리의 파일 목록
    if os.path.exists(SHARED_PATH):
        try:
            files = os.listdir(SHARED_PATH)
            info['debug']['files_in_channel'] = files
        except:
            info['debug']['files_in_channel'] = []
    
    if os.path.exists(info_path):
        with open(info_path, 'r') as f:
            info['text'] = f.read()
    
    if os.path.exists(size_path):
        with open(size_path, 'r') as f:
            size_str = f.read().strip()
            try:
                info['size_bytes'] = int(size_str)
                info['size_kb'] = info['size_bytes'] / 1024
                info['size_mb'] = info['size_kb'] / 1024
            except:
                info['size_bytes'] = 0
    
    return info


# --- 웹 페이지 설정 ---
st.set_page_config(page_title="Privacy-Preserving AI", layout="wide")

st.title("🏥 프라이버시 보존형 AI 의료 진단 시스템")
st.markdown("### Homomorphic Encryption (CKKS) based Heart Disease Prediction")
st.write("환자의 데이터는 **동형암호화**되어 서버로 전송되며, 서버는 내용을 볼 수 없습니다.")

col1, col2 = st.columns([1, 1])

# --- [왼쪽] 병원: 환자 정보 입력 ---
with col1:
    st.header("1. 환자 데이터 입력 (Hospital)")
    with st.container(border=True):
        # 정규화 범위에 맞춘 입력 범위 (train_model.py와 동일)
        age = st.slider("나이 (Age) [범위: 29-77]", 29, 77, 50)
        bp = st.number_input("혈압 (Blood Pressure) [범위: 94-200]", 94, 200, 120)
        chol = st.number_input("콜레스테롤 (Cholesterol) [범위: 126-564]", 126, 564, 200)
        thalach = st.slider("최대 심박수 (Maximum Heart Rate) [범위: 71-202]", 71, 202, 150)
        
        if st.button("🔒 암호화 진단 요청 (Run Secure AI)", use_container_width=True):
            # 1. 기존 결과 파일 삭제 (초기화)
            if os.path.exists(RESULT_PATH):
                os.remove(RESULT_PATH)

            # 2. 입력값을 정규화하여 파일로 저장
            # 원본 데이터: [나이, 혈압, 콜레스테롤, 최대심박수]
            raw_data = [age, bp, chol, thalach]
            
            # 0.0~1.0 사이로 정규화 (train_model.py와 동일한 방식)
            normalized_data = normalize_data(raw_data)
            
            # 정규화된 데이터를 파일로 저장 (client_main.cpp가 읽을 위치)
            # 정확히 4개의 값만 저장 (마지막 빈 줄 없음)
            with open(RAW_DATA_PATH, "w") as f:
                for i, val in enumerate(normalized_data):
                    f.write(f"{val:.6f}")
                    if i < len(normalized_data) - 1:
                        f.write("\n")

            st.info(f"📝 원본 데이터: 나이={age}, 혈압={bp}, 콜레스테롤={chol}, 심박수={thalach}")
            st.info(f"📊 정규화된 데이터: {[f'{v:.6f}' for v in normalized_data]}")
            st.info("🔐 암호화 엔진(C++)을 실행합니다...")
            
            # 3. C++ Client 실행
            try:
                if not os.path.exists(CLIENT_EXE_PATH):
                    st.error(f"실행 파일을 찾을 수 없습니다: {CLIENT_EXE_PATH}")
                else:
                    # cwd=os.getcwd() : 현재 app.py가 있는 폴더를 기준(Root)으로 실행
                    process = subprocess.Popen([CLIENT_EXE_PATH], cwd=os.getcwd())
                    
                    # C++이 종료될 때까지 대기 (최대 30초)
                    max_wait_time = 30
                    elapsed_time = 0
                    with st.spinner('서버(Cloud)와 동형암호 통신 중...'):
                        while process.poll() is None and elapsed_time < max_wait_time:
                            time.sleep(0.5)
                            elapsed_time += 0.5
                    
                    # 프로세스가 아직 실행 중이면 타임아웃
                    if process.poll() is None:
                        process.terminate()
                        st.error(f"⏱️ 타임아웃: 서버 응답을 {max_wait_time}초 내에 받지 못했습니다.")
                        st.info("💡 서버(Server_AI.exe)가 실행 중인지 확인해주세요.")
                    else:
                        # 프로세스 종료 코드 확인
                        return_code = process.returncode
                        if return_code != 0:
                            st.warning(f"⚠️ 클라이언트가 비정상 종료되었습니다. (종료 코드: {return_code})")
                            st.info("💡 콘솔 출력을 확인하여 오류를 확인해주세요.")
                        
                        # 4. 결과 파일 읽기
                        if os.path.exists(RESULT_PATH):
                            with open(RESULT_PATH, "r") as f:
                                score_str = f.read().strip()
                            
                            if not score_str:
                                st.error("결과 파일이 비어있습니다.")
                            else:
                                try:
                                    score = float(score_str)
                                    st.success("진단 완료!")
                                    st.metric(label="AI 예측 심장질환 위험도", value=f"{score*100:.2f}%")
                                    
                                    if score > 0.7:
                                        st.error("⚠️ 고위험군입니다. 정밀 검사가 필요합니다.")
                                    else:
                                        st.balloons()
                                        st.success("✅ 정상 범위입니다.")
                                    
                                    # --- 암호문 시각화 섹션 추가 ---
                                    st.divider()
                                    st.subheader("🔐 암호화된 데이터 시각화")
                                    
                                    # 암호문 정보 로드
                                    cipher_info = load_ciphertext_info()
                                    
                                    if cipher_info:
                                        # 1. 암호문 정보 텍스트 표시
                                        with st.expander("📊 암호문 정보 (숫자)", expanded=True):
                                            if 'text' in cipher_info:
                                                st.text(cipher_info['text'])
                                            
                                            if 'size_bytes' in cipher_info and cipher_info['size_bytes'] > 0:
                                                col_size1, col_size2, col_size3 = st.columns(3)
                                                with col_size1:
                                                    st.metric("암호문 크기 (Bytes)", f"{cipher_info['size_bytes']:,}")
                                                with col_size2:
                                                    st.metric("암호문 크기 (KB)", f"{cipher_info['size_kb']:.2f}")
                                                with col_size3:
                                                    st.metric("암호문 크기 (MB)", f"{cipher_info['size_mb']:.4f}")
                                        
                                        # 2. 암호문 바이너리를 이미지로 시각화
                                        binary_path = os.path.join(SHARED_PATH, "ciphertext_binary.dat")
                                        if os.path.exists(binary_path):
                                            st.subheader("🎨 암호문 바이너리 시각화 (픽셀 이미지)")
                                            st.caption("암호문의 바이너리 데이터를 픽셀 값으로 변환하여 색상으로 표현합니다.")
                                            
                                            # 이미지 크기 선택
                                            img_size = st.selectbox("이미지 크기 선택", [128, 256, 512], index=1)
                                            
                                            cipher_img = visualize_ciphertext_binary(binary_path, width=img_size, height=img_size)
                                            
                                            if cipher_img:
                                                st.image(cipher_img, caption=f"암호문 바이너리 데이터 ({img_size}x{img_size} 픽셀)", use_container_width=True)
                                                st.caption("💡 각 픽셀의 색상은 암호문 바이너리 데이터의 바이트 값을 나타냅니다.")
                                            else:
                                                st.warning("이미지 변환에 실패했습니다.")
                                    else:
                                        st.info("암호문 정보를 찾을 수 없습니다. 서버가 암호화를 완료했는지 확인해주세요.")
                                    
                                except ValueError:
                                    st.error(f"결과 파일 형식이 잘못되었습니다: {score_str}")
                        else:
                            st.error("오류: 결과 파일(result.txt)이 생성되지 않았습니다.")
                            st.info("💡 다음을 확인해주세요:")
                            st.info("1. 서버(Server_AI.exe)가 실행 중인지 확인")
                            st.info("2. Shared_Channel 폴더가 존재하는지 확인")
                            st.info("3. weights.txt와 bias.txt 파일이 루트 디렉토리에 있는지 확인")

            except Exception as e:
                st.error(f"실행 오류: {e}")

# --- [오른쪽] 서버 상태 모니터링 ---
with col2:
    st.header("2. AI 서버 상태 (Cloud)")
    
    # 서버 폴더 감시
    req_file = os.path.join(SHARED_PATH, "request.ckks")
    res_file = os.path.join(SHARED_PATH, "response.ckks")
    
    if os.path.exists(req_file):
        st.warning("📡 [DETECTED] 암호화된 요청이 감지되었습니다!")
        st.code("Processing Homomorphic Encryption...\nEvaluating Polynomials...", language="bash")
    elif os.path.exists(res_file):
        st.success("✅ [SENT] 연산 결과가 병원으로 전송되었습니다.")
    else:
        st.info("💤 서버 대기 중 (Waiting for request)...")
        
    st.image("./heart_attack.jpg", caption="Heart Diesease")