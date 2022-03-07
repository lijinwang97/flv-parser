#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <iostream>
#include <fstream>

#include "FlvParser.h"

using namespace std;

#define CheckBuffer(x) { if ((nBufSize-nOffset)<(x)) { nUsedLen = nOffset; return 0;} }

int CFlvParser::CAudioTag::_aacProfile;
int CFlvParser::CAudioTag::_sampleRateIndex;
int CFlvParser::CAudioTag::_channelConfig;

static const unsigned int nH264StartCode = 0x01000000;

CFlvParser::CFlvParser() {
    _pFlvHeader = NULL;
    _vjj = new CVideojj();
}

CFlvParser::~CFlvParser() {
    for (int i = 0; i < _vpTag.size(); i++) {
        DestroyTag(_vpTag[i]);
        delete _vpTag[i];
    }
    if (_vjj != NULL)
        delete _vjj;
}

int CFlvParser::Parse(unsigned char *pBuf, int nBufSize, int &nUsedLen) {
    int nOffset = 0;

    if (_pFlvHeader == 0) {
        CheckBuffer(9);
        _pFlvHeader = CreateFlvHeader(pBuf + nOffset);
        nOffset += _pFlvHeader->nHeadSize;
    }

    while (1) {
        CheckBuffer(15);
        int nPrevSize = ShowU32(pBuf + nOffset);
        nOffset += 4;

        Tag *pTag = CreateTag(pBuf + nOffset, nBufSize - nOffset);
        if (pTag == NULL) {
            nOffset -= 4;
            break;
        }
        nOffset += (11 + pTag->_header.nDataSize);

        _vpTag.push_back(pTag);
    }

    nUsedLen = nOffset;
    return 0;
}

int CFlvParser::PrintInfo() {
    Stat();

    cout << "vnum: " << _sStat.nVideoNum << " , anum: " << _sStat.nAudioNum << " , mnum: " << _sStat.nMetaNum << endl;
    cout << "maxTimeStamp: " << _sStat.nMaxTimeStamp << " ,nLengthSize: " << _sStat.nLengthSize << endl;
    cout << "Vjj SEI num: " << _vjj->_vVjjSEI.size() << endl;
    for (int i = 0; i < _vjj->_vVjjSEI.size(); i++)
        cout << "SEI time : " << _vjj->_vVjjSEI[i].nTimeStamp << endl;
    return 1;
}

int CFlvParser::DumpH264(const std::string &path) {
    fstream f;
    f.open(path.c_str(), ios_base::out | ios_base::binary);

    vector<Tag *>::iterator it_tag;
    for (it_tag = _vpTag.begin(); it_tag != _vpTag.end(); it_tag++) {
        if ((*it_tag)->_header.nType != 0x09)
            continue;

        f.write((char *) (*it_tag)->_pMedia, (*it_tag)->_nMediaLen);
    }
    f.close();

    return 1;
}

int CFlvParser::DumpAAC(const std::string &path) {
    fstream f;
    f.open(path.c_str(), ios_base::out | ios_base::binary);

    vector<Tag *>::iterator it_tag;
    for (it_tag = _vpTag.begin(); it_tag != _vpTag.end(); it_tag++) {
        if ((*it_tag)->_header.nType != 0x08)
            continue;

        CAudioTag *pAudioTag = (CAudioTag *) (*it_tag);
        if (pAudioTag->_nSoundFormat != 10)
            continue;

        if (pAudioTag->_nMediaLen != 0)
            f.write((char *) (*it_tag)->_pMedia, (*it_tag)->_nMediaLen);
    }
    f.close();

    return 1;
}

