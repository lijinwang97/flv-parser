#ifndef FLVPARSER_H
#define FLVPARSER_H

#include <vector>

using namespace std;

typedef unsigned long long uint64_t;

class CFlvParser {
public:
    CFlvParser();

    virtual ~CFlvParser();

    int parse(unsigned char *pBuf, int nBufSize, int &nUsedLen);

    int print_info();

    int dump_H264(const std::string &path);

    int dump_AAC(const std::string &path);

    int dump_Flv(const std::string &path);

private:
    typedef struct FlvHeader_s {
        int m_version;
        int m_haveVideo, m_haveAudio;
        int m_headSize;

        unsigned char *m_flvHeader;
    } FlvHeader;

    struct TagHeader {
        int m_type;
        int m_dataSize;
        int m_timeStamp;
        int m_TSEx;
        int m_StreamID;

        unsigned int m_TotalTS;

        TagHeader() : m_type(0), m_dataSize(0), m_timeStamp(0), m_TSEx(0), m_StreamID(0), m_TotalTS(0) {}

        ~TagHeader() {}
    };

    class Tag {
    public:
        Tag() : m_tagHeader(NULL), m_tagData(NULL), m_media(NULL), m_mediaLen(0) {}

        void init(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen);

        TagHeader m_header;
        unsigned char *m_tagHeader; //记录tag header原始数据
        unsigned char *m_tagData; //记录tag data原始数据
        unsigned char *m_media;
        int m_mediaLen;
    };

    class CVideoTag : public Tag {
    public:
        CVideoTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser);

        int m_frameType;
        int m_codecID;

        int parse_H264_tag(CFlvParser *pParser);

        int parse_H264_configuration(CFlvParser *pParser, unsigned char *pTagData);

        int parse_nalu(CFlvParser *pParser, unsigned char *pTagData);
    };

    class CAudioTag : public Tag {
    public:
        CAudioTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser);

        int m_soundFormat;
        int m_soundRate;
        int m_soundSize;
        int m_soundType;

        // aac
        static int m_aacProfile;
        static int m_sampleRateIndex;
        static int m_channelConfig;

        int parse_AAC_tag(CFlvParser *pParser);

        int parse_audio_specificConfig(CFlvParser *pParser, unsigned char *pTagData);

        int parse_rawAAC(CFlvParser *pParser, unsigned char *pTagData);
    };

    class CMetaDataTag : public Tag {
    public:
        CMetaDataTag(TagHeader *pHeader, uint8_t *pBuf, int nLeftLen, CFlvParser *pParser);

        double hex_str2double(const unsigned char *hex, const unsigned int length);

        int parse_meta(CFlvParser *pParser);

        void print_meta();

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

    struct FlvStat {
        int m_metaNum, m_videoNum, m_audioNum;
        int m_maxTimeStamp;
        int m_lengthSize;

        FlvStat() : m_metaNum(0), m_videoNum(0), m_audioNum(0), m_maxTimeStamp(0), m_lengthSize(0) {}

        ~FlvStat() {}
    };


    static unsigned int show_u32(unsigned char *pBuf) {
        return (pBuf[0] << 24) | (pBuf[1] << 16) | (pBuf[2] << 8) | pBuf[3];
    }

    static unsigned int show_u24(unsigned char *pBuf) { return (pBuf[0] << 16) | (pBuf[1] << 8) | (pBuf[2]); }

    static unsigned int show_u16(unsigned char *pBuf) { return (pBuf[0] << 8) | (pBuf[1]); }

    static unsigned int show_u8(unsigned char *pBuf) { return (pBuf[0]); }

    static void write_u64(uint64_t &x, int length, int value) {
        uint64_t mask = 0xFFFFFFFFFFFFFFFF >> (64 - length);
        x = (x << length) | ((uint64_t) value & mask);
    }

    static unsigned int write_u32(unsigned int n) {
        unsigned int nn = 0;
        unsigned char *p = (unsigned char *) &n;
        unsigned char *pp = (unsigned char *) &nn;
        pp[0] = p[3];
        pp[1] = p[2];
        pp[2] = p[1];
        pp[3] = p[0];
        return nn;
    }

    friend class Tag;

private:

    FlvHeader *create_flvHeader(unsigned char *pBuf);

    int destroy_flvHeader(FlvHeader *pHeader);

    Tag *create_tag(unsigned char *pBuf, int nLeftLen);

    int destroy_tag(Tag *pTag);

    int stat();

    int stat_video(Tag *pTag);

    int is_user_dataTag(Tag *pTag);

private:

    FlvHeader *m_pFlvHeader;
    vector<Tag *> m_tag;
    FlvStat m_stat;

    // H.264
    int m_nalUnitLength;

};

#endif // FLVPARSER_H
