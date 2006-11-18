#ifndef __AES_H__
#define __AES_H__

#include <cstring>
#include "inspircd_config.h"
#include "base.h"

using namespace std;

/** The AES class is a utility class for use in modules and the core for encryption of data.
 */
class AES : public classbase
{
public:
	enum { ECB=0, CBC=1, CFB=2 };

private:
	enum { DEFAULT_BLOCK_SIZE=16 };
	enum { MAX_BLOCK_SIZE=32, MAX_ROUNDS=14, MAX_KC=8, MAX_BC=8 };

	static int Mul(int a, int b)
	{
		return (a != 0 && b != 0) ? sm_alog[(sm_log[a & 0xFF] + sm_log[b & 0xFF]) % 255] : 0;
	}

	/** Convenience method used in generating Transposition Boxes
	 */
	static int Mul4(int a, char b[])
	{
		if(a == 0)
			return 0;
		a = sm_log[a & 0xFF];
		int a0 = (b[0] != 0) ? sm_alog[(a + sm_log[b[0] & 0xFF]) % 255] & 0xFF : 0;
		int a1 = (b[1] != 0) ? sm_alog[(a + sm_log[b[1] & 0xFF]) % 255] & 0xFF : 0;
		int a2 = (b[2] != 0) ? sm_alog[(a + sm_log[b[2] & 0xFF]) % 255] & 0xFF : 0;
		int a3 = (b[3] != 0) ? sm_alog[(a + sm_log[b[3] & 0xFF]) % 255] & 0xFF : 0;
		return a0 << 24 | a1 << 16 | a2 << 8 | a3;
	}

public:
	AES();

	virtual ~AES();

	/** Expand a user-supplied key material into a session key.
	 * 
	 * @param key The 128/192/256-bit user-key to use.
	 * @param chain Initial chain block for CBC and CFB modes.
	 * @param keylength 16, 24 or 32 bytes
	 * @param blockSize The block size in bytes of this Rijndael (16, 24 or 32 bytes).
	 */
	void MakeKey(char const* key, char const* chain, int keylength=DEFAULT_BLOCK_SIZE, int blockSize=DEFAULT_BLOCK_SIZE);

private:
	/** Auxiliary Function
	 */
	void Xor(char* buff, char const* chain)
	{
		if(false==m_bKeyInit)
			return;
		for(int i=0; i<m_blockSize; i++)
			*(buff++) ^= *(chain++);	
	}

	/** Convenience method to encrypt exactly one block of plaintext, assuming Rijndael's default block size (128-bit).
	 * @param in The plaintext
	 * @param result The ciphertext generated from a plaintext using the key
	 */
	void DefEncryptBlock(char const* in, char* result);

	/** Convenience method to decrypt exactly one block of plaintext, assuming Rijndael's default block size (128-bit).
	 * @param in The ciphertext.
	 * @param result The plaintext generated from a ciphertext using the session key.
	 */
	void DefDecryptBlock(char const* in, char* result);

public:
	/** Encrypt exactly one block of plaintext.
	 * @param in The plaintext.
	 * @param result The ciphertext generated from a plaintext using the key.
	 */
	void EncryptBlock(char const* in, char* result);
	
	/** Decrypt exactly one block of ciphertext.
	 * @param in The ciphertext.
	 * @param result The plaintext generated from a ciphertext using the session key.
	 */
	void DecryptBlock(char const* in, char* result);

	/** Encrypt multiple blocks of plaintext.
	 * @param n Number of bytes to encrypt, must be a multiple of the keysize
	 * @param in The plaintext to encrypt
	 * @param result The output ciphertext
	 * @param iMode Mode to use
	 */
	void Encrypt(char const* in, char* result, size_t n, int iMode=ECB);
	
	/** Decrypt multiple blocks of ciphertext.
	 * @param n Number of bytes to decrypt, must be a multiple of the keysize
	 * @param in The ciphertext to decrypt
	 * @param result The output plaintext
	 * @param iMode Mode to use
	 */
	void Decrypt(char const* in, char* result, size_t n, int iMode=ECB);

	/** Get Key Length
	 */
	int GetKeyLength()
	{
		if(false==m_bKeyInit)
			return 0;
		return m_keylength;
	}

	/** Get Block Size
	 */
	int GetBlockSize()
	{
		if(false==m_bKeyInit)
			return 0;
		return m_blockSize;
	}
	
	/** Get Number of Rounds
	 */
	int GetRounds()
	{
		if(false==m_bKeyInit)
			return 0;
		return m_iROUNDS;
	}

	/** Reset the chain
	 */
	void ResetChain()
	{
		memcpy(m_chain, m_chain0, m_blockSize);
	}

public:
	/** Null chain
	 */
	static char const* sm_chain0;

private:
	static const int sm_alog[256];
	static const int sm_log[256];
	static const char sm_S[256];
	static const char sm_Si[256];
	static const int sm_T1[256];
	static const int sm_T2[256];
	static const int sm_T3[256];
	static const int sm_T4[256];
	static const int sm_T5[256];
	static const int sm_T6[256];
	static const int sm_T7[256];
	static const int sm_T8[256];
	static const int sm_U1[256];
	static const int sm_U2[256];
	static const int sm_U3[256];
	static const int sm_U4[256];
	static const char sm_rcon[30];
	static const int sm_shifts[3][4][2];
	/** Key Initialization Flag
	 */
	bool m_bKeyInit;
	/** Encryption (m_Ke) round key
	 */
	int m_Ke[MAX_ROUNDS+1][MAX_BC];
	/** Decryption (m_Kd) round key
	 */
	int m_Kd[MAX_ROUNDS+1][MAX_BC];
	/** Key Length
	 */
	int m_keylength;
	/** Block Size
	 */
	int	m_blockSize;
	/** Number of Rounds
	 */
	int m_iROUNDS;
	/**Chain Block
	 */
	char m_chain0[MAX_BLOCK_SIZE];
	char m_chain[MAX_BLOCK_SIZE];
	/** Auxiliary private use buffers
	 */
	int tk[MAX_KC];
	int a[MAX_BC];
	int t[MAX_BC];
};

#endif

/** Convert from binary to base64
 * @param out Output
 * @param in Input
 * @param inlen Number of bytes in input buffer
 */

void to64frombits(unsigned char *out, const unsigned char *in, int inlen);
/** Convert from base64 to binary
 * @out Output
 * @in Input
 * @maxlen Size of output buffer
 * @return Number of bytes actually converted
 */
int from64tobits(char *out, const char *in, int maxlen);

