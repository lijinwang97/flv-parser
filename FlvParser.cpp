#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <iostream>
#include <fstream>

#include "FlvParser.h"

using namespace std;

#define CheckBuffer(x) { if ((nBufSize-nOffset)<(x)) { nUsedLen = nOffset; return 0;} }

int CFlvParser::CAudioTag::m_aacProfile;
int CFlvParser::CAudioTag::m_sampleRateIndex;
int CFlvParser::CAudioTag::m_channelConfig;

static const unsigned int g_h264StartCode = 0x01000000;

CFlvParser::CFlvParser() {
    m_pFlvHeader = NULL;
}

CFlvParser::~CFlvParser() {
    for (int i = 0; i < m_tag.size(); i++) {
        destroy_tag(m_tag[i]);
        delete m_tag[i];
    }
}

int CFlvParser::parse(unsigned char *pBuf, int nBufSize, int &nUsedLen) {
    int nOffset = 0;

    if (m_pFlvHeader == 0) {
        CheckBuffer(9); //flv header头为9字节
        m_pFlvHeader = create_flvHeader(pBuf + nOffset); //创建FLV header
        nOffset += m_pFlvHeader->m_headSize;
    }

    while (1) {
        CheckBuffer(15); // previousTagSize(4字节) + tag header(11字节)
        int nPrevSize = show_u32(pBuf + nOffset);
        nOffset += 4;

        Tag *pTag = create_tag(pBuf + nOffset, nBufSize - nOffset); //获取一个flv tag
        if (pTag == NULL) {
            nOffset -= 4; //如果tag为null，复原offset
            break;
        }
        nOffset += (11 + pTag->m_header.m_dataSize); //tag header + tag datasize

        m_tag.push_back(pTag); //获取一个一个tag，加入tag队列
    }

    nUsedLen = nOffset; //记录使用的size
    return 0;
}

int CFlvParser::print_info() {
    stat();

    cout << "vnum: " << m_stat.m_videoNum << " , anum: " << m_stat.m_audioNum << " , mnum: " << m_stat.m_metaNum
         << endl;
    cout << "maxTimeStamp: " << m_stat.m_maxTimeStamp << " ,m_lengthSize: " << m_stat.m_lengthSize << endl;
    return 1;
}

int CFlvParser::dump_H264(const std::string &path) {
    fstream f;
    f.open(path.c_str(), ios_base::out | ios_base::binary); //以二进制文件输出

    vector<Tag *>::iterator it_tag;
    for (it_tag = m_tag.begin(); it_tag != m_tag.end(); it_tag++) {
        if ((*it_tag)->m_header.m_type != 0x09) //如果不是视频tag，continue
            continue;

        f.write((char *) (*it_tag)->m_media, (*it_tag)->m_mediaLen); //输出
    }
    f.close();

    return 1;
}

int CFlvParser::dump_AAC(const std::string &path) {
    fstream f;
    f.open(path.c_str(), ios_base::out | ios_base::binary); //以二进制方式输出

    vector<Tag *>::iterator it_tag;
    for (it_tag = m_tag.begin(); it_tag != m_tag.end(); it_tag++) {
        if ((*it_tag)->m_header.m_type != 0x08) //如果不是音频tag，continue
            continue;

        CAudioTag *pAudioTag = (CAudioTag *) (*it_tag);
        if (pAudioTag->m_soundFormat != 10) //如果不是AAC格式，continue
            continue;

        if (pAudioTag->m_mediaLen != 0)
            f.write((char *) (*it_tag)->m_media, (*it_tag)->m_mediaLen);
    }
    f.close();

    return 1;
}

