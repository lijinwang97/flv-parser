#ifndef FLVPARSER_H
#define FLVPARSER_H

#include <vector>

#include "Videojj.h"

using namespace std;

typedef unsigned long long uint64_t;

class CFlvParser
{
public:
	CFlvParser();
	virtual ~CFlvParser();

	int Parse(unsigned char *pBuf, int nBufSize, int &nUsedLen);
	int PrintInfo();
	int DumpH264(const std::string &path);
	int DumpAAC(const std::string &path);
	int DumpFlv(const std::string &path);

private:
	typedef struct FlvHeader_s
	{
		int nVersion;
		int bHaveVideo, bHaveAudio;
		int nHeadSize;

		unsigned char *pFlvHeader;
	} FlvHeader;
	struct TagHeader
	{
		int nType;
		int nDataSize;
		int nTimeStamp;
		int nTSEx;
		int nStreamID;

		unsigned int nTotalTS;

		TagHeader() : nType(0), nDataSize(0), nTimeStamp(0), nTSEx(0), nStreamID(0), nTotalTS(0) {}
		~TagHeader() {}
	};

	class Tag
	{
	public:
	    Tag() : _pTagHeader(NULL), _pTagData(NULL), _pMedia(NULL), _nMediaLen(0) {}
		void Init(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen);

		TagHeader _header;
		unsigned char *_pTagHeader;
		unsigned char *_pTagData;
		unsigned char *_pMedia;
		int _nMediaLen;
	};

	class CVideoTag : public Tag
	{
	public:
		CVideoTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser);

		int _nFrameType;
		int _nCodecID;
		int ParseH264Tag(CFlvParser *pParser);
		int ParseH264Configuration(CFlvParser *pParser, unsigned char *pTagData);
		int ParseNalu(CFlvParser *pParser, unsigned char *pTagData);
	};

	class CAudioTag : public Tag
	{
	public:
		CAudioTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser);

		int _nSoundFormat;
		int _nSoundRate;
		int _nSoundSize;
		int _nSoundType;

		// aac
		static int _aacProfile;
		static int _sampleRateIndex;
		static int _channelConfig;

		int ParseAACTag(CFlvParser *pParser);
		int ParseAudioSpecificConfig(CFlvParser *pParser, unsigned char *pTagData);
		int ParseRawAAC(CFlvParser *pParser, unsigned char *pTagData);
	};

    class CMetaDataTag : public Tag {
    public:
        CMetaDataTag(TagHeader *pHeader, uint8_t *pBuf, int nLeftLen, CFlvParser *pParser);

        double hexStr2double(const unsigned char *hex, const unsigned int length);

        int parseMeta(CFlvParser *pParser);

        void printMeta();

        uint8_t m_amf1_type;
        uint32_t m_amf1_size;
        uint8_t m_amf2_type;
        unsigned char *m_meta;
        unsigned int m_length;

        double m_duration;
        double m_width;
        double m_height;
        double m_videodatarate;
        double m_framerate;
        double m_videocodecid;

        double m_audiodatarate;
        double m_audiosamplerate;
        double m_audiosamplesize;
        bool m_stereo;
        double m_audiocodecid;

        string m_major_brand;
        string m_minor_version;
        string m_compatible_brands;
        string m_encoder;

        double m_filesize;
    };

	struct FlvStat
	{
		int nMetaNum, nVideoNum, nAudioNum;
		int nMaxTimeStamp;
		int nLengthSize;

		FlvStat() : nMetaNum(0), nVideoNum(0), nAudioNum(0), nMaxTimeStamp(0), nLengthSize(0){}
		~FlvStat() {}
	};



	static unsigned int ShowU32(unsigned char *pBuf) { return (pBuf[0] << 24) | (pBuf[1] << 16) | (pBuf[2] << 8) | pBuf[3]; }
	static unsigned int ShowU24(unsigned char *pBuf) { return (pBuf[0] << 16) | (pBuf[1] << 8) | (pBuf[2]); }
	static unsigned int ShowU16(unsigned char *pBuf) { return (pBuf[0] << 8) | (pBuf[1]); }
	static unsigned int ShowU8(unsigned char *pBuf) { return (pBuf[0]); }
	static void WriteU64(uint64_t & x, int length, int value)
	{
		uint64_t mask = 0xFFFFFFFFFFFFFFFF >> (64 - length);
		x = (x << length) | ((uint64_t)value & mask);
	}
    static unsigned int WriteU32(unsigned int n)
    {
        unsigned int nn = 0;
        unsigned char *p = (unsigned char *)&n;
        unsigned char *pp = (unsigned char *)&nn;
        pp[0] = p[3];
        pp[1] = p[2];
        pp[2] = p[1];
        pp[3] = p[0];
        return nn;
    }

	friend class Tag;
	
private:

	FlvHeader *CreateFlvHeader(unsigned char *pBuf);
	int DestroyFlvHeader(FlvHeader *pHeader);
	Tag *CreateTag(unsigned char *pBuf, int nLeftLen);
	int DestroyTag(Tag *pTag);
	int Stat();
	int StatVideo(Tag *pTag);
	int IsUserDataTag(Tag *pTag);

private:

	FlvHeader* _pFlvHeader;
	vector<Tag *> _vpTag;
	FlvStat _sStat;
	CVideojj *_vjj;

	// H.264
	int _nNalUnitLength;

};

#endif // FLVPARSER_H
