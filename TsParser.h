/**
 * File: TsParser.h
 * Author: qiuye.gan
 * Date: 2025-12-01
 * Description: TsParser class definition
 * Copyright (C) 2024 Qiuye.gan(ganqiuye@163.com) All Rights Reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef _TS_PARSER_H_
#define _TS_PARSER_H_

#include <cstring>
#include <iostream>
#include <vector>
#include <map>
#include <cstdint>
#include <algorithm>
using namespace std;

typedef enum command_options {
    OPTION_SET_INPUT_FILE,
    OPTION_OUTPUT_PID,
    OPTION_REMOVE_OTHER_PIDS,
    OPTION_MERGE_ALL_PIDS,
    OPTION_PRINT_PTS,
    OPTION_SHOW_STREAM_INFO,
} CommandOption;

typedef struct PmtStreamInfo {
    uint8_t stream_type;
    uint16_t elementary_pid;
    uint16_t es_info_length;
    uint8_t *es_info; // ES_info_length bytes
} PmtStreamInfo;

typedef struct Pmt {
    uint16_t section_length;
    uint16_t program_number;
    uint8_t version_number;
    uint8_t current_next_indicator;
    uint8_t section_number;
    uint16_t pcr_pid;
    uint8_t last_section_number;
    uint16_t program_info_length;
    uint8_t *program_info; // program_info_length bytes
    vector<PmtStreamInfo> streams;
    bool isGotPmt = false;
    bool isGotServiceInfo = false;
} Pmt;

struct SectionBuffer {
    std::vector<uint8_t> data;
    int expected_length = 0;
    int last_cc = -1;
    bool collecting = false;
};

struct ServiceInfo {
    uint16_t service_id;
    std::string service_name;
    std::string provider_name;
};

class TsParser{
    public:
        TsParser(const string& file_path = "");
        ~TsParser();
        void setCommand(CommandOption option, void* param = nullptr);
        int parse();
        void showStreamInfo();
    private:
        int mVideoPid;
        int mAudioPid;
        int mTextPid;
        FILE *mInFp;
        vector<FILE*> mOutPidsFp;
        map<int, FILE*> mOutPids;
        string mFilePath;
        uint64_t mLastPcr;
        bool mPrintPts = false;
        bool mShowStreamInfo = false;
        map<int, string> mStreamInfo;
        map<int, int> mPat; // program_number to PMT PID
        vector<Pmt> mPmt;
        bool mDumpAllPids = false;
        bool mPrintAllPids = false;
        int mPrintPid;
        bool isHasGetPat = false;
        bool isHasGetPmt = false;
        std::map<int, SectionBuffer> mPmtSectionBuf;
        std::map<int, ServiceInfo> mServiceInfos;
        std::map<int, SectionBuffer> mSdtSectionBuf;
        uint64_t mPacketIndex = 0;
    private:
        void packet(uint8_t *pkt);
        int parseAdaptationField(uint8_t *pkt, int pid);
        void parsePes(uint8_t *pkt, int len, int pid);
        int parsePat(uint8_t *pkt, int len);
        void parsePmt(uint8_t *pkt, int len);
        void parsePcr(uint8_t *pkt, int len);
        void parsePesHeader(uint8_t *pkt, int len);
        void parsePesPayload(uint8_t *pkt, int len);
        void saveEs(uint8_t *pkt, int len, int pid);
        void storeStreamInfo(const uint8_t* es_info, int es_info_length, uint8_t stream_type, uint16_t elementary_pid);
        string parsePrivatePesDescriptor(const uint8_t* es_info, int es_info_length);
        void parseSdt(uint8_t *pkt, int len);
        void processSectionData(uint8_t* pkt, int offset, int pid, int continuity_counter, int payload_unit_start_indicator, std::map<int, SectionBuffer>& secbuf_map, void (TsParser::*parseFunc)(uint8_t*, int));
        bool readNextTsPacket(FILE* fp, uint8_t* pkt, bool& isSynced);
};

#endif /* _TS_PARSER_H_ */