int CFlvParser::dump_Flv(const std::string &path) {
    fstream f;
    f.open(path.c_str(), ios_base::out | ios_base::binary);

    // 写Flv header
    f.write((char *) m_pFlvHeader->m_flvHeader, m_pFlvHeader->m_headSize);
    unsigned int nLastTagSize = 0;


    // 写 flv tag
    vector<Tag *>::iterator it_tag;
    for (it_tag = m_tag.begin(); it_tag < m_tag.end(); it_tag++) {
        unsigned int nn = write_u32(nLastTagSize); //第一个previousTagSize
        f.write((char *) &nn, 4);

        //检查重复的start code
        if ((*it_tag)->m_header.m_type == 0x09 && *((*it_tag)->m_tagData + 1) == 0x01) {
            bool duplicate = false;
            unsigned char *pStartCode = (*it_tag)->m_tagData + 5 + m_nalUnitLength;
            //printf("tagsize=%d\n",(*it_tag)->m_header.m_dataSize);
            unsigned nalu_len = 0;
            unsigned char *p_nalu_len = (unsigned char *) &nalu_len;
            switch (m_nalUnitLength) {
                case 4:
                    nalu_len = show_u32((*it_tag)->m_tagData + 5);
                    break;
                case 3:
                    nalu_len = show_u24((*it_tag)->m_tagData + 5);
                    break;
                case 2:
                    nalu_len = show_u16((*it_tag)->m_tagData + 5);
                    break;
                default:
                    nalu_len = show_u8((*it_tag)->m_tagData + 5);
                    break;
            }
            /*
            printf("nalu_len=%u\n",nalu_len);
            printf("%x,%x,%x,%x,%x,%x,%x,%x,%x\n",(*it_tag)->m_tagData[5],(*it_tag)->m_tagData[6],
                    (*it_tag)->m_tagData[7],(*it_tag)->m_tagData[8],(*it_tag)->m_tagData[9],
                    (*it_tag)->m_tagData[10],(*it_tag)->m_tagData[11],(*it_tag)->m_tagData[12],
                    (*it_tag)->m_tagData[13]);
            */

            unsigned char *pStartCodeRecord = pStartCode;
            int i;
            for (i = 0; i < (*it_tag)->m_header.m_dataSize - 5 - m_nalUnitLength - 4; ++i) {
                if (pStartCode[i] == 0x00 && pStartCode[i + 1] == 0x00 && pStartCode[i + 2] == 0x00 &&
                    pStartCode[i + 3] == 0x01) {
                    if (pStartCode[i + 4] == 0x67) {
                        //printf("duplicate sps found!\n");
                        i += 4;
                        continue;
                    } else if (pStartCode[i + 4] == 0x68) {
                        //printf("duplicate pps found!\n");
                        i += 4;
                        continue;
                    } else if (pStartCode[i + 4] == 0x06) {
                        //printf("duplicate sei found!\n");
                        i += 4;
                        continue;
                    } else {
                        i += 4;
                        //printf("offset=%d\n",i);
                        duplicate = true;
                        break;
                    }
                }
            }

            if (duplicate) {
                nalu_len -= i;
                (*it_tag)->m_header.m_dataSize -= i;
                unsigned char *p = (unsigned char *) &((*it_tag)->m_header.m_dataSize);
                (*it_tag)->m_tagHeader[1] = p[2];
                (*it_tag)->m_tagHeader[2] = p[1];
                (*it_tag)->m_tagHeader[3] = p[0];
                //printf("after,tagsize=%d\n",(int)show_u24((*it_tag)->m_tagHeader + 1));
                //printf("%x,%x,%x\n",(*it_tag)->m_tagHeader[1],(*it_tag)->m_tagHeader[2],(*it_tag)->m_tagHeader[3]);

                f.write((char *) (*it_tag)->m_tagHeader, 11);
                switch (m_nalUnitLength) {
                    case 4:
                        *((*it_tag)->m_tagData + 5) = p_nalu_len[3];
                        *((*it_tag)->m_tagData + 6) = p_nalu_len[2];
                        *((*it_tag)->m_tagData + 7) = p_nalu_len[1];
                        *((*it_tag)->m_tagData + 8) = p_nalu_len[0];
                        break;
                    case 3:
                        *((*it_tag)->m_tagData + 5) = p_nalu_len[2];
                        *((*it_tag)->m_tagData + 6) = p_nalu_len[1];
                        *((*it_tag)->m_tagData + 7) = p_nalu_len[0];
                        break;
                    case 2:
                        *((*it_tag)->m_tagData + 5) = p_nalu_len[1];
                        *((*it_tag)->m_tagData + 6) = p_nalu_len[0];
                        break;
                    default:
                        *((*it_tag)->m_tagData + 5) = p_nalu_len[0];
                        break;
                }
                //printf("after,nalu_len=%d\n",(int)show_u32((*it_tag)->m_tagData + 5));
                f.write((char *) (*it_tag)->m_tagData, pStartCode - (*it_tag)->m_tagData);
                /*
                printf("%x,%x,%x,%x,%x,%x,%x,%x,%x\n",(*it_tag)->m_tagData[0],(*it_tag)->m_tagData[1],(*it_tag)->m_tagData[2],
                        (*it_tag)->m_tagData[3],(*it_tag)->m_tagData[4],(*it_tag)->m_tagData[5],(*it_tag)->m_tagData[6],
                        (*it_tag)->m_tagData[7],(*it_tag)->m_tagData[8]);
                */
                f.write((char *) pStartCode + i, (*it_tag)->m_header.m_dataSize - (pStartCode - (*it_tag)->m_tagData));
                /*
                printf("write size:%d\n", (pStartCode - (*it_tag)->m_tagData) +
                        ((*it_tag)->m_header.m_dataSize - (pStartCode - (*it_tag)->m_tagData)));
                */
            } else {
                f.write((char *) (*it_tag)->m_tagHeader, 11);
                f.write((char *) (*it_tag)->m_tagData, (*it_tag)->m_header.m_dataSize);
            }
        } else {
            f.write((char *) (*it_tag)->m_tagHeader, 11);
            f.write((char *) (*it_tag)->m_tagData, (*it_tag)->m_header.m_dataSize);
        }

        nLastTagSize = 11 + (*it_tag)->m_header.m_dataSize;
    }
    unsigned int nn = write_u32(nLastTagSize);
    f.write((char *) &nn, 4);

    f.close();

    return 1;
}

