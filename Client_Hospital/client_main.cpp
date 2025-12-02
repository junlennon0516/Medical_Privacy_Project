#include "seal/seal.h"
#include <iostream>
#include <fstream>	
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <string>

using namespace std;
using namespace seal;
namespace fs = std::filesystem;

int main() {
	// -- 0. 시작 전 Shared_Channel 비우기 --
	cout << "[Client] Cleaning Shared_Channel..." << "\n";
	try {
		for (const auto& entry : fs::directory_iterator("Shared_Channel"))
			fs::remove_all(entry.path());
	}
	catch (...) {} // 폴더가 비어있으면 패스

	// -- 1. CKKS Parameters 설정 (서버와 동일하게 맞춤) --
	EncryptionParameters parms(scheme_type::ckks);
	// 원래 작동하던 파라미터로 복원
	size_t poly_modulus_degree = 8192;
	parms.set_poly_modulus_degree(poly_modulus_degree);
	parms.set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree, { 60, 40, 40, 60 }));

	SEALContext context(parms);

	// -- 2. key 생성 (KeyGen) --
	cout << "[Client] Key Generation started." << "\n";
	KeyGenerator keygen(context);
	auto sercret_key = keygen.secret_key();
	PublicKey public_key;
	keygen.create_public_key(public_key);
	
	// RelinKeys 생성 (암호문끼리 곱셈을 위한 키)
	RelinKeys relin_keys;
	keygen.create_relin_keys(relin_keys);

	// -- 3. key 공유 (Upload Keys) --
	ofstream pk_file("Shared_Channel/pub_key.dat", ios::binary);
	public_key.save(pk_file);
	pk_file.close();

	cout << "[Client] Public key upload to Shared_Channel completed.\n";

	// -- 4. Secret Key도 저장 (서버에서 복호화용) --
	ofstream sk_file("Shared_Channel/secret_key.dat", ios::binary);
	sercret_key.save(sk_file);
	sk_file.close();

	cout << "[Client] Secret key upload to Shared_Channel completed.\n";
	
	// -- 5. RelinKeys 저장 (서버에서 다항식 연산용) --
	ofstream rk_file("Shared_Channel/relin_keys.dat", ios::binary);
	relin_keys.save(rk_file);
	rk_file.close();

	cout << "[Client] RelinKeys upload to Shared_Channel completed.\n";

	vector <double> input_data;
	// result_app.py에서 생성한 raw_data.txt 파일 읽기 (루트 디렉토리)
	ifstream infile("raw_data.txt");
	if (!infile.is_open()) {
		cout << "[Error] raw_data.txt 파일을 찾을 수 없습니다.\n";
		cout << "[Error] result_app.py에서 먼저 데이터를 입력해주세요.\n";
		return 1;
	}
	
	double val;
	string line;
	while (getline(infile, line)) {
		// 빈 줄 건너뛰기
		if (line.empty() || line.find_first_not_of(" \t\r\n") == string::npos) {
			continue;
		}
		// 문자열을 double로 변환
		try {
			val = stod(line);
			input_data.push_back(val);
		}
		catch (...) {
			// 변환 실패 시 건너뛰기
			continue;
		}
	}
	infile.close();

	if (input_data.empty()) {
		cout << "[Error] 데이터 파일이 비어있습니다.\n";
		cout << "[Error] result_app.py에서 올바른 데이터를 입력해주세요.\n";
		return 1;
	}
	
	// 정확히 4개의 값이 필요함 (age, trestbps, chol, thalach)
	if (input_data.size() != 4) {
		cout << "[Error] 데이터 개수가 올바르지 않습니다. (필요: 4개, 실제: " << input_data.size() << "개)\n";
		cout << "[Error] result_app.py에서 4개의 값을 입력해주세요.\n";
		return 1;
	}
	
	cout << "[Client] Loaded " << input_data.size() << " normalized values from result_app.py\n";
	cout << "[Client] Input data: ";
	for (size_t i = 0; i < input_data.size(); i++) {
		cout << input_data[i];
		if (i < input_data.size() - 1) cout << ", ";
	}
	cout << "\n";

	// -- 5. 평문 데이터를 Shared_Channel에 전송 (서버에서 암호화 및 연산 수행) --
	// 키 파일이 완전히 닫힌 후에 요청 파일 보내기
	// 파일 시스템 딜레이를 위해 0.5초 대기
	this_thread::sleep_for(chrono::milliseconds(500));

	ofstream req_file("Shared_Channel/request.txt");
	for (size_t i = 0; i < input_data.size(); i++) {
		req_file << input_data[i];
		if (i < input_data.size() - 1) req_file << "\n";
	}
	req_file.close();

	cout << "[Client] Plain data has been sent. (request.txt)" << "\n";
	cout << "[Client] Waiting for result..." << "\n";

	// -- 6. 결과 수신 대기 (Polling) - 서버에서 평문 결과 반환 --
	while (true) {
		ifstream resp_check("Shared_Channel/response.txt");
		if (resp_check.good()) {
			// 파일이 생성되었어도 쓰기 중일 수 있으니 잠시 대기
			this_thread::sleep_for(chrono::milliseconds(200));

			double final_score;
			resp_check >> final_score;
			resp_check.close();

			// 결과 출력
			cout << "[Client] Final score received: " << final_score << "\n";

			// 파일로 저장
			cout << ">>> [Client] 결과 수신 완료. 파일로 저장합니다." << endl;

			ofstream res_file("Client_Hospital/result.txt");
			res_file << final_score;
			res_file.close();

			cout << "\n>>> [Client] Result has arrived! <<<" << "\n";

			break;
		}
		else {
			this_thread::sleep_for(chrono::seconds(1));
			cout << ".";
		}
	}

	return 0;
}