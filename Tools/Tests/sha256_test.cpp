// Standalone check of the plugin's SHA-256 against published vectors and (optionally) hex
// digests supplied on stdin as "<hex-input> <hex-digest>" lines.
//   g++ -std=c++17 -I ../../Source/JiweSDK/Private sha256_test.cpp -o sha256_test && ./sha256_test
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
typedef uint8_t uint8;
typedef uint32_t uint32;
typedef uint64_t uint64;
#define JIWE_SHA256_STANDALONE 1
#include "JiweSha256.h"

static std::string Hex(const uint8* D, int N)
{
	static const char* X = "0123456789abcdef";
	std::string S;
	for (int I = 0; I < N; ++I) { S += X[D[I] >> 4]; S += X[D[I] & 15]; }
	return S;
}

static std::vector<uint8> FromHex(const std::string& S)
{
	std::vector<uint8> V;
	for (size_t I = 0; I + 1 < S.size(); I += 2) V.push_back((uint8)std::stoi(S.substr(I, 2), nullptr, 16));
	return V;
}

int main(int argc, char** argv)
{
	int Failures = 0, Checked = 0;
	auto Check = [&](const std::vector<uint8>& In, const std::string& Expected)
	{
		uint8 Out[32];
		static const uint8 Empty = 0;
		JiweSha256(In.empty() ? &Empty : In.data(), In.size(), Out);
		++Checked;
		if (Hex(Out, 32) != Expected) { ++Failures; std::printf("FAIL len=%zu got=%s want=%s\n", In.size(), Hex(Out, 32).c_str(), Expected.c_str()); }
	};
	auto Str = [](const char* S) { return std::vector<uint8>(S, S + std::strlen(S)); };

	// FIPS 180-4 / NIST vectors
	Check(Str(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	Check(Str("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	Check(Str("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
	// RFC 7636 appendix B: the PKCE example verifier (challenge is the base64url of this digest)
	Check(Str("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"), "13d31e961a1ad8ec2f16b10c4c982e0876a878ad6df144566ee1894acb70f9c3");

	if (argc > 1 && std::string(argv[1]) == "--stdin")
	{
		std::string In, Digest;
		while (std::cin >> In >> Digest) Check(FromHex(In == "-" ? "" : In), Digest);
	}
	std::printf("%d checked, %d failed\n", Checked, Failures);
	return Failures ? 1 : 0;
}