int CFlvParser::stat() {
    for (int i = 0; i < m_tag.size(); i++) {
        switch (m_tag[i]->m_header.m_type) {
            case 0x08:
                m_stat.m_audioNum++;
                break;
            case 0x09:
                stat_video(m_tag[i]);
                break;
            case 0x12:
                m_stat.m_metaNum++;
                break;
            default:;
        }
    }

    return 1;
}

int CFlvParser::stat_video(Tag *pTag) {
    m_stat.m_videoNum++;
    m_stat.m_maxTimeStamp = pTag->m_header.m_timeStamp;

    if (pTag->m_tagData[0] == 0x17 && pTag->m_tagData[1] == 0x00) {
        m_stat.m_lengthSize = (pTag->m_tagData[9] & 0x03) + 1;
    }

    return 1;
}

CFlvParser::FlvHeader *CFlvParser::create_flvHeader(unsigned char *pBuf) {
    FlvHeader *pHeader = new FlvHeader;
    pHeader->m_version = pBuf[3];
    pHeader->m_haveAudio = (pBuf[4] >> 2) & 0x01;    //是否有音频，音频标识在第5bit，比如 0000 0101，右移两位就是0000 01，& 0x01就可以得到值了
    pHeader->m_haveVideo = (pBuf[4] >> 0) & 0x01;    //是否有视频
    pHeader->m_headSize = show_u32(pBuf + 5);   //头部长度，从第6字节开始，所以需要移动5字节，flv header头为9字节

    pHeader->m_flvHeader = new unsigned char[pHeader->m_headSize];
    memcpy(pHeader->m_flvHeader, pBuf, pHeader->m_headSize);

    return pHeader;
}

int CFlvParser::destroy_flvHeader(FlvHeader *pHeader) {
    if (pHeader == NULL)
        return 0;

    delete pHeader->m_flvHeader;
    return 1;
}

void CFlvParser::Tag::init(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen) {
    memcpy(&m_header, pHeader, sizeof(TagHeader)); //拷贝pHeader内容到m_header

    m_tagHeader = new unsigned char[11];
    memcpy(m_tagHeader, pBuf, 11); //记录tag header原始数据

    m_tagData = new unsigned char[m_header.m_dataSize];
    memcpy(m_tagData, pBuf + 11, m_header.m_dataSize); //记录tag data原始数据
}

CFlvParser::CVideoTag::CVideoTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser) {
    init(pHeader, pBuf, nLeftLen); //存储tag header和tag data原始数据

    unsigned char *pd = m_tagData;
    m_frameType = (pd[0] & 0xf0) >> 4; //帧类型
    m_codecID = pd[0] & 0x0f;          //视频编码类型，值为7表示AVC
    if (m_header.m_type == 0x09 && m_codecID == 7) { //如果type=9并且codeId为7，那么表示h264
        parse_H264_tag(pParser); //解析h264 tag data
    }
}

