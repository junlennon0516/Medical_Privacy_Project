#include "seal/seal.h"
#include <iostream>

using namespace std;
using namespace seal;

int main() {

    cout << "Start" << endl;

    EncryptionParameters parms(scheme_type::bfv);            
    // bfv 동형암호를 쓸 것이니 파라미터를 준비해라
    size_t poly_modulus_degree = 4096;                       
    // 한 암호문에 최대 4096개까지의 독립된 메시지를 암호활 수 있다. 2의 지수승으로 조정 가능, 작을수록 빠른 연산 제공

    // 위 설정대로 기타 파라미터 설정
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(poly_modulus_degree));         // 디폴트로 4096에 맞도록 modulus 크기 정해준다. modulus는 암호문을 이루는 각 숫자가 어느 모듈로 정수 값을 지니는가를 의미
    parms.set_plain_modulus(PlainModulus::Batching(poly_modulus_degree, 20));       // 20은 평문이 모듈로 20비트 (약 2^20 크기의) 정수임을 의미; 즉 정수로 이루어진 평문 간의 연산 결과가 2^20을 넘지 않는 한 정수 연산이 잘 수행된다. 그렇지 않을 경우 2^20 크기의 정수로 나눈 나머지가 출력됨

    SEALContext context(parms);     // 파라미터에 맞게 여러 기본 context 설정 (수정할 필요 없음)

    KeyGenerator keygen(context);   // 키 생성
    SecretKey secret_key = keygen.secret_key();  // 비밀키 생성
    PublicKey public_key;
    keygen.create_public_key(public_key);       // 공개키 생성

    Encryptor encryptor(context, public_key);   // 암호화를 위한 객체 생성 
    Evaluator evaluator(context);               // 암호문에 대한 연산을 위한 객체 생성
    Decryptor decryptor(context, secret_key);   // 복호화를 위한 객체 생성
    cout << "Enc/Eval/Dec prepared!" << endl;

    BatchEncoder encoder(context);              // 여러 평문을 한 암호문에 넣기 위한 인코더 객체 생성
    size_t slot_count = encoder.slot_count();   // 한 암호문에 몇개 까지의 평문을 넣을 수 있는가를 slot_count 변수에 저장 
    vector<uint64_t> plain_vector(slot_count, 0ULL);    // slot_count 개수의 원소를 지니는 0으로 이루어진 벡터 생성
    plain_vector[0] = 3;        // 벡터의 일부 원소에 값 할당 : 이부분은 평문이 무엇이냐에 따라 변경하시면 됩니다.
    plain_vector[1] = 5;
    cout << "Input: " << plain_vector[0] << ", " << plain_vector[1] << ", " << plain_vector[2] << endl;


    // 평문을 생성하여 인코딩 (즉 평문 메시지 벡터를 암호화하기 위해서 형태를 변경하는 것이라 이해하시면 됩니다. 정확히는 다항식으로 변경)
    Plaintext plain;
    encoder.encode(plain_vector, plain);

    // 암호문 생성 후 encrypted 라는 암호문에 위 평문을 암호화한 결과를 저장
    Ciphertext encrypted;
    encryptor.encrypt(plain, encrypted);
    cout << "Enc completed!" << endl;


    // 위 암호문과 동일한 또다른 암호문 생성 
    Ciphertext encrypted2 = encrypted;
    evaluator.add_inplace(encrypted, encrypted2); // 두 암호문을 더한 후 그 결과를 encrypted에 저장
    evaluator.multiply_inplace(encrypted, encrypted2); // encrypted에 encrypted2 곱한다.
    cout << "Ciphertext computation completed!" << endl;

    // 복호화 결과를 저장할 곳을 생성
    Plaintext plain_result;
    decryptor.decrypt(encrypted, plain_result);

    // 복호화한 평문을 다시 메시지 벡터로 디코딩, 이후 결과 출력
    vector<uint64_t> result_vector;
    encoder.decode(plain_result, result_vector);
    cout << "End!" << endl;
    cout << "Result: (input+input)*input: " << result_vector[0] << ", " << result_vector[1] << ", " << result_vector[2] << endl;

    return 0;
}