int CFlvParser::DumpFlv(const std::string &path) {
    fstream f;
    f.open(path.c_str(), ios_base::out | ios_base::binary);

    // write flv-header
    f.write((char *) _pFlvHeader->pFlvHeader, _pFlvHeader->nHeadSize);
    unsigned int nLastTagSize = 0;


    // write flv-tag
    vector<Tag *>::iterator it_tag;
    for (it_tag = _vpTag.begin(); it_tag < _vpTag.end(); it_tag++) {
        unsigned int nn = WriteU32(nLastTagSize);
        f.write((char *) &nn, 4);

        //check duplicate start code
        if ((*it_tag)->_header.nType == 0x09 && *((*it_tag)->_pTagData + 1) == 0x01) {
            bool duplicate = false;
            unsigned char *pStartCode = (*it_tag)->_pTagData + 5 + _nNalUnitLength;
            //printf("tagsize=%d\n",(*it_tag)->_header.nDataSize);
            unsigned nalu_len = 0;
            unsigned char *p_nalu_len = (unsigned char *) &nalu_len;
            switch (_nNalUnitLength) {
                case 4:
                    nalu_len = ShowU32((*it_tag)->_pTagData + 5);
                    break;
                case 3:
                    nalu_len = ShowU24((*it_tag)->_pTagData + 5);
                    break;
                case 2:
                    nalu_len = ShowU16((*it_tag)->_pTagData + 5);
                    break;
                default:
                    nalu_len = ShowU8((*it_tag)->_pTagData + 5);
                    break;
            }
            /*
            printf("nalu_len=%u\n",nalu_len);
            printf("%x,%x,%x,%x,%x,%x,%x,%x,%x\n",(*it_tag)->_pTagData[5],(*it_tag)->_pTagData[6],
                    (*it_tag)->_pTagData[7],(*it_tag)->_pTagData[8],(*it_tag)->_pTagData[9],
                    (*it_tag)->_pTagData[10],(*it_tag)->_pTagData[11],(*it_tag)->_pTagData[12],
                    (*it_tag)->_pTagData[13]);
            */

            unsigned char *pStartCodeRecord = pStartCode;
            int i;
            for (i = 0; i < (*it_tag)->_header.nDataSize - 5 - _nNalUnitLength - 4; ++i) {
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
                (*it_tag)->_header.nDataSize -= i;
                unsigned char *p = (unsigned char *) &((*it_tag)->_header.nDataSize);
                (*it_tag)->_pTagHeader[1] = p[2];
                (*it_tag)->_pTagHeader[2] = p[1];
                (*it_tag)->_pTagHeader[3] = p[0];
                //printf("after,tagsize=%d\n",(int)ShowU24((*it_tag)->_pTagHeader + 1));
                //printf("%x,%x,%x\n",(*it_tag)->_pTagHeader[1],(*it_tag)->_pTagHeader[2],(*it_tag)->_pTagHeader[3]);

                f.write((char *) (*it_tag)->_pTagHeader, 11);
                switch (_nNalUnitLength) {
                    case 4:
                        *((*it_tag)->_pTagData + 5) = p_nalu_len[3];
                        *((*it_tag)->_pTagData + 6) = p_nalu_len[2];
                        *((*it_tag)->_pTagData + 7) = p_nalu_len[1];
                        *((*it_tag)->_pTagData + 8) = p_nalu_len[0];
                        break;
                    case 3:
                        *((*it_tag)->_pTagData + 5) = p_nalu_len[2];
                        *((*it_tag)->_pTagData + 6) = p_nalu_len[1];
                        *((*it_tag)->_pTagData + 7) = p_nalu_len[0];
                        break;
                    case 2:
                        *((*it_tag)->_pTagData + 5) = p_nalu_len[1];
                        *((*it_tag)->_pTagData + 6) = p_nalu_len[0];
                        break;
                    default:
                        *((*it_tag)->_pTagData + 5) = p_nalu_len[0];
                        break;
                }
                //printf("after,nalu_len=%d\n",(int)ShowU32((*it_tag)->_pTagData + 5));
                f.write((char *) (*it_tag)->_pTagData, pStartCode - (*it_tag)->_pTagData);
                /*
                printf("%x,%x,%x,%x,%x,%x,%x,%x,%x\n",(*it_tag)->_pTagData[0],(*it_tag)->_pTagData[1],(*it_tag)->_pTagData[2],
                        (*it_tag)->_pTagData[3],(*it_tag)->_pTagData[4],(*it_tag)->_pTagData[5],(*it_tag)->_pTagData[6],
                        (*it_tag)->_pTagData[7],(*it_tag)->_pTagData[8]);
                */
                f.write((char *) pStartCode + i, (*it_tag)->_header.nDataSize - (pStartCode - (*it_tag)->_pTagData));
                /*
                printf("write size:%d\n", (pStartCode - (*it_tag)->_pTagData) +
                        ((*it_tag)->_header.nDataSize - (pStartCode - (*it_tag)->_pTagData)));
                */
            } else {
                f.write((char *) (*it_tag)->_pTagHeader, 11);
                f.write((char *) (*it_tag)->_pTagData, (*it_tag)->_header.nDataSize);
            }
        } else {
            f.write((char *) (*it_tag)->_pTagHeader, 11);
            f.write((char *) (*it_tag)->_pTagData, (*it_tag)->_header.nDataSize);
        }

        nLastTagSize = 11 + (*it_tag)->_header.nDataSize;
    }
    unsigned int nn = WriteU32(nLastTagSize);
    f.write((char *) &nn, 4);

    f.close();

    return 1;
}