CFlvParser::CAudioTag::CAudioTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser) {
    init(pHeader, pBuf, nLeftLen);

    unsigned char *pd = m_tagData;
    m_soundFormat = (pd[0] & 0xf0) >> 4; //音频格式
    m_soundRate = (pd[0] & 0x0c) >> 2;   //采样率
    m_soundSize = (pd[0] & 0x02) >> 1;   //采样精度
    m_soundType = (pd[0] & 0x01);        //是否立体声
    if (m_soundFormat == 10) // m_soundFormat=10时表示AAC
    {
        parse_AAC_tag(pParser); //解析AAC tag
    }
}

int CFlvParser::CAudioTag::parse_AAC_tag(CFlvParser *pParser) {
    unsigned char *pd = m_tagData;
    int nAACPacketType = pd[1]; //值为0表示AAC sequence header，值为1表示AAC raw

    if (nAACPacketType == 0) { //值为0表示AAC sequence header，数据是AudioSpecificConfig
        parse_audio_specificConfig(pParser, pd);
    } else if (nAACPacketType == 1) { //值为1表示AAC raw，数据是Raw AAC frame data
        parse_rawAAC(pParser, pd);
    } else {

    }

    return 1;
}

int CFlvParser::CAudioTag::parse_audio_specificConfig(CFlvParser *pParser, unsigned char *pTagData) {
    unsigned char *pd = m_tagData;

    m_aacProfile = ((pd[2] & 0xf8) >> 3) - 1; //5bit，AAC编码级别，audioObjectType
    m_sampleRateIndex = ((pd[2] & 0x07) << 1) | (pd[3] >> 7); //4bit，真正的采样率索引，samplingFrequencyIndex
    m_channelConfig = (pd[3] >> 3) & 0x0f; //4bit，通道数量

    m_media = NULL;
    m_mediaLen = 0;

    return 1;
}

int CFlvParser::CAudioTag::parse_rawAAC(CFlvParser *pParser, unsigned char *pTagData) {
    uint64_t bits = 0;
    int dataSize = m_header.m_dataSize - 2; // 减去两字节的 audio tag data信息部分

    //制作元数据，见ADTS头格式, https://blog.csdn.net/weixin_41910694/article/details/107735932
    write_u64(bits, 12, 0xFFF);
    write_u64(bits, 1, 0);
    write_u64(bits, 2, 0);
    write_u64(bits, 1, 1);
    write_u64(bits, 2, m_aacProfile);
    write_u64(bits, 4, m_sampleRateIndex);
    write_u64(bits, 1, 0);
    write_u64(bits, 3, m_channelConfig);
    write_u64(bits, 1, 0);
    write_u64(bits, 1, 0);
    write_u64(bits, 1, 0);
    write_u64(bits, 1, 0);
    write_u64(bits, 13, 7 + dataSize);
    write_u64(bits, 11, 0x7FF);
    write_u64(bits, 2, 0);

    m_mediaLen = 7 + dataSize;
    m_media = new unsigned char[m_mediaLen];
    unsigned char p64[8];
    p64[0] = (unsigned char) (bits >> 56);
    p64[1] = (unsigned char) (bits >> 48);
    p64[2] = (unsigned char) (bits >> 40);
    p64[3] = (unsigned char) (bits >> 32);
    p64[4] = (unsigned char) (bits >> 24);
    p64[5] = (unsigned char) (bits >> 16);
    p64[6] = (unsigned char) (bits >> 8);
    p64[7] = (unsigned char) (bits);

    memcpy(m_media, p64 + 1, 7);
    memcpy(m_media + 7, pTagData + 2, dataSize);

    return 1;
}

