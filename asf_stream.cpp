#include "asf_stream.h"
#include "datastream.h"

///////////////////////////////

union typefield{
    uint8_t byte;
    uint16_t word;
    uint32_t dword;
};

enum DATATYPE{
	NONE = 0,
	BYTE,
	WORD,
	DWORD
};

struct asf_protocol{
	asf_protocol()
		: m_maxpacket_size(32768)
	{
		m_sequencetype = NONE;
		m_paddlentype = NONE;
		m_packlentype = DWORD;
		m_repllentype = NONE;
		m_offsettype = DWORD;
		m_mediaobjnumlentype = NONE;
		m_streamnumlentype = NONE;
		m_mul_payl_pres = false;
		m_errcorr = 0;
		m_serial = 0;
	}

    void set_serial(uint8_t value){
		m_serial = value & 0x7F;
	}

    uint8_t length_flag() const{
        uint8_t res = 0;
		res = m_mul_payl_pres;
		res |= (m_sequencetype << 1);
		res |= (m_paddlentype << 3);
		res |= (m_packlentype << 5);
		res |= (m_errcorr << 7);
		return res;
	}

    void set_length_flag(uint8_t value){
		m_mul_payl_pres		= static_cast< DATATYPE >(value & 1);
		m_sequencetype		= static_cast< DATATYPE >((value >> 1) & 0x3);
		m_paddlentype		= static_cast< DATATYPE >((value >> 3) & 0x3);
		m_packlentype		= static_cast< DATATYPE >((value >> 5) & 0x3);
		m_errcorr			= static_cast< DATATYPE >((value >> 7) & 0x1);
	}

    uint8_t property_flag() const{
        uint8_t res = 0;
		res |= (m_repllentype);
		res |= (m_offsettype << 2);
		res |= (m_mediaobjnumlentype << 4);
		res |= (m_streamnumlentype << 6);
		return res;
	}

    void set_property_flag(uint8_t value){
		m_repllentype			= static_cast< DATATYPE >(value & 0x3);
		m_offsettype			= static_cast< DATATYPE >((value >> 2) & 0x3);
		m_mediaobjnumlentype	= static_cast< DATATYPE >((value >> 4) & 0x3);
		m_streamnumlentype		= static_cast< DATATYPE >((value >> 6) & 0x3);
	}

	bool create_packet(datastream& stream_in, datastream& stream_out){
        uint8_t uc_value;
        uint32_t dword = 0;
        uint16_t word = 0;
		std::vector< char > buffer;
        uint32_t len = 0;

		stream_out << length_flag();
		stream_out << property_flag();
		dword = stream_in.size();
		stream_out << dword;
        stream_out << static_cast< uint32_t >(0);
        stream_out << static_cast< uint32_t >(0);
		dword = get_curtime_msec();
		stream_out << dword;
		stream_out << word;

		uc_value = m_serial;
		stream_out << uc_value;

		dword = stream_in.pos();
		stream_out << dword;

		len	= std::min(m_maxpacket_size, stream_in.size() - stream_in.pos());
		buffer.resize(len);
		stream_in.readRawData(&buffer[0], len);
		stream_out.writeRawData(&buffer[0], len);

		bool res = stream_in.pos() == stream_in.size();

		return res;
	}

	void write_notsupported(){
		std::cout << "not supported\n";
	}

	bool parse_bytearray(const bytearray& data){
		datastream stream(data);
        uint8_t uc_value, serial;
        uint16_t us_value;
        uint32_t ul_value, len = 0, size, offset;

		stream >> uc_value;
		set_length_flag(uc_value);
		stream >> uc_value;
		set_property_flag(uc_value);
        stream >> size;
		stream >> ul_value;
		stream >> ul_value;
		stream >> m_time;
		stream >> us_value;

		if(m_sequencetype || m_paddlentype || m_mediaobjnumlentype || m_streamnumlentype){
			write_notsupported();
			return false;
		}

		stream >> serial;
		stream >> offset;

        m_buffer.resize(size);

		if(m_serial != serial){
			m_serial = serial;
		}
        if(m_buffer.empty() || offset + len > m_buffer.size()){
            return false;
        }
        len = stream.size() - stream.pos();
        //std::cout << (uint16_t)serial << " " << offset << " " << size << " " << offset + len << "\n";
		stream.readRawData(&m_buffer[offset], len);

		return (offset + len == m_buffer.size());
	}

	bytearray buffer() const{
		return m_buffer;
	}

	void set_maxsize_packet(size_t value){
		m_maxpacket_size = value;
	}

	size_t maxsize_packet() const{
		return m_maxpacket_size;
	}

private:
	bool m_mul_payl_pres;
	DATATYPE m_sequencetype;
	DATATYPE m_paddlentype;
	DATATYPE m_packlentype;
	bool m_errcorr;
	DATATYPE m_repllentype;
	DATATYPE m_offsettype;
	DATATYPE m_mediaobjnumlentype;
	DATATYPE m_streamnumlentype;
    uint8_t m_serial;
	size_t m_maxpacket_size;
    uint32_t m_time;
	bytearray m_buffer;
};

///////////////////////////////

asf_stream::asf_stream()
	: m_maxpacket_size(32768)
	, m_serial(1)
	, m_protocol(new asf_protocol)
{

}

asf_stream::~asf_stream()
{
	if(m_protocol)
		delete m_protocol;
}

void asf_stream::set_maxsize_packet(size_t value)
{
	m_maxpacket_size = value;
	m_serial = 1;
}

size_t asf_stream::maxsize_packet() const
{
	return m_maxpacket_size;
}

vectorstream asf_stream::generate_stream(const bytearray &data)
{
	vectorstream vstream;
	datastream stream_in(data);

	asf_protocol protocol;
	protocol.set_maxsize_packet(m_maxpacket_size);
	bool eof = false;

	protocol.set_serial(m_serial++);

	do{
		bytearray buffer;
		datastream stream(&buffer);
		eof = protocol.create_packet(stream_in, stream);
		vstream.push_back(buffer);
	}while(!eof);

	return vstream;
}

bytearray asf_stream::add_packet(const bytearray &data) const
{
	bytearray res;

	if(m_protocol->parse_bytearray(data)){
		res = m_protocol->buffer();
	}

	return res;
}