int CFlvParser::Stat() {
    for (int i = 0; i < _vpTag.size(); i++) {
        switch (_vpTag[i]->_header.nType) {
            case 0x08:
                _sStat.nAudioNum++;
                break;
            case 0x09:
                StatVideo(_vpTag[i]);
                break;
            case 0x12:
                _sStat.nMetaNum++;
                break;
            default:;
        }
    }

    return 1;
}

int CFlvParser::StatVideo(Tag *pTag) {
    _sStat.nVideoNum++;
    _sStat.nMaxTimeStamp = pTag->_header.nTimeStamp;

    if (pTag->_pTagData[0] == 0x17 && pTag->_pTagData[1] == 0x00) {
        _sStat.nLengthSize = (pTag->_pTagData[9] & 0x03) + 1;
    }

    return 1;
}

CFlvParser::FlvHeader *CFlvParser::CreateFlvHeader(unsigned char *pBuf) {
    FlvHeader *pHeader = new FlvHeader;
    pHeader->nVersion = pBuf[3];
    pHeader->bHaveAudio = (pBuf[4] >> 2) & 0x01;
    pHeader->bHaveVideo = (pBuf[4] >> 0) & 0x01;
    pHeader->nHeadSize = ShowU32(pBuf + 5);

    pHeader->pFlvHeader = new unsigned char[pHeader->nHeadSize];
    memcpy(pHeader->pFlvHeader, pBuf, pHeader->nHeadSize);

    return pHeader;
}

int CFlvParser::DestroyFlvHeader(FlvHeader *pHeader) {
    if (pHeader == NULL)
        return 0;

    delete pHeader->pFlvHeader;
    return 1;
}

void CFlvParser::Tag::Init(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen) {
    memcpy(&_header, pHeader, sizeof(TagHeader));

    _pTagHeader = new unsigned char[11];
    memcpy(_pTagHeader, pBuf, 11);

    _pTagData = new unsigned char[_header.nDataSize];
    memcpy(_pTagData, pBuf + 11, _header.nDataSize);

}

CFlvParser::CVideoTag::CVideoTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser) {
    Init(pHeader, pBuf, nLeftLen);

    unsigned char *pd = _pTagData;
    _nFrameType = (pd[0] & 0xf0) >> 4;
    _nCodecID = pd[0] & 0x0f;
    if (_header.nType == 0x09 && _nCodecID == 7) {
        ParseH264Tag(pParser);
    }
}

CFlvParser::CAudioTag::CAudioTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser) {
    Init(pHeader, pBuf, nLeftLen);

    unsigned char *pd = _pTagData;
    _nSoundFormat = (pd[0] & 0xf0) >> 4;
    _nSoundRate = (pd[0] & 0x0c) >> 2;
    _nSoundSize = (pd[0] & 0x02) >> 1;
    _nSoundType = (pd[0] & 0x01);
    if (_nSoundFormat == 10) // AAC
    {
        ParseAACTag(pParser);
    }
}

int CFlvParser::CAudioTag::ParseAACTag(CFlvParser *pParser) {
    unsigned char *pd = _pTagData;
    int nAACPacketType = pd[1];

    if (nAACPacketType == 0) {
        ParseAudioSpecificConfig(pParser, pd);
    } else if (nAACPacketType == 1) {
        ParseRawAAC(pParser, pd);
    } else {

    }

    return 1;
}

int CFlvParser::CAudioTag::ParseAudioSpecificConfig(CFlvParser *pParser, unsigned char *pTagData) {
    unsigned char *pd = _pTagData;

    _aacProfile = ((pd[2] & 0xf8) >> 3) - 1;
    _sampleRateIndex = ((pd[2] & 0x07) << 1) | (pd[3] >> 7);
    _channelConfig = (pd[3] >> 3) & 0x0f;

    _pMedia = NULL;
    _nMediaLen = 0;

    return 1;
}

