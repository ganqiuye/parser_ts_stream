#include "TsParser.h"
#include <getopt.h>
void Usage (char* argv[]) {
    std::cout << "Usage: " << argv[0] << " <infile> [OPTIONS...]" << std::endl;
    std::cout << "OPTIONS:" << std::endl;
    std::cout << "  -h | --help           : Show this help message" << std::endl;
    std::cout << "  -i | --infile         : Input TS file path" << std::endl;
    std::cout << "  -v | --video_pid      : Video PID in decimal or hex (prefix 0x)" << std::endl;
    std::cout << "  -a | --audio_pid      : Audio PID in decimal or hex (prefix 0x)" << std::endl;
    std::cout << "  -t | --text_pid       : Text PID in decimal or hex (prefix 0x)" << std::endl;
    std::cout << "  -d | --dump           : Dump all PIDs to es files" << std::endl;
    std::cout << "  -s | --showinfo       : Show stream information" << std::endl;
    std::cout << "  -o | --output_pid     : Output PID to out_pid.es" << std::endl;
    std::cout << "  -r | --remove         : Remove all PIDs except video, audio and text" << std::endl;
    std::cout << "  -m | --merge          : Merge all PIDs into one file" << std::endl;
    std::cout << "  -p | --print          : Print pts" << std::endl;
    std::cout << "\nExample: " << argv[0] << " -i input.ts -v 0x100 -a 0x101" << std::endl;
    std::cout << "\nIf only <infile> is provided, it is equivalent to: " << argv[0] << " -i <infile> -s\n" << std::endl;
}

int GetPid(char* pid) {
    if (*pid == '0' && (*(pid + 1) == 'x' || *(pid + 1) == 'X')) {
        return strtol(pid,NULL,16);
    }
    return strtol(pid,NULL,10);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        Usage(argv);
        return -1;
    }

    int optionChar = 0;
    int optionIndex = 0;
    const char *shortOptions = "h i: v: a: t: d s o: r m p";
    const struct option longOptions[] = {
        {"help",          no_argument,       0, 'h'},
        {"infile",        required_argument, 0, 'i'},
        {"video_pid",     required_argument, 0, 'v'},
        {"audio_pid",     required_argument, 0, 'a'},
        {"text_pid",      required_argument, 0, 't'},
        {"dump",          no_argument,       0, 'd'},
        {"showinfo",      no_argument,       0, 's'},
        {"output_pid",    required_argument, 0, 'o'},
        {"remove",        no_argument,       0, 'r'},
        {"merge",         no_argument,       0, 'm'},
        {"print",         no_argument,       0, 'p'},
        {0, 0, 0, 0}
    };
    TsParser parser;
    int pid = 0;
    bool showInfoFlag = false;
    bool hasInputFile = false;
    if (argc == 2) {
        parser.setCommand(OPTION_SET_INPUT_FILE, (void*)argv[1]);
        showInfoFlag = true;
        parser.setCommand(OPTION_SHOW_STREAM_INFO, nullptr);
        hasInputFile = true;
    } else {
        while ((optionChar = getopt_long(argc, argv, shortOptions, longOptions, &optionIndex)) != -1) {
            switch (optionChar) {
                case 'h':
                    Usage(argv);
                    return 0;
                case 'i':
                    parser.setCommand(OPTION_SET_INPUT_FILE, (void*)optarg);
                    hasInputFile = true;
                    break;
                case 'v':
                    pid = GetPid(optarg);
                    if (pid != 0x1fff) {
                        parser.setCommand(OPTION_SET_VIDEO_PID, &pid);
                    }
                    break;
                case 'a':
                    pid = GetPid(optarg);
                    if (pid != 0x1fff) {
                        parser.setCommand(OPTION_SET_AUDIO_PID, &pid);
                    }
                    break;
                case 't':
                    pid = GetPid(optarg);
                    if (pid != 0x1fff) {
                        parser.setCommand(OPTION_SET_TEXT_PID, &pid);
                    }
                    break;
                case 'd':
                    parser.setCommand(OPTION_DUMP_ALL_PIDS, nullptr);
                    break;
                case 's':
                    showInfoFlag = true;
                    parser.setCommand(OPTION_SHOW_STREAM_INFO, nullptr);
                    break;
                case 'o':
                    pid = GetPid(optarg);
                    parser.setCommand(OPTION_OUTPUT_PID, (void*)&pid);
                    break;
                case 'r':
                    // Implement remove all PIDs except video, audio and text functionality
                    break;
                case 'm':
                    // Implement merge all PIDs into one file functionality
                    break;
                case 'p':
                    parser.setCommand(OPTION_PRINT_PTS, nullptr);
                    break;
                default:
                    Usage(argv);
                    return -1;
            }
        }
    }
    if (!hasInputFile) {
        Usage(argv);
        return -1;
    }
    parser.parse();
    if (showInfoFlag) {
        parser.showStreamInfo();
    }
    return 0;
}