CFlvParser::Tag *CFlvParser::create_tag(unsigned char *pBuf, int nLeftLen) {
    TagHeader header;   //开始解析标签头部
    header.m_type = show_u8(pBuf + 0);          //类型
    header.m_dataSize = show_u24(pBuf + 1);     //标签body的长度
    header.m_timeStamp = show_u24(pBuf + 4);    //时间戳 低24bit
    header.m_TSEx = show_u8(pBuf + 7);          //时间戳的扩展字段, 高8bit
    header.m_StreamID = show_u24(pBuf + 8);     //流id
    header.m_TotalTS =
            (unsigned int) ((header.m_TSEx << 24)) + header.m_timeStamp; //转换成uint32_t类型，nTSEx为高位，往左移动24位，然后加上nTimeStamp
    cout << "total TS : " << header.m_TotalTS << endl;
    //cout << "nLeftLen : " << nLeftLen << " , m_dataSize : " << pTag->header.m_dataSize << endl;
    if ((header.m_dataSize + 11) > nLeftLen) { //如果tag header + tag datasize长度大于nLeftLen
        return NULL;
    }

    Tag *pTag;
    switch (header.m_type) { //根据tag type匹配对应类型，格式参考blog：https://blog.csdn.net/weixin_41910694/article/details/109564752
        case 0x09:
            pTag = new CVideoTag(&header, pBuf, nLeftLen, this); //解析视频tag
            break;
        case 0x08:
            pTag = new CAudioTag(&header, pBuf, nLeftLen, this); //解析音频tag
            break;
        case 0x12:
            pTag = new CMetaDataTag(&header, pBuf, nLeftLen, this); //解析metadata tag
            break;
        default:
            pTag = new Tag();
            pTag->init(&header, pBuf, nLeftLen);
    }

    return pTag;
}

int CFlvParser::destroy_tag(Tag *pTag) {
    if (pTag->m_media != NULL)
        delete[]pTag->m_media;
    if (pTag->m_tagData != NULL)
        delete[]pTag->m_tagData;
    if (pTag->m_tagHeader != NULL)
        delete[]pTag->m_tagHeader;

    return 1;
}

int CFlvParser::CVideoTag::parse_H264_tag(CFlvParser *pParser) {
    unsigned char *pd = m_tagData; //pd[0]表示帧类型和编码ID
    //pd[1]表示AVCPacketType，值为0表示AVC sequence header，1表示AVC NALU
    int nAVCPacketType = pd[1];
    int nCompositionTime = CFlvParser::show_u24(pd + 2);

    if (nAVCPacketType == 0) { //AVCPacketType=0那么data数据为AVCDecoderConfigurationRecoder
        parse_H264_configuration(pParser, pd);
    } else if (nAVCPacketType == 1) { //AVCPacketType=1那么表示有一个或多个NALU
        parse_nalu(pParser, pd);
    } else {

    }
    return 1;
}

/**
AVCDecoderConfigurationRecord {
    uint32_t(8) configurationVersion = 1;  [0]
    uint32_t(8) AVCProfileIndication;       [1]
    uint32_t(8) profile_compatibility;      [2]
    uint32_t(8) AVCLevelIndication;         [3]
    bit(6) reserved = ‘111111’b;            [4]
    uint32_t(2) lengthSizeMinusOne;         [4] 计算方法是 1 + (lengthSizeMinusOne & 3)，实际计算结果一直是4
    bit(3) reserved = ‘111’b;                   [5]
    uint32_t(5) numOfSequenceParameterSets; [5] SPS 的个数，计算方法是 numOfSequenceParameterSets & 0x1F，实际计算结果一直为1
    for (i=0; i< numOfSequenceParameterSets; i++) {
        uint32_t(16) sequenceParameterSetLength ;   [6,7]
        bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
    }
    uint32_t(8) numOfPictureParameterSets;      PPS 的个数，一直为1
    for (i=0; i< numOfPictureParameterSets; i++) {
        uint32_t(16) pictureParameterSetLength;
        bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
    }
}

m_nalUnitLength 这个变量告诉我们用几个字节来存储NALU的长度，如果NALULengthSizeMinusOne是0，
那么每个NALU使用一个字节的前缀来指定长度，那么每个NALU包的最大长度是255字节，
这个明显太小了，使用2个字节的前缀来指定长度，那么每个NALU包的最大长度是64K字节，
也不一定够，一般分辨率达到1280*720 的图像编码出的I帧，可能大于64K；3字节是比较完美的，
但是因为一些原因（例如对齐）没有被广泛支持；因此4字节长度的前缀是目前使用最多的方式
 */