int CFlvParser::CAudioTag::ParseRawAAC(CFlvParser *pParser, unsigned char *pTagData) {
    uint64_t bits = 0;
    int dataSize = _header.nDataSize - 2;

    WriteU64(bits, 12, 0xFFF);
    WriteU64(bits, 1, 0);
    WriteU64(bits, 2, 0);
    WriteU64(bits, 1, 1);
    WriteU64(bits, 2, _aacProfile);
    WriteU64(bits, 4, _sampleRateIndex);
    WriteU64(bits, 1, 0);
    WriteU64(bits, 3, _channelConfig);
    WriteU64(bits, 1, 0);
    WriteU64(bits, 1, 0);
    WriteU64(bits, 1, 0);
    WriteU64(bits, 1, 0);
    WriteU64(bits, 13, 7 + dataSize);
    WriteU64(bits, 11, 0x7FF);
    WriteU64(bits, 2, 0);

    _nMediaLen = 7 + dataSize;
    _pMedia = new unsigned char[_nMediaLen];
    unsigned char p64[8];
    p64[0] = (unsigned char) (bits >> 56);
    p64[1] = (unsigned char) (bits >> 48);
    p64[2] = (unsigned char) (bits >> 40);
    p64[3] = (unsigned char) (bits >> 32);
    p64[4] = (unsigned char) (bits >> 24);
    p64[5] = (unsigned char) (bits >> 16);
    p64[6] = (unsigned char) (bits >> 8);
    p64[7] = (unsigned char) (bits);

    memcpy(_pMedia, p64 + 1, 7);
    memcpy(_pMedia + 7, pTagData + 2, dataSize);

    return 1;
}

CFlvParser::Tag *CFlvParser::CreateTag(unsigned char *pBuf, int nLeftLen) {
    TagHeader header;
    header.nType = ShowU8(pBuf + 0);
    header.nDataSize = ShowU24(pBuf + 1);
    header.nTimeStamp = ShowU24(pBuf + 4);
    header.nTSEx = ShowU8(pBuf + 7);
    header.nStreamID = ShowU24(pBuf + 8);
    header.nTotalTS = (unsigned int) ((header.nTSEx << 24)) + header.nTimeStamp;
    cout << "total TS : " << header.nTotalTS << endl;
    //cout << "nLeftLen : " << nLeftLen << " , nDataSize : " << pTag->header.nDataSize << endl;
    if ((header.nDataSize + 11) > nLeftLen) {
        return NULL;
    }

    Tag *pTag;
    switch (header.nType) {
        case 0x09:
            pTag = new CVideoTag(&header, pBuf, nLeftLen, this);
            break;
        case 0x08:
            pTag = new CAudioTag(&header, pBuf, nLeftLen, this);
            break;
        case 0x12:
            pTag = new CMetaDataTag(&header, pBuf, nLeftLen, this);
            break;
        default:
            pTag = new Tag();
            pTag->Init(&header, pBuf, nLeftLen);
    }

    return pTag;
}

int CFlvParser::DestroyTag(Tag *pTag) {
    if (pTag->_pMedia != NULL)
        delete[]pTag->_pMedia;
    if (pTag->_pTagData != NULL)
        delete[]pTag->_pTagData;
    if (pTag->_pTagHeader != NULL)
        delete[]pTag->_pTagHeader;

    return 1;
}

int CFlvParser::CVideoTag::ParseH264Tag(CFlvParser *pParser) {
    unsigned char *pd = _pTagData;
    int nAVCPacketType = pd[1];
    int nCompositionTime = CFlvParser::ShowU24(pd + 2);

    if (nAVCPacketType == 0) {
        ParseH264Configuration(pParser, pd);
    } else if (nAVCPacketType == 1) {
        ParseNalu(pParser, pd);
    } else {

    }
    return 1;
}

int CFlvParser::CVideoTag::ParseH264Configuration(CFlvParser *pParser, unsigned char *pTagData) {
    unsigned char *pd = pTagData;

    pParser->_nNalUnitLength = (pd[9] & 0x03) + 1;

    int sps_size, pps_size;
    sps_size = CFlvParser::ShowU16(pd + 11);
    pps_size = CFlvParser::ShowU16(pd + 11 + (2 + sps_size) + 1);

    _nMediaLen = 4 + sps_size + 4 + pps_size;
    _pMedia = new unsigned char[_nMediaLen];
    memcpy(_pMedia, &nH264StartCode, 4);
    memcpy(_pMedia + 4, pd + 11 + 2, sps_size);
    memcpy(_pMedia + 4 + sps_size, &nH264StartCode, 4);
    memcpy(_pMedia + 4 + sps_size + 4, pd + 11 + 2 + sps_size + 2 + 1, pps_size);

    return 1;
}

