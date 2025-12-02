#include "seal/seal.h"
#include <iostream>
#include <fstream>	
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <algorithm>

using namespace std;
using namespace seal;
namespace fs = std::filesystem;

int main() {
    cout << "==================================================" << "\n";
    cout << "Current Working Directory: " << fs::current_path() << "\n";
    cout << "Please place 'weights.txt' and 'bias.txt' here" << "\n";
    cout << "==================================================" << "\n\n";

    cout << "[Server] Cleaning up Shared_Channel..." << "\n";
    cout.flush();

    try {
        cout << "[Server] Initializing encryption parameters...\n";
        cout.flush();
        
        EncryptionParameters parms(scheme_type::ckks);
        // 원래 작동하던 파라미터로 복원
        size_t poly_modulus_degree = 8192;
        parms.set_poly_modulus_degree(poly_modulus_degree);
        parms.set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree, { 60, 40, 40, 60 }));

        cout << "[Server] Creating SEAL context...\n";
        cout.flush();
        SEALContext context(parms);
        
        cout << "[Server] Creating evaluator and encoder...\n";
        cout.flush();
        Evaluator evaluator(context);
        CKKSEncoder encoder(context);

        cout << "[Server] AI Server is running... Waiting for KEY...." << "\n";
        cout.flush();

        PublicKey public_key;
        SecretKey secret_key;

        while (true) {
            ifstream req_file("Shared_Channel/request.txt");

        if (req_file.good()) {
            try {
                // 키 로딩
                ifstream pk_file("Shared_Channel/pub_key.dat", ios::binary);
                ifstream sk_file("Shared_Channel/secret_key.dat", ios::binary);

                if (pk_file.good() && sk_file.good()) {
                    public_key.load(context, pk_file);
                    secret_key.load(context, sk_file);
                    pk_file.close();
                    sk_file.close();
                    cout << "\n[Server] New Keys Loaded successfully!" << "\n";
                }
                else {
                    cout << "[Server] Error: Request exists but Keys are missing.\n";
                    req_file.close();
                    this_thread::sleep_for(chrono::seconds(1));
                    continue;
                }

                cout << "[Server] !! Request data detected. !! Processing..." << "\n";

                // --- 1. Weights & Bias 파일 읽기 ---

                // Weights 읽기
                ifstream w_file("./weights.txt");
                if (!w_file.is_open()) throw runtime_error("weights.txt not found!");
                vector<double> weights;
                double temp;
                while (w_file >> temp) weights.push_back(temp);
                w_file.close();

                // Bias 읽기
                ifstream b_file("./bias.txt");
                if (!b_file.is_open()) throw runtime_error("bias.txt not found!");
                double bias;
                b_file >> bias;
                b_file.close();

                cout << "[Server] Model Loaded. Bias: " << bias << "\n";

                // --- 2. 평문 데이터 로딩 및 암호화 ---
                cout << "[Server] Loading input data from request.txt...\n";
                vector<double> input_data;
                double temp_val;
                while (req_file >> temp_val) {
                    input_data.push_back(temp_val);
                }
                req_file.close();

                if (input_data.size() != weights.size()) {
                    throw runtime_error("Input data size mismatch with weights!");
                }

                cout << "[Server] Input data loaded: " << input_data.size() << " values\n";
                cout << "[Server] Starting encryption process...\n";

                // --- 비트 인코딩: 10진수 값을 정수로 변환하여 비트 단위로 저장 ---
                // 정규화된 값(0.0~1.0)을 정수로 변환 (예: 0.123 → 1230, 스케일 10000)
                const int bit_scale = 10000; // 10진수 4자리 정밀도
                vector<double> bit_encoded_data;
                for (double val : input_data) {
                    // 0.0~1.0 범위를 0~10000 정수로 변환
                    int int_val = static_cast<int>(val * bit_scale);
                    // 정수 값을 다시 double로 변환 (CKKS는 실수 연산)
                    bit_encoded_data.push_back(static_cast<double>(int_val));
                }

                cout << "[Server] Data bit-encoded: ";
                for (size_t i = 0; i < bit_encoded_data.size(); i++) {
                    cout << bit_encoded_data[i];
                    if (i < bit_encoded_data.size() - 1) cout << ", ";
                }
                cout << "\n";

                // 암호화 수행 (scale 줄임: 2^30)
                double scale = pow(2.0, 30);
                Encryptor encryptor(context, public_key);
                Plaintext plain_input;
                encoder.encode(bit_encoded_data, scale, plain_input);
                Ciphertext encrypted_input;
                encryptor.encrypt(plain_input, encrypted_input);

                cout << "[Server] Data encrypted successfully.\n";

                // --- 암호문 정보 추출 및 저장 (시각화용) ---
                // 현재 작업 디렉토리 확인 및 Shared_Channel 경로 구성
                fs::path current_dir = fs::current_path();
                fs::path shared_channel = current_dir / "Shared_Channel";
                fs::path cipher_info_path = shared_channel / "ciphertext_info.txt";
                fs::path cipher_size_path = shared_channel / "ciphertext_size.txt";
                fs::path cipher_binary_path = shared_channel / "ciphertext_binary.dat";
                
                cout << "[Server] Current directory: " << current_dir << "\n";
                cout << "[Server] Shared_Channel path: " << shared_channel << "\n";
                cout << "[Server] Saving ciphertext info to: " << cipher_info_path << "\n";
                
                // Shared_Channel 디렉토리가 없으면 생성
                if (!fs::exists(shared_channel)) {
                    fs::create_directories(shared_channel);
                    cout << "[Server] Created Shared_Channel directory.\n";
                }
                
                try {
                    // 1. 암호문 메타데이터 저장
                    ofstream cipher_info(cipher_info_path.string());
                    if (!cipher_info.is_open()) {
                        cout << "[Server] Warning: Failed to open ciphertext_info.txt\n";
                    } else {
                        cipher_info << "Ciphertext Information\n";
                        cipher_info << "=====================\n";
                        cipher_info << "Size (polynomials): " << encrypted_input.size() << "\n";
                        cipher_info << "Poly Modulus Degree: " << poly_modulus_degree << "\n";
                        cipher_info << "Coeff Modulus Size: " << encrypted_input.coeff_modulus_size() << "\n";
                        cipher_info << "Scale: " << encrypted_input.scale() << "\n";
                        
                        // 2. 암호문의 일부 계수 추출 (처음 100개)
                        size_t sample_count = min(static_cast<size_t>(100), 
                            static_cast<size_t>(encrypted_input.size() * poly_modulus_degree * encrypted_input.coeff_modulus_size()));
                        cipher_info << "\nSample Coefficients (first " << sample_count << "):\n";
                        
                        const auto* cipher_data = encrypted_input.data();
                        for (size_t i = 0; i < sample_count; i++) {
                            cipher_info << cipher_data[i];
                            if (i < sample_count - 1) cipher_info << " ";
                        }
                        cipher_info << "\n";
                        cipher_info.flush();
                        cipher_info.close();
                        cout << "[Server] Ciphertext info saved.\n";
                    }

                    // 3. 암호문 바이너리 크기 정보 저장
                    // 암호문을 메모리에 저장하여 크기 계산
                    stringstream cipher_stream;
                    encrypted_input.save(cipher_stream);
                    size_t cipher_size = cipher_stream.tellp();
                    
                    ofstream cipher_size_info(cipher_size_path.string());
                    if (!cipher_size_info.is_open()) {
                        cout << "[Server] Warning: Failed to open " << cipher_size_path << "\n";
                    } else {
                        cipher_size_info << cipher_size << "\n";
                        cipher_size_info.flush();
                        cipher_size_info.close();
                        cout << "[Server] Ciphertext size saved: " << cipher_size << " bytes to " << cipher_size_path << "\n";
                    }

                    // 4. 암호문 바이너리 저장 (이미지 변환용)
                    ofstream cipher_binary(cipher_binary_path.string(), ios::binary);
                    if (!cipher_binary.is_open()) {
                        cout << "[Server] Warning: Failed to open " << cipher_binary_path << "\n";
                    } else {
                        encrypted_input.save(cipher_binary);
                        cipher_binary.flush();
                        cipher_binary.close();
                        cout << "[Server] Ciphertext binary saved to " << cipher_binary_path << "\n";
                        
                        // 파일이 실제로 생성되었는지 확인
                        if (fs::exists(cipher_binary_path)) {
                            auto file_size = fs::file_size(cipher_binary_path);
                            cout << "[Server] Verified: Binary file exists, size = " << file_size << " bytes\n";
                        } else {
                            cout << "[Server] Error: Binary file was not created!\n";
                        }
                    }
                }
                catch (const exception& e) {
                    cout << "[Server] Error saving ciphertext info: " << e.what() << "\n";
                }

                // --- 3. 동형암호 연산 (Prediction) ---

                // 가중치도 비트 스케일에 맞춰 조정
                vector<double> bit_encoded_weights;
                for (double w : weights) {
                    bit_encoded_weights.push_back(w * bit_scale);
                }

                // [Step 1] W * x (가중치 곱하기)
                Plaintext plain_weights;
                encoder.encode(bit_encoded_weights, encrypted_input.parms_id(), encrypted_input.scale(), plain_weights);

                evaluator.multiply_plain_inplace(encrypted_input, plain_weights);

                // [Step 2] Rescaling 제거 (coeff_modulus가 2개만 있으므로 rescale 불필요)

                // [Step 3] Bias 더하기 (비트 스케일에 맞춰 조정)
                // 단순히 bias를 더하면 [v1+b, v2+b, v3+b...]가 되어 합산 시 4b가 됨.
                // 따라서 [b, 0, 0, 0...] 벡터를 만들어 더해줌.
                // 그러면 [v1+b, v2, v3...]가 되고, 합산하면 Total + b가 되어 정확해짐.

                vector<double> bias_vec(weights.size(), 0.0); // 0으로 초기화된 벡터
                bias_vec[0] = bias * bit_scale * bit_scale; // bias도 비트 스케일 적용 (W*x 결과에 맞춤)

                Plaintext plain_bias;
                // 현재 encrypted_input의 scale에 맞춰서 인코딩해야 덧셈 가능
                encoder.encode(bias_vec, encrypted_input.parms_id(), encrypted_input.scale(), plain_bias);

                evaluator.add_plain_inplace(encrypted_input, plain_bias);

                cout << "[Server] Prediction (Wx + b) computed securely." << "\n";

                // --- 4. 복호화 및 최종 점수 계산 ---
                Decryptor decryptor(context, secret_key);
                Plaintext plain_result;
                decryptor.decrypt(encrypted_input, plain_result);
                vector<double> result_vec;
                encoder.decode(plain_result, result_vec);

                // 최종 점수 합산 (서버에서 모든 연산 완료)
                // 비트 인코딩된 값을 원래 스케일로 복원
                double final_score = 0.0;
                for (size_t i = 0; i < result_vec.size() && i < input_data.size(); i++) {
                    final_score += result_vec[i];
                }
                // 비트 스케일로 나누어 원래 값으로 복원
                final_score = final_score / (bit_scale * bit_scale);

                cout << "[Server] Final score computed: " << final_score << "\n";

                // --- 5. 평문 결과 전송 ---
                ofstream resp_file("Shared_Channel/response.txt");
                resp_file << final_score;
                resp_file.close();

                fs::remove("Shared_Channel/request.txt");
                cout << "[Server] Result sent. Standby." << "\n";

            }
            catch (const std::exception& e) {
                cerr << "[SERVER ERROR] " << e.what() << "\n";
                req_file.close();
                fs::remove("Shared_Channel/request.ckks");
            }
        }
        else {
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        }
    }
    catch (const exception& e) {
        cerr << "[SERVER FATAL ERROR] " << e.what() << "\n";
        cerr.flush();
        return 1;
    }
    catch (...) {
        cerr << "[SERVER FATAL ERROR] Unknown exception occurred during initialization!\n";
        cerr.flush();
        return 1;
    }
    
    return 0;
}