int CFlvParser::CVideoTag::parse_H264_configuration(CFlvParser *pParser, unsigned char *pTagData) {
    unsigned char *pd = pTagData;
    // 跨过 Tag Data的VIDEODATA(1字节) AVCVIDEOPACKET(AVCPacketType(1字节) 和 CompositionTime(3字节) 5字节)
    pParser->m_nalUnitLength = (pd[9] & 0x03) + 1; //m_nalUnitLength = 1 + (lengthSizeMinusOne & 3)，表示NALU的长度

    int sps_size, pps_size;
    sps_size = CFlvParser::show_u16(pd + 11); //sequenceParameterSetLength
    pps_size = CFlvParser::show_u16(pd + 11 + (2 + sps_size) + 1); //pictureParameterSetLength

    m_mediaLen = 4 + sps_size + 4 + pps_size;
    m_media = new unsigned char[m_mediaLen];
    memcpy(m_media, &g_h264StartCode, 4);
    memcpy(m_media + 4, pd + 11 + 2, sps_size); //sequenceParameterSetNALUnit
    memcpy(m_media + 4 + sps_size, &g_h264StartCode, 4);
    memcpy(m_media + 4 + sps_size + 4, pd + 11 + 2 + sps_size + 2 + 1, pps_size); //pictureParameterSetNALUnit

    return 1;
}

int CFlvParser::CVideoTag::parse_nalu(CFlvParser *pParser, unsigned char *pTagData) {
    unsigned char *pd = pTagData;
    int nOffset = 0;

    m_media = new unsigned char[m_header.m_dataSize + 10];
    m_mediaLen = 0;

    nOffset = 5;
    while (1) {
        if (nOffset >= m_header.m_dataSize) //如果解析完了一个tag，跳出循环
            break;
        //一个tag可能包含多个nalu, 所以每个nalu前面有NalUnitLength字节表示每个nalu的长度
        int nNaluLen;  //nNaluLen表示时间nalu数据长度
        switch (pParser->m_nalUnitLength) {
            case 4:
                nNaluLen = CFlvParser::show_u32(pd + nOffset);
                break;
            case 3:
                nNaluLen = CFlvParser::show_u24(pd + nOffset);
                break;
            case 2:
                nNaluLen = CFlvParser::show_u16(pd + nOffset);
                break;
            default:
                nNaluLen = CFlvParser::show_u8(pd + nOffset);
        }
        memcpy(m_media + m_mediaLen, &g_h264StartCode, 4);
        memcpy(m_media + m_mediaLen + 4, pd + nOffset + pParser->m_nalUnitLength, nNaluLen);
        m_mediaLen += (4 + nNaluLen); //m_mediaLen = startcode + nNaluLen（实际数据长度）
        nOffset += (pParser->m_nalUnitLength + nNaluLen); //一个nalu整体长度 = NalUnitLength + NaluLen
    }

    return 1;
}

CFlvParser::CMetaDataTag::CMetaDataTag(CFlvParser::TagHeader *pHeader, uint8_t *pBuf, int nLeftLen,
                                       CFlvParser *pParser) {

    init(pHeader, pBuf, nLeftLen);
    uint8_t *pd = pBuf;
    m_amf1_type = show_u8(pd + 0); //amf1包的type
    m_amf1_size = show_u16(pd + 1); //amf1包的value_size

    if (m_amf1_type != 2) {
        printf("no metadata\n");
        return;
    }

    //解析script
    if (strncmp((const char *) "onMetaData", (const char *) (pd + 3), 10) == 0) { //从第3字节开始为amf1的value
        parse_meta(pParser);
    }

}

double CFlvParser::CMetaDataTag::hex_str2double(const uint8_t *hex, const uint32_t length) {

    double ret = 0;
    char hexstr[length * 2];
    memset(hexstr, 0, sizeof(hexstr));

    for (uint32_t i = 0; i < length; i++) {
        sprintf(hexstr + i * 2, "%02x", hex[i]);
    }

    sscanf(hexstr, "%llx", (unsigned long long *) &ret);

    return ret;
}

