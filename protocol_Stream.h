// 网络数据传输协议

#include <stdlib.h>
#include <sstream> 
#include <assert.h>
#include <string>
#include <WinSock2.h>
#include <type_traits>
#include <tuple>

enum 
{
	TEXT_PACKLEN_LEN = 4,
	TEXT_PACKAGE_MAXLEN = 0xffff,
	BINARY_PACKLEN_LEN = 2,
	BINARY_PACKAGE_MAXLEN = 0xffff,

	TEXT_PACKLEN_LEN_2 = 6,
	TEXT_PACKAGE_MAXLEN_2 = 0xffffff,

	BINARY_PACKLEN_LEN_2 = 4,               //4字节头长度
	BINARY_PACKAGE_MAXLEN_2 = 0x10000000,   //包最大长度是256M

	CHECKSUM_LEN = 2,
};

// 计算校验和
unsigned short checksum(const unsigned short* buffer, int size);
// 将一个4字节（8字节）的整型数值压缩成1~5个字节（1~10个字节）。
template<typename T>
void write7BitEncoded(T& value, std::string& buf); // uint32_t, uint64_t
//将一个1~5个字节（1~10个字节）的字符数组值还原成4字节（8字节）的整型值
template<typename T>
void read7BitEncoded(const char* buf, uint32_t len, T& value); // uint32_t, uint64_t

class BinaryStream_Reader final 
{
public:
	BinaryStream_Reader(const char* ptr, size_t len);
	~BinaryStream_Reader() = default;

public:
	bool IsEmpty() const; // 若 len<=包头长 则数据流为空
	bool IsEnd() const;
	virtual size_t GetSize() const;
	virtual const char* GetData() const;
	const char* GetCurrent() const { return cur; }
	bool ReadLength(size_t& len);
	bool ReadLengthWithoutOffset(size_t& head_len, size_t& out_len); // 读无偏移量的数据流长度
	size_t ReadAll(char* szBuffer, size_t iLen) const;
	bool ReadString(std::string& str, size_t max_len, size_t& out_len);
	bool ReadCString(char* str, size_t str_len, size_t& out_len);
	bool ReadCCString(const char** str, size_t max_len, size_t& out_len);
	size_t StrAbleSize(size_t s_len, size_t& out_len);

public:
	template<typename T>
	bool ReadFixedWidth(T& value);

private:
	BinaryStream_Reader(const BinaryStream_Reader&) = delete;
	BinaryStream_Reader& operator=(const BinaryStream_Reader&) = delete;

private:
	const char* ptr;
	const size_t len;
	const char* cur;
};

class BinaryStream_Writer final 
{
public:
	BinaryStream_Writer(std::string* data);
	~BinaryStream_Writer() = default;

public:
	virtual const char* GetData() const;
	virtual size_t GetSize() const;
	size_t GetCurrentPos() const { return m_data->length(); }
	void Flush(); // 将m_data长度值存到m_data开头
	void Clear(); // 清空并保证m_data数据长度为 包长+校验长

public:
	template<typename T>
	bool WriteFixedWidth(T value, bool isNull = false);
	template<typename T, typename ... Args>
	bool WriteString(const T& str, Args ...args);

private:
	BinaryStream_Writer(const BinaryStream_Writer&) = delete;
	BinaryStream_Writer& operator=(const BinaryStream_Writer&) = delete;

private:
	std::string* m_data;
};


// tamplate definition

// uint32_t, uint64_t
template<typename T>
void write7BitEncoded(T& value, std::string& buf)
{
	do {
		uint8_t byte = static_cast<uint8_t>(value & 0x7f); // 取出7位
		value >>= 7; // 右移7位
		if (value) {
			byte |= 0x80; // 设置最高位1，表后面仍有数据
		}
		buf.push_back(byte);
	} while (value);
}

// uint32_t, uint64_t
template<typename T>
void read7BitEncoded(const char* buf, uint32_t len, T& value) 
{
	char c = 0;
	int index = 0;
	int bitcount = 0;
	value = 0;

	do {
		c = buf[index]; // 每次从buf中取出一个字符
		T byte = (c & 0x7f);
		byte <<= bitcount;
		value += byte;
		bitcount += 7;
		++index;
	} while (c & 0x80);
}

template<typename T>
bool BinaryStream_Reader::ReadFixedWidth(T& value) 
{
	const int VALUE_SIZE = sizeof(T);
	if (cur + VALUE_SIZE > ptr + len)
		return false;

	// 根据类型进行网络字节序转换
	if constexpr (std::is_same_v<T, char>) {
		value = (char)value;
	}
	else if constexpr (std::is_same_v<T, short>) {
		value = ntohs((short)value);
	}
	else if constexpr (std::is_same_v<T, int32_t>) {
		value = ntohl((int32_t)value);
	}
	else if constexpr (std::is_same_v<T, int64_t>) {
		char tmp[128];
		size_t length;
		if (!ReadCString(tmp, 128, length)) {
			return false;
		}
		value = atoll(tmp);
		return true;
	}
	else return false;

	memcpy(&value, cur, VALUE_SIZE);

	cur += VALUE_SIZE; // 更新当前读取位置
	return true;
}

template<typename T>
bool BinaryStream_Writer::WriteFixedWidth(T value, bool isNull) 
{
	T tmp = 0;

	if constexpr (std::is_same_v<T, char>) {
		if(!isNull)
			tmp = (char)value;
		m_data->append((char*)&tmp, sizeof(tmp));
	}
	else if constexpr (std::is_same_v<T, short>) {
		if (!isNull)
			tmp = htons((short)value);
		m_data->append((char*)&tmp, sizeof(tmp));
	}
	else if constexpr (std::is_same_v<T, int32_t>) {
		if (!isNull)
			tmp = htonl((int32_t)value);
		m_data->append((char*)&tmp, sizeof(tmp));
	}
	else if constexpr (std::is_same_v<T, int64_t>) {
		char tmp[128];
		size_t length = 0;
		if (!isNull) {
			length = strlen(tmp);
#ifndef _WIN32
			sprintf(tmp, "%ld", value);
#else 		
			sprintf(tmp, "%lld", value);
#endif 
		}
		WriteString(tmp, length);
	}
	else if constexpr (std::is_same_v<T, double>) {
		char tmp[128];
		size_t length = 0;
		if (!isNull) {
			length = strlen(tmp);
			sprintf(tmp, "%f", value);
		}
		WriteString(tmp, length);
	}
	else return false;

	return true;
}

template<typename T, typename ... Args>
bool BinaryStream_Writer::WriteString(const T& str, Args ...args) 
{
	size_t len;
	std::string buf;

	if constexpr(sizeof...(args) == 1) {
		// str是char*类型，则需取参数包第一个参数len
		std::tuple<Args...> argsTuple(std::forward<Args>(args)...);
		len = static_cast<size_t>(std::get<0>(argsTuple));
	}else if constexpr ((sizeof...(args) == 0) && std::is_same_v<T, std::string>) {
		// str是stiring
		len = str.length();
	}else {
		return false;
	}

	write7BitEncoded(len, buf);
	m_data->append(buf);
	m_data->append(str, len);

	return true;
}

