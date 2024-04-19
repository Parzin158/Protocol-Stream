#include "protocol_Stream.h"

unsigned short checksum(const unsigned short* buffer, int size)
{
	unsigned int cksum = 0;
	// 将buffer数据加入cksum，当size=1时退出循环
	while (size > 1) {
		cksum += *buffer++; // 依靠size判断是否到结尾
		size -= sizeof(unsigned short);
	}
	// 当size=1时，buffer会剩下2个字节或者1个字节，此时按unsigned char类型添加
	if (size) {
		cksum += *(unsigned char*)buffer;
	}
	// 将32bit转成16bit校验和，cksum右移16位后不为0，代表高16bit有数据
	while (cksum >> 16) {
		cksum = (cksum >> 16) + (cksum & 0xffff); // 高16bit与0xffff按位与，保留高16bit信息
		// 将cksum高位16bit与低位16bit值相加
	}
	return (unsigned short)(~cksum); // cksum按位取反
}


/* ================== BinaryStream_Reader ================== */

BinaryStream_Reader::BinaryStream_Reader(const char* ptr_, size_t len_)
	: ptr(ptr_), len(len_), cur(ptr_)
{
	cur += BINARY_PACKLEN_LEN_2 + CHECKSUM_LEN;
	// cur跳过头部4字节、校验和2字节长度，开始读取二进制数据流
}

bool BinaryStream_Reader::IsEmpty() const
{
	return len <= BINARY_PACKLEN_LEN_2; 
}

bool BinaryStream_Reader::IsEnd() const
{
	assert(cur <= ptr + len);
	return cur == ptr + len;
}

size_t BinaryStream_Reader::GetSize() const
{
	return len;
}

const char* BinaryStream_Reader::GetData() const
{
	return ptr;
}

bool BinaryStream_Reader::ReadLength(size_t& len)
{
	size_t head_len;
	if (!ReadLengthWithoutOffset(head_len, len)) 
		return false;

	//cur += BINARY_PACKLEN_LEN_2;
	cur += head_len; // 跳过已读取长度的数据流
	return true;
}

bool BinaryStream_Reader::ReadLengthWithoutOffset(size_t& head_len, size_t& out_len)
{
	head_len = 0;
	const char* tmp = cur;
	char buf[5];

	for (size_t i = 0; i < sizeof(buf); ++i) {
		memcpy(buf + i, tmp, sizeof(char));
		++tmp;
		++head_len;

		if ((buf[i] & 0x80) == 0x00) 
			break; // 最高位为0，表示后续无数据
	}

	if (cur + head_len > ptr + len)
		return false;

	unsigned int value_len;
	read7BitEncoded(buf, head_len, value_len); // 对压缩的包数据还原
	out_len = value_len;

	/*
	if ( cur + BINARY_PACKLEN_LEN_2 > ptr + len ) {
		return false;
	}
	unsigned int tmp;
	memcpy(&tmp, cur, sizeof(tmp));
	outlen = ntohl(tmp);
	*/

	return true;
}

size_t BinaryStream_Reader::ReadAll(char* szBuffer, size_t iLen) const
{
	size_t iRealLen = min(iLen, len);
	memcpy(szBuffer, ptr, iRealLen);
	return iRealLen;
}

bool BinaryStream_Reader::ReadString(std::string& str, size_t max_len, size_t& out_len)
{
	size_t field_len = StrAbleSize(max_len, out_len );
	if (field_len == 0) {
		return false;
	}

	str.assign(cur, field_len);
	out_len = field_len;
	cur += out_len;

	return true;
}

bool BinaryStream_Reader::ReadCString(char* str, size_t str_len, size_t& out_len)
{
	size_t field_len = StrAbleSize(str_len, out_len);
	if (field_len == 0) {
		return false;
	}

	memcpy(str, cur, field_len);
	out_len = field_len;
	cur += out_len;

	return true;
}

bool BinaryStream_Reader::ReadCCString(const char** str, size_t max_len, size_t& out_len)
{
	size_t field_len = StrAbleSize(max_len, out_len);
	if (field_len == 0) {
		return false;
	}

	*str = cur;
	out_len = field_len;
	cur += out_len;

	return true;
}

size_t BinaryStream_Reader::StrAbleSize(size_t s_len, size_t& out_len)
{
	size_t head_len;
	size_t field_len;
	if (!ReadLengthWithoutOffset(head_len, field_len)) {
		return 0;
	}

	// 缓冲区不足
	if (s_len != 0 && s_len < field_len) {
		return 0;
	}
	
	// 偏移到数据的位置
	//cur += BINARY_PACKLEN_LEN_2;	
	cur += head_len;

	if (cur + field_len > ptr + len) {
		out_len = 0;
		return 0;
	}

	return field_len;
}


/* ================== BinaryStream_Writer ================== */

BinaryStream_Writer::BinaryStream_Writer(std::string* data)
	: m_data(data)
{
	m_data->clear();
	char str[BINARY_PACKAGE_MAXLEN_2 + CHECKSUM_LEN];
	m_data->append(str, sizeof(str));
}

const char* BinaryStream_Writer::GetData() const
{
	return m_data->data();
}

size_t BinaryStream_Writer::GetSize() const
{
	return m_data->length();
}

void BinaryStream_Writer::Flush()
{
	char* ptr = &(*m_data)[0]; // 取m_data字符串首元素地址
	unsigned int ulen = htonl(m_data->length()); 
	memcpy(ptr, &ulen, sizeof(ulen));  // 将m_data长度值存到其开头
}

void BinaryStream_Writer::Clear()
{
	m_data->clear();
	char str[BINARY_PACKAGE_MAXLEN_2 + CHECKSUM_LEN];
	m_data->append(str, sizeof(str));
}
