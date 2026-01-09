#include "TsParser.h"
#include <getopt.h>
#define VERSION "1.2.0"
void Usage (char* argv[]) {
    std::cout << "Copyright: qiuye.gan(qiuye.gan@amlogic.com)" << std::endl;
    std::cout << "Version: " << VERSION << "\n" << std::endl;
    std::cout << "Usage: " << argv[0] << " <infile> [OPTIONS...]" << std::endl;
    std::cout << "OPTIONS:" << std::endl;
    std::cout << "  -i, --infile <FILE>     Input TS file path" << std::endl;
    std::cout << "  -s, --showinfo          Show stream information" << std::endl;
    std::cout << "  -o, --output_pid [PID]  Output PID to out_pid.es (no PID => dump all PIDs)" << std::endl;
    // std::cout << "  -r | --remove         : Remove all PIDs except video, audio and text" << std::endl;
    // std::cout << "  -m | --merge          : Merge all PIDs into one file" << std::endl;
    std::cout << "  -p, --print [PID]       Print pts (no PID => print all PIDs)" << std::endl;
    std::cout << "  -h, --help              Show this help message" << std::endl;
    std::cout << "  -v, --version           Show version information" << std::endl;
    std::cout << "\nExample: " << argv[0] << " -i input.ts -p" << std::endl;
    std::cout << "\nIf only <infile> is provided, it is equivalent to: " << argv[0] << " -i <infile> -s\n" << std::endl;
}

int GetPid(char* pid) {
    if (pid == nullptr) return -1;
    char *end = nullptr;
    int base = 10;
    if (pid[0] == '0' && (pid[1] == 'x' || pid[1] == 'X')) base = 16;
    long val = strtol(pid, &end, base);
    if (end == pid || *end != '\0' || val < 0 || val > 0x1fff) return -1;
    return (int)val;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        Usage(argv);
        return -1;
    }

    int optionChar = 0;
    int optionIndex = 0;
    const char *shortOptions = "hi:so::p::v";
    const struct option longOptions[] = {
        {"help",          no_argument,       0, 'h'},
        {"infile",        required_argument, 0, 'i'},
        {"showinfo",      no_argument,       0, 's'},
        {"output_pid",    optional_argument, 0, 'o'},
        // {"remove",        no_argument,       0, 'r'},
        // {"merge",         no_argument,       0, 'm'},
        {"print",         optional_argument, 0, 'p'},
        {"version",       no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };
    TsParser parser;
    int pid = 0;
    bool showInfoFlag = false;
    bool hasInputFile = false;
    if (argc == 2 && argv[1][0] != '-') {
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
                case 's':
                    showInfoFlag = true;
                    parser.setCommand(OPTION_SHOW_STREAM_INFO, nullptr);
                    break;
                case 'o':
                    if (optarg == nullptr) {
                        if (optind < argc && argv[optind][0] != '-') {
                            pid = GetPid(argv[optind]);
                            if (pid < 0) {
                                std::cerr << "Invalid PID: " << argv[optind] << std::endl;
                                return -1;
                            }
                            optind++;
                        } else {
                            pid = 8191;
                        }
                    } else {
                        pid = GetPid(optarg);
                        if (pid < 0) {
                            std::cerr << "Invalid PID: " << optarg << std::endl;
                            return -1;
                        }
                    }
                    parser.setCommand(OPTION_OUTPUT_PID, (void*)&pid);
                    break;
                case 'r':
                    // Implement remove all PIDs except video, audio and text functionality
                    // TODO
                    break;
                case 'm':
                    // Implement merge all PIDs into one file functionality
                    // TODO
                    break;
                case 'p':
                {
                    if (optarg == nullptr) {
                        if (optind < argc && argv[optind][0] != '-') {
                            pid = GetPid(argv[optind]);
                            if (pid < 0) {
                                std::cerr << "Invalid PID: " << argv[optind] << std::endl;
                                return -1;
                            }
                            optind++;
                        } else {
                            pid = 8191;
                        }
                    } else {
                        pid = GetPid(optarg);
                        if (pid < 0) {
                            std::cerr << "Invalid PID: " << optarg << std::endl;
                            return -1;
                        }
                    }
                    printf("Print pts for pid: 0x%X\n", pid);
                    parser.setCommand(OPTION_PRINT_PTS, (void*)&pid);
                    break;
                }
                case 'v':
                case ':':
                case '?':
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