int CFlvParser::CVideoTag::ParseNalu(CFlvParser *pParser, unsigned char *pTagData) {
    unsigned char *pd = pTagData;
    int nOffset = 0;

    _pMedia = new unsigned char[_header.nDataSize + 10];
    _nMediaLen = 0;

    nOffset = 5;
    while (1) {
        if (nOffset >= _header.nDataSize)
            break;

        int nNaluLen;
        switch (pParser->_nNalUnitLength) {
            case 4:
                nNaluLen = CFlvParser::ShowU32(pd + nOffset);
                break;
            case 3:
                nNaluLen = CFlvParser::ShowU24(pd + nOffset);
                break;
            case 2:
                nNaluLen = CFlvParser::ShowU16(pd + nOffset);
                break;
            default:
                nNaluLen = CFlvParser::ShowU8(pd + nOffset);
        }
        memcpy(_pMedia + _nMediaLen, &nH264StartCode, 4);
        memcpy(_pMedia + _nMediaLen + 4, pd + nOffset + pParser->_nNalUnitLength, nNaluLen);
        pParser->_vjj->Process(_pMedia + _nMediaLen, 4 + nNaluLen, _header.nTotalTS);
        _nMediaLen += (4 + nNaluLen);
        nOffset += (pParser->_nNalUnitLength + nNaluLen);
    }

    return 1;
}

CFlvParser::CMetaDataTag::CMetaDataTag(CFlvParser::TagHeader *pHeader, uint8_t *pBuf, int nLeftLen,
                                       CFlvParser *pParser) {

    Init(pHeader, pBuf, nLeftLen);
    uint8_t *pd = pBuf;
    m_amf1_type = ShowU8(pd + 0);
    m_amf1_size = ShowU16(pd + 1);

    if (m_amf1_type != 2) {
        printf("no metadata\n");
        return;
    }

    //解析script
    if (strncmp((const char *) "onMetaData", (const char *) (pd + 3), 10) == 0) {
        parseMeta(pParser);
    }

}

double CFlvParser::CMetaDataTag::hexStr2double(const uint8_t *hex, const uint32_t length) {

    double ret = 0;
    char hexstr[length * 2];
    memset(hexstr, 0, sizeof(hexstr));

    for (uint32_t i = 0; i < length; i++) {
        sprintf(hexstr + i * 2, "%02x", hex[i]);
    }

    sscanf(hexstr, "%llx", (unsigned long long *) &ret);

    return ret;
}

int CFlvParser::CMetaDataTag::parseMeta(CFlvParser *pParser) {
    uint8_t *pd = _pTagData;
    int dataSize = _header.nDataSize;

    uint32_t arrayLen = 0;
    uint32_t offset = 13; // Type + Value_Size + Value = 13字节

    uint32_t nameLen = 0;
    double doubleValue = 0;
    string strValue = "";
    bool boolValue = false;
    uint32_t valueLen = 0;
    uint8_t u8Value = 0;

    if (pd[offset++] == 0x08) { // 0x08 onMetaData
        arrayLen = ShowU32(pd + offset);
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
        nameLen = ShowU16(pd + offset);
        offset += 2;

        char name[nameLen + 1];
        memset(name, 0, sizeof(name));
        memcpy(name, &pd[offset], nameLen);
        name[nameLen + 1] = '\0';
        offset += nameLen; //跳过字段名占用的长度

        uint8_t amfType = pd[offset++];
        switch (amfType) {
            case 0x0: //Number type, 就是double类型，占用8字节
                doubleValue = hexStr2double(&pd[offset], 8);
                offset += 8; //跳过8字节
                break;
            case 0x1: //boolean type，bool类型，占用1字节
                u8Value = ShowU8(pd + offset);
                offset += 1;
                if (u8Value != 0x00) {
                    boolValue = true;
                } else {
                    boolValue = false;
                }
                break;
            case 0x2: //string type，占2字节
                valueLen = ShowU16(pd + offset);
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

    printMeta();
    return 1;
}

void CFlvParser::CMetaDataTag::printMeta() {
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
