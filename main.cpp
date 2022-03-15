#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <fstream>

#include "FlvParser.h"

using namespace std;

void process(fstream &fin, const char *filename);

int main(int argc, char *argv[]) {
    cout << "this is FLV parser test program!\ninput: flv, output:flv\n";

    if (argc != 3) {
        cout << "FlvParser.exe [input flv] [output flv]" << endl;
        return 0;
    }

    fstream fin;
    fin.open(argv[1], ios_base::in | ios_base::binary); //argv[1]，输入flv文件
    if (!fin)
        return 0;

    process(fin, argv[2]); //argv[2]，输出flv文件

    fin.close();

    return 1;
}

void process(fstream &fin, const char *filename) {
    CFlvParser parser;

    int nBufSize = 2 * 1024 * 1024;
    int nFlvPos = 0;
    unsigned char *pBuf, *pBak;
    pBuf = new unsigned char[nBufSize];
    pBak = new unsigned char[nBufSize];

    while (1) {
        int nReadNum = 0;
        int nUsedLen = 0;
        fin.read((char *) pBuf + nFlvPos, nBufSize - nFlvPos);
        nReadNum = fin.gcount();
        if (nReadNum == 0)
            break;
        nFlvPos += nReadNum;

        parser.parse(pBuf, nFlvPos, nUsedLen);
        if (nFlvPos != nUsedLen) {
            memcpy(pBak, pBuf + nUsedLen, nFlvPos - nUsedLen); //将未使用的size复制到pBak
            memcpy(pBuf, pBak, nFlvPos - nUsedLen); //从pBak复制回pBuf
        }
        nFlvPos -= nUsedLen;
    }
    parser.print_info(); //打印解析信息
    parser.dump_H264("parser.264"); //dump h264文件
    parser.dump_AAC("parser.aac"); //dump aac文件

    //dump into flv
    parser.dump_Flv(filename); //输出flv格式文件

    delete[]pBak; //释放内存，使用delete[]
    delete[]pBuf;
}