int CFlvParser::CMetaDataTag::parse_meta(CFlvParser *pParser) {
    uint8_t *pd = m_tagData;
    int dataSize = m_header.m_dataSize;

    uint32_t arrayLen = 0;
    uint32_t offset = 13; // Type + Value_Size + Value = 13字节

    uint32_t nameLen = 0;
    double doubleValue = 0;
    string strValue = "";
    bool boolValue = false;
    uint32_t valueLen = 0;
    uint8_t u8Value = 0;

    if (pd[offset++] == 0x08) { // 0x08表示onMetaData
        arrayLen = show_u32(pd + offset); //ECMAArrayLength
        offset += 4; //跳过 [ECMAArrayLength]占用的字节
        printf("ArrayLen = %d\n", arrayLen);
    } else {
        printf("metadata format error!!!");
        return -1;
    }

    for (uint32_t i = 0; i < arrayLen; i++) {
        doubleValue = 0;
        boolValue = false;
        strValue = "";
        //读取字段长度
        nameLen = show_u16(pd + offset); //stringLength
        offset += 2;

        char name[nameLen + 1];
        memset(name, 0, sizeof(name));
        memcpy(name, &pd[offset], nameLen); //stringData
        name[nameLen + 1] = '\0';
        offset += nameLen; //跳过字段名占用的长度

        uint8_t amfType = pd[offset++]; //type
        switch (amfType) {
            case 0x0: //Number type, 就是double类型，占用8字节
                doubleValue = hex_str2double(&pd[offset], 8);
                offset += 8; //跳过8字节
                break;
            case 0x1: //boolean type，bool类型，占用1字节
                u8Value = show_u8(pd + offset);
                offset += 1;
                if (u8Value != 0x00) {
                    boolValue = true;
                } else {
                    boolValue = false;
                }
                break;
            case 0x2: //string type，占2字节
                valueLen = show_u16(pd + offset);
                offset += 2;
                strValue.append(pd + offset, pd + offset + valueLen); //todo
                strValue.append("");
                offset += valueLen;
                break;
            default:
                printf("un handle amfType:%d\n", amfType);
                break;
        }

        if (strncmp(name, "duration", 8) == 0) {
            m_duration = doubleValue;
        } else if (strncmp(name, "width", 5) == 0) {
            m_width = doubleValue;
        } else if (strncmp(name, "height", 6) == 0) {
            m_height = doubleValue;
        } else if (strncmp(name, "videodatarate", 13) == 0) {
            m_videodatarate = doubleValue;
        } else if (strncmp(name, "framerate", 9) == 0) {
            m_framerate = doubleValue;
        } else if (strncmp(name, "videocodecid", 12) == 0) {
            m_videocodecid = doubleValue;
        } else if (strncmp(name, "audiodatarate", 13) == 0) {
            m_audiodatarate = doubleValue;
        } else if (strncmp(name, "audiosamplerate", 15) == 0) {
            m_audiosamplerate = doubleValue;
        } else if (strncmp(name, "audiosamplesize", 15) == 0) {
            m_audiosamplesize = doubleValue;
        } else if (strncmp(name, "stereo", 6) == 0) {
            m_stereo = boolValue;
        } else if (strncmp(name, "audiocodecid", 12) == 0) {
            m_audiocodecid = doubleValue;
        } else if (strncmp(name, "major_brand", 11) == 0) {
            m_major_brand = strValue;
        } else if (strncmp(name, "minor_version", 13) == 0) {
            m_minor_version = strValue;
        } else if (strncmp(name, "compatible_brands", 17) == 0) {
            m_compatible_brands = strValue;
        } else if (strncmp(name, "encoder", 7) == 0) {
            m_encoder = strValue;
        } else if (strncmp(name, "filesize", 8) == 0) {
            m_filesize = doubleValue;
        }
    }

    print_meta();
    return 1;
}

void CFlvParser::CMetaDataTag::print_meta() {
    printf("\nduration: %0.2lfs, filesize: %.0lfbytes\n", m_duration, m_filesize);

    printf("width: %0.0lf, height: %0.0lf\n", m_width, m_height);
    printf("videodatarate: %0.2lfkbps, framerate: %0.0lffps\n", m_videodatarate, m_framerate);
    printf("videocodecid: %0.0lf\n", m_videocodecid);

    printf("audiodatarate: %0.2lfkbps, audiosamplerate: %0.0lfKhz\n",
           m_audiodatarate, m_audiosamplerate);
    printf("audiosamplesize: %0.0lfbit, stereo: %d\n", m_audiosamplesize, m_stereo);
    printf("audiocodecid: %0.0lf\n", m_audiocodecid);

    printf("major_brand: %s, minor_version: %s\n", m_major_brand.c_str(), m_minor_version.c_str());
    printf("compatible_brands: %s, encoder: %s\n\n", m_compatible_brands.c_str(), m_encoder.c_str());
}
