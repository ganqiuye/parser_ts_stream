/**
 * File: TsParser.cpp
 * Author: qiuye.gan
 * Date: 2025-12-01
 * Description: Implementation of TsParser class methods
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
#include "TsParser.h"

TsParser::TsParser(const std::string& file_path)
    : mFilePath(file_path),
      mInFp(nullptr),
      mLastPcr(0x1fff),
      mVideoPid(0x1fff),
      mAudioPid(0x1fff),
      mTextPid(0x1fff),
      mPrintPid(0x1fff) {
    mOutPidsFp.clear();
    mOutPids.clear();
}

TsParser::~TsParser() {
    if (mInFp) {
        fclose(mInFp);
    }
    for (auto& fp : mOutPidsFp) {
        if (fp) {
            fclose(fp);
        }
    }
}

void TsParser::setCommand(CommandOption option, void* param) {
    switch (option) {
        case OPTION_SET_INPUT_FILE:
            mFilePath = string((char*)param);
            break;
        case OPTION_OUTPUT_PID:
        {
            int pid = *(int*)param;
            if (pid == 0x1fff) {
                mDumpAllPids = true;
            } else {
                char out_filename[256];
                sprintf(out_filename, "out_%04x.es", pid);
                FILE* out_fp = fopen(out_filename, "wb");
                if (out_fp) {
                    mOutPidsFp.push_back(out_fp);
                    mOutPids[pid] = out_fp;
                } else {
                    std::cerr << "Cannot open output file: " << out_filename << std::endl;
                }
            }
            break;
        }
        case OPTION_PRINT_PTS:
        {
            int pid = *(int*)param;
            if (pid == 0x1fff) {
                mPrintAllPids = true;
            } else {
                mPrintPid = pid;
            }
            mPrintPts = true;
            break;
        }
        case OPTION_SHOW_STREAM_INFO:
            mShowStreamInfo = true;
            break;
        default:
            break;
    }
}

bool TsParser::readNextTsPacket(FILE* fp, uint8_t* pkt, bool& isSynced) {
    if (!isSynced) {
        int c;
        while ((c = fgetc(fp)) != EOF) {
            if (c == 0x47) {
                pkt[0] = 0x47;
                size_t n = fread(pkt + 1, 1, 187, fp);
                if (n < 187) return false;
                int next = fgetc(fp);
                if (next == 0x47) {
                    fseek(fp, -1, SEEK_CUR);
                    isSynced = true;
                    return true;
                } else if (next == EOF) {
                    return false;
                }
                fseek(fp, -187, SEEK_CUR);
            }
        }
        return false;
    } else {
        size_t n = fread(pkt, 1, 188, fp);
        return n == 188;
    }
}

int TsParser::parse() {
    mInFp = fopen(mFilePath.c_str(), "rb");
    if (!mInFp) {
        std::cerr << "Cannot open " << mFilePath << std::endl;
        return -1;
    }

    uint8_t pkt[188];
    bool isSynced = false;
    while (readNextTsPacket(mInFp, pkt, isSynced)) {
        packet(pkt);

        if (mShowStreamInfo && !mPat.empty()) {
            int pat_program_count = mPat.size();
            int got_pmt_count = 0;
            for (const auto& entry : mPat) {
                uint16_t program_number = entry.first;
                for (const auto& pmt : mPmt) {
                    if (pmt.program_number == program_number && pmt.isGotPmt && pmt.isGotServiceInfo) {
                        got_pmt_count++;
                        break;
                    }
                }
            }
            if (got_pmt_count == pat_program_count) {
                break;
            }
        }
    }

    fclose(mInFp);
    mInFp = nullptr;
    return 0;
}

void TsParser::saveEs(uint8_t *pkt, int len, int pid) {
    if (mDumpAllPids) {
        char out_filename[256];
        sprintf(out_filename, "out_%04x.es", pid);
        FILE* out_fp = nullptr;
        auto it = mOutPids.find(pid);
        if (it == mOutPids.end()) {
            out_fp = fopen(out_filename, "wb");
            if (out_fp) {
                mOutPidsFp.push_back(out_fp);
                mOutPids[pid] = out_fp;
            } else {
                std::cerr << "Cannot open output file: " << out_filename << std::endl;
                return;
            }
        } else {
            out_fp = it->second;
        }
        if (out_fp) {
            fwrite(pkt, 1, len, out_fp);
        }
        return;
    }
    for (auto& outPid : mOutPids) {
        if (outPid.first == pid) {
            fwrite(pkt, 1, len, outPid.second);
            break;
        }
    }
}

int TsParser::parseAdaptationField(uint8_t *pkt, int pid) {
    uint8_t adaptation_field_length = pkt[0];
    if (adaptation_field_length > 0) {
        int PCR_flag = (pkt[1] >> 4) & 0x01;
        if (PCR_flag) {
            if (adaptation_field_length < 7) {
                return 0;
            }
            uint64_t pcr_base = (((uint64_t)pkt[2]) << 25)
                        | (pkt[3] << 17)
                        | (pkt[4] << 9)
                        | (pkt[5] << 1)
                        | ((pkt[6] & 0x80) >> 7);
            uint64_t pcr_extension = ((pkt[6] & 0x01) << 8) | pkt[7];
            uint64_t pcr = pcr_base * 300 + pcr_extension;
            mLastPcr = pcr;
        }
    } else {
        adaptation_field_length = 0;
    }
    return adaptation_field_length;
}

void TsParser::parsePes(uint8_t *pkt, int len, int pid)
{
    // Implementation for parsing the PES header
    if (len < 9) {
        return;
    }
    int packet_start_code_prefix = (pkt[0] << 16) | (pkt[1] << 8) | pkt[2];
    if (packet_start_code_prefix != 0x000001)
    {
        cerr << "Invalid packet start code prefix: " << std::hex << packet_start_code_prefix << std::dec << std::endl;
        return;
    }
    
    int stream_id = pkt[3];
    int pes_packet_length = (pkt[4] << 8) | pkt[5];//N


    int pes_header_length = pkt[8];
    if (len < 9 + pes_header_length) {
        return;
    }
    if (mPrintPts) {
        if (stream_id != 0xBC && stream_id != 0xBF &&
            stream_id != 0xF0 && stream_id != 0xF1 && stream_id != 0xFF &&
            stream_id != 0xF2 && stream_id != 0xF8) {
            // Audio or Video stream
            int pts_dts_flag = (pkt[7] >> 6) & 0x03;
            if (pts_dts_flag == 0x02 || pts_dts_flag == 0x03) {
                int pts_dts = ((pkt[9] & 0x0e) << 29)
                            | (pkt[10] << 22)
                            | ((pkt[11] & 0xfe) << 14)
                            | (pkt[12] << 7)
                            | (pkt[13] >> 1);
                // Further processing of PTS/DTS can be added here
                if (pts_dts_flag == 0x02) {
                    // PTS only
                    uint64_t pts = pts_dts;
                    // mLastPts = pts;
                    // Process PTS value as needed
                    if (mPrintPid == pid  || mPrintAllPids) {
                        std::cout << "PID: " << pid << ", PTS: 0x" << std::hex << pts << std::dec  << " (" << pts << ")" << " mPrintPid: " << mPrintPid  << " mPrintAllPids: " << mPrintAllPids << std::endl;
                    }
                } else if (pts_dts_flag == 0x03) {
                    // PTS and DTS
                    uint64_t pts = pts_dts;
                    uint64_t dts = ((pkt[14] & 0x0E) << 29)
                                | (pkt[15] << 22)
                                | ((pkt[16] & 0xFE) << 14)
                                | (pkt[17] << 7)
                                | (pkt[18] >> 1);
                    // mLastPts = pts;
                    // mLastDts = dts;
                    // Process PTS and DTS values as needed
                    if (mPrintPid == pid || mPrintAllPids) {
                        std::cout << "PID: " << pid << ", PTS: 0x" << std::hex << pts << ", DTS: 0x" << std::hex << dts << std::dec << " mPrintPid: " << mPrintPid  << " mPrintAllPids: " << mPrintAllPids<< std::endl;
                    }
                }
            }
        }
    }

    int size = len - 9 - pes_header_length;
    if (size > 0) {
        saveEs(pkt + 9 + pes_header_length, size, pid);
    }
}

void TsParser::processSectionData(uint8_t* pkt, int offset, int pid, int continuity_counter, int payload_unit_start_indicator, std::map<int, SectionBuffer>& secbuf_map, void (TsParser::*parseFunc)(uint8_t*, int)) {
    auto& secbuf = secbuf_map[pid];
    if (payload_unit_start_indicator) {
        secbuf.data.clear();
        secbuf.collecting = true;
        secbuf.last_cc = continuity_counter;
        int pointer_field = pkt[offset];
        offset += 1 + pointer_field;
        int remain = 188 - offset;
        if (remain > 0) {
            secbuf.data.insert(secbuf.data.end(), pkt + offset, pkt + 188);
        }
        if (secbuf.data.size() >= 3) {
            int section_length = ((secbuf.data[1] & 0x0F) << 8) | secbuf.data[2];
            secbuf.expected_length = section_length + 3;
        }
    } else if (secbuf.collecting) {
        if (((secbuf.last_cc + 1) & 0x0F) != continuity_counter) {
            secbuf.data.clear();
            secbuf.collecting = false;
        } else {
            secbuf.last_cc = continuity_counter;
            int remain = 188 - offset;
            if (remain > 0) {
                secbuf.data.insert(secbuf.data.end(), pkt + offset, pkt + 188);
            }
        }
    } else {
        // ignore
    }
    if (secbuf.collecting && secbuf.expected_length > 0 &&
        (int)secbuf.data.size() >= secbuf.expected_length) {
        secbuf.collecting = false;
        (this->*parseFunc)(secbuf.data.data(), secbuf.expected_length);
        secbuf.data.clear();
        secbuf.expected_length = 0;
        secbuf.collecting = false;
    }
}

void TsParser::packet(uint8_t *pkt) {
    int sync_byte = pkt[0];
    if (sync_byte != 0x47) {
        return;
    }
    mPacketIndex++;
    int transport_error_indicator = (pkt[1] >> 7) & 0x01;
    int payload_unit_start_indicator = (pkt[1] >> 6) & 0x01;
    int transport_priority = (pkt[1] >> 5) & 0x01;
    int pid = ((pkt[1] & 0x1f) << 8) | pkt[2];
    if (pid == 0x1fff) {
        return;
    }
    int transport_scrambling_control = (pkt[3] >> 6) & 0x03;
    int adaptation_field_control = (pkt[3] >> 4) & 0x03;
    int continuity_counter = pkt[3] & 0x0F;
    int offset = 4;
    // printf("transport_error_indicator=%d, payload_unit_start_indicator=%d, transport_priority=%d, pid=%d, transport_scrambling_control=%d, adaptation_field_control=%d, continuity_counter=%d\n",
    //     transport_error_indicator, payload_unit_start_indicator, transport_priority, pid, transport_scrambling_control, adaptation_field_control, continuity_counter);
    if (adaptation_field_control & 0x02) {
        int adaptation_field_length = parseAdaptationField(pkt + offset, pid);
        offset += 1 + adaptation_field_length;// +1:pkt[0]
    }
    if (adaptation_field_control & 0x01) {
        if (pid == 0x0000) {
            if (!isHasGetPat) {
                offset += payload_unit_start_indicator ? 1 : 0;
                if (!parsePat(pkt + offset, 188 - offset))
                    isHasGetPat = true;
            }
            return;
        }
        if (pid == 0x0011) { // SDT PID
            // offset += payload_unit_start_indicator ? 1 : 0;
            // parseSdt(pkt + offset, 188 - offset);
            processSectionData(pkt, offset, pid, continuity_counter, payload_unit_start_indicator, mSdtSectionBuf, &TsParser::parseSdt);
            return;
        }
        bool isPmtPid = false;
        for (const auto& entry : mPat) {
            if (entry.second == pid) {
                isPmtPid = true;
                break;
            }
        }
        if (isPmtPid) {
            // offset += payload_unit_start_indicator ? 1 : 0;
            // parsePmt(pkt + offset, 188 - offset);
            processSectionData(pkt, offset, pid, continuity_counter, payload_unit_start_indicator, mPmtSectionBuf, &TsParser::parsePmt);
            return;
        }
        bool isPesPid = false;
        for (const auto& entry : mPmt) {
            for (const auto& stream : entry.streams) {
                if (stream.elementary_pid == pid) {
                    isPesPid = true;
                    break;
                }
            }
            if (isPesPid) {
                break;
            }
        }
        if (!mShowStreamInfo && isPesPid) {
            if (payload_unit_start_indicator) {
                parsePes(pkt + offset, 188 - offset, pid);
            } else {
                saveEs(pkt + offset, 188 - offset, pid);
            }
        }
    }
}

int TsParser::parsePat(uint8_t *pkt, int len)
{
    if (len < 12) {
        return -1;
    }
    // for (int i=0; i<4; i++) {
    //     printf("%02X ", pkt[i]);
    // }
    // printf("\n");
    uint16_t table_id = pkt[0];
    if (table_id != 0x00) {
        return -1;
    }

    uint8_t section_syntax_indicator = (pkt[1] >> 7) & 0x01;
    uint16_t section_length = ((pkt[1] & 0x0f) << 8) | pkt[2];
    if (section_length + 3 > len) {
        return -1;
    }
    uint16_t transport_stream_id = (pkt[3] << 8) | pkt[4];
    uint8_t version = (pkt[5] & 0x1e) >> 1;
    uint8_t current_next_indicator = pkt[5] & 0x01;
    uint8_t section_number = pkt[6];
    uint8_t last_section_number = pkt[7];
    int program_info_len = section_length - 9; // Exclude CRC (4 bytes) and header (5 bytes)
    // cout << "section_syntax_indicator: " << (int)section_syntax_indicator
    //      << ", section_length: " << (int)section_length
    //      << ", transport_stream_id: " << (int)transport_stream_id
    //      << ", version: " << (int)version
    //      << ", current_next_indicator: " << (int)current_next_indicator
    //      << ", section_number: " << (int)section_number
    //      << ", last_section_number: " << (int)last_section_number
    //      << ", program_info_len: " << program_info_len << endl;

    for (int i = 0; i < program_info_len; i += 4) {
        uint16_t program_number = (pkt[8 + i] << 8) | pkt[9 + i];
        if (program_number != 0) {
            uint16_t program_map_pid = (pkt[10 + i] & 0x1f) << 8 | pkt[11 + i];
            // printf("program_number: 0x%04x, program_map_pid: 0x%04x\n", program_number, program_map_pid);
            mPat[program_number] = program_map_pid;
        }
    }
    // std::cout << "Parsed PAT: " << mPat.size() << " programs found." << std::endl;
    // for (auto& entry : mPat) {
    //     std::cout << "  Program Number: " << entry.first << ", PMT PID: 0x"
    //               << std::hex << entry.second << std::dec << std::endl;
    // }
    return 0;
}

string TsParser::parsePrivatePesDescriptor(const uint8_t* es_info, int es_info_length) {
    std::string desc_detail;
    int desc_pos = 0;
    while (desc_pos + 2 <= es_info_length) {
        uint8_t descriptor_tag = es_info[desc_pos];
        uint8_t descriptor_len = es_info[desc_pos + 1];
        if (desc_pos + 2 + descriptor_len > es_info_length) break;
        if (descriptor_tag == 0x6A) desc_detail = "AC3 Audio";
        else if (descriptor_tag == 0x73) desc_detail = "DTS Audio";
        else if (descriptor_tag == 0x59) desc_detail = "Subtitles";
        else if (descriptor_tag == 0x05 && descriptor_len >= 4) {
            char format[5] = {0};
            memcpy(format, es_info + desc_pos + 2, 4);
            desc_detail = std::string("Registration: ") + format;
        } else {
            desc_detail = "Unknown";
        }
        desc_pos += 2 + descriptor_len;
    }
    return desc_detail;
}

void TsParser::storeStreamInfo(const uint8_t* es_info, int es_info_length, uint8_t stream_type, uint16_t elementary_pid) {
    std::string stream_desc;
    std::string desc_detail;

    switch (stream_type) {
        case 0x01:
        case 0x02:
            stream_desc = "MPEG-2 Video";
            break;
        case 0x03:
        case 0x04:
            stream_desc = "MPEG-2 Audio";
            break;
        case 0x05:
            stream_desc = "Private Sections";
            break;
        case 0x06:
            desc_detail = parsePrivatePesDescriptor(es_info, es_info_length);
            stream_desc = desc_detail.empty() ? "Private PES" : desc_detail;
            break;
        case 0x0F:
            stream_desc = "AAC Audio";
            break;
        case 0x10:
            stream_desc = "MPEG-4 Video";
            break;
        case 0x11:
            stream_desc = "AAC LATM Audio";
            break;
        case 0x1B:
            stream_desc = "H.264 Video";
            break;
        case 0x1C:
            stream_desc = "MPEG4 Audio";
            break;
        case 0x20:
            stream_desc = "MVC Video";
            break;
        case 0x21:
            stream_desc = "JPEG Video";
            break;
        case 0x24:
            stream_desc = "H.265 Video";
            break;
        case 0x33:
            stream_desc = "VVC Video";
            break;
        case 0x42:
            stream_desc = "AVS Video";
            break;
        case 0x81:
            stream_desc = "AC3";
            break;
        case 0x82:
            stream_desc = "DTS";
            break;
        case 0x83:
            stream_desc = "E-AC-3";
            break;
        case 0x84:
            stream_desc = "DTS-HD";
            break;
        case 0x87:
            stream_desc = "TrueHD";
            break;
        case 0x88:
            stream_desc = "AC4";
            break;
        case 0xD2:
            stream_desc = "AVS2";
            break;
        case 0xD4:
            stream_desc = "AVS3";
            break;
        case 0xEA:
            stream_desc = "VC-1 Video";
            break;
        default:
            stream_desc = "Unknown(type 0x" + std::to_string((int)stream_type) + ")";
            if (!desc_detail.empty()) stream_desc += " (" + desc_detail + ")";
            break;
    }
    mStreamInfo[elementary_pid] = stream_desc;
}

void TsParser::parsePmt(uint8_t *pkt, int len)
{
    if (len < 13) {
        return;
    }
    uint16_t table_id = pkt[0];
    if (table_id != 0x02) {
        return;
    }
    // for (int i=0; i<4; i++) {
    //     printf("%02X ", pkt[i]);
    // }
    // printf("\n");
    Pmt pmt;
    pmt.section_length = ((pkt[1] & 0x0f) << 8) | pkt[2];
    // printf("section_length:%d, len:%d\n", section_length, len);
    // section_length maybe larger than len
    // then the next packet is also pmt
    // if (pmt.section_length + 3 > len) {
    //     return;
    // }
    pmt.program_number = (pkt[3] << 8) | pkt[4];
    
    for (auto &entry: mPmt) {
        if (pmt.program_number == entry.program_number && entry.isGotPmt) {
            // printf("program_number: %d, pmt already got\n", pmt.program_number);
            return;
        }
    }

    pmt.version_number = (pkt[5] & 0x1e) >> 1;
    pmt.current_next_indicator = pkt[5] & 0x01;
    pmt.section_number = pkt[6];
    pmt.last_section_number = pkt[7];
    pmt.pcr_pid = ((pkt[8] & 0x1f) << 8) | pkt[9];
    pmt.program_info_length = ((pkt[10] & 0x0f) << 8) | pkt[11];
    // printf("section_length:%d, program_number: 0x%04x, pcr_pid: 0x%04x, program_info_length:%d\n", section_length, program_number, pcr_pid, program_info_length);
    int pos = 12 + pmt.program_info_length;
    while (pos < pmt.section_length + 3 - 4) { // Exclude CRC
        if (pos + 5 > len) {
            break;
        }
        PmtStreamInfo stream_info;
        stream_info.stream_type = pkt[pos];
        stream_info.elementary_pid = ((pkt[pos + 1] & 0x1f) << 8) | pkt[pos + 2];
        stream_info.es_info_length = ((pkt[pos + 3] & 0x0f) << 8) | pkt[pos + 4];
        pos += 5;
        if (pos + stream_info.es_info_length > len) {
            break;
        }
        storeStreamInfo(pkt + pos, stream_info.es_info_length, stream_info.stream_type, stream_info.elementary_pid);
        pmt.streams.push_back(stream_info);
        pos += stream_info.es_info_length;
    }
    pmt.isGotPmt = true;
    mPmt.push_back(pmt);
    // cout << "Parsed PMT for Program Number: " << program_number << ", PCR PID: 0x"
    //      << std::hex << pcr_pid << std::dec << std::endl;
    // for (auto& info : mStreamInfo) {
    //     std::cout << "    PID: 0x" << std::hex << info.first << std::dec
    //               << " : " << info.second << std::endl;
    // }
}

void TsParser::parseSdt(uint8_t *pkt, int len) {
    if (len < 11) return;
    uint8_t table_id = pkt[0];
    if (table_id != 0x42 && table_id != 0x46) return;
    uint16_t section_length = ((pkt[1] & 0x0F) << 8) | pkt[2];
    if (section_length + 3 > len) return;
    uint16_t transport_stream_id = (pkt[3] << 8) | pkt[4];
    uint8_t version_number = (pkt[5] & 0x3E) >> 1;
    uint8_t section_number = pkt[6];
    uint8_t last_section_number = pkt[7];
    uint16_t original_network_id = (pkt[8] << 8) | pkt[9];
    // printf("table_id:%x, section_lenght:%d, transport_stream_id:%x, version_number:%d, section_number:%d, last_section_number:%d, original_network_id:%x\n",
    //        table_id, section_length, transport_stream_id, version_number,
    //        section_number, last_section_number, original_network_id);
    int pos = 11;
    while (pos < section_length + 3 - 4) { // Exclude CRC
        if (pos + 5 > len) break;
        uint16_t service_id = (pkt[pos] << 8) | pkt[pos + 1];
        if (mServiceInfos.find(service_id) != mServiceInfos.end()) {
            // Service info already exists
            uint16_t descriptors_loop_length = ((pkt[pos + 3] & 0x0F) << 8) | pkt[pos + 4];
            pos += 5 + descriptors_loop_length;
            continue;
        }

        uint8_t eit_schedule_flag = (pkt[pos + 2] >> 1) & 0x01;
        uint8_t eit_present_following_flag = pkt[pos + 2] & 0x01;
        uint8_t running_status = (pkt[pos + 3] >> 5) & 0x07;
        uint8_t free_ca_mode = (pkt[pos + 3] >> 4) & 0x01;
        uint16_t descriptors_loop_length = ((pkt[pos + 3] & 0x0F) << 8) | pkt[pos + 4];
        int desc_pos = pos + 5;
        int desc_end = desc_pos + descriptors_loop_length;
        std::string service_name;
        std::string provider_name;
        while (desc_pos + 2 <= desc_end && desc_end <= len) {
            uint8_t descriptor_tag = pkt[desc_pos];
            if (descriptor_tag != 0x48) {
                desc_pos += 2 + pkt[desc_pos + 1];
                continue;
            }
            uint8_t descriptor_len = pkt[desc_pos + 1];
            uint8_t service_type   = pkt[desc_pos + 2];
            if (desc_pos + 2 + descriptor_len > desc_end) break;
            uint8_t service_provider_name_length = pkt[desc_pos + 3];
            if (service_provider_name_length + 4 > descriptor_len) break;
            desc_pos += 4;
            provider_name = std::string((char*)pkt + desc_pos, service_provider_name_length);
            desc_pos += service_provider_name_length;
            uint8_t service_name_length = pkt[desc_pos];
            desc_pos += 1;
            service_name = std::string((char*)pkt + desc_pos, service_name_length);
            desc_pos += service_name_length;
        }
        mServiceInfos[service_id] = {service_id, service_name, provider_name};
        // printf("  Service ID: 0x%04x, Service Name: %s, Provider Name: %s\n",
        //        service_id, service_name.c_str(), provider_name.c_str());
        for (auto &entry: mPmt) {
            if (entry.program_number == service_id) {
                entry.isGotServiceInfo = true;
                break;
            }
        }
        pos = desc_end;
    }
}

void TsParser::showStreamInfo()
{
    // std::cout << "Stream Information: "<< mFilePath << std::endl;
    // for (auto & info : mStreamInfo) {
    //     std::cout << "  PID: 0x" << std::hex << info.first << std::dec
    //               << " : " << info.second << std::endl;
    // }
    for (const auto &entry : mPmt) {
        cout << "Program Number: " << entry.program_number;
        auto it = mServiceInfos.find(entry.program_number);
        if (it != mServiceInfos.end()) {
            cout << "\n  service_provider: " << it->second.provider_name << "\n  service_name    : " << it->second.service_name;
        }
        cout << std::endl;
        for (const auto& stream : entry.streams) {
            cout << "   pid: 0x" << std::hex << stream.elementary_pid << std::dec
                    << " : " << mStreamInfo[stream.elementary_pid] << std::endl;
        }
        cout << "----------------------------------------" << std::endl;
    }
}
