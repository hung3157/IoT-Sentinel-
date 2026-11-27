#include <cerrno>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

struct DeviceData {
    std::string mac;
    std::string name;
    std::string trend;
    std::string risk;
    bool known;
    int rawRssi;
    float averageRssi;
    int channel;
    unsigned long packets;
    unsigned long seen;
    unsigned long age;
    float distance;
    unsigned int activity;
    unsigned int riskScore;
    unsigned int frameType;
    unsigned int frameSubtype;
    unsigned long beaconCount;
    unsigned long probeRequestCount;
    unsigned long probeResponseCount;
};

struct DeviceGuess {
    std::string label;
    std::string confidence;
    std::string reason;
    std::string nameClue;
    std::string vendor;
};

struct ScreenData {
    unsigned long timeMs;
    size_t selected;
    bool detailMode;
    std::vector<DeviceData> devices;
};

static bool helpMode = false;
static std::map<std::string, std::string> vendorByPrefix;

static std::vector<std::string> splitLine(const std::string& line) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        const size_t separator = line.find('|', start);
        if (separator == std::string::npos) {
            parts.push_back(line.substr(start));
            break;
        }
        parts.push_back(line.substr(start, separator - start));
        start = separator + 1;
    }
    return parts;
}

static bool readDevice(const std::vector<std::string>& fields,
                       DeviceData& device) {
    if (fields.size() != 21 || fields[0] != "@DEVICE") {
        return false;
    }

    device.known = std::atoi(fields[2].c_str()) != 0;
    device.mac = fields[3];
    device.name = fields[4];
    device.rawRssi = std::atoi(fields[5].c_str());
    device.averageRssi = (float)std::atof(fields[6].c_str());
    device.channel = std::atoi(fields[7].c_str());
    device.packets = std::strtoul(fields[8].c_str(), NULL, 10);
    device.seen = std::strtoul(fields[9].c_str(), NULL, 10);
    device.age = std::strtoul(fields[10].c_str(), NULL, 10);
    device.distance = (float)std::atof(fields[11].c_str());
    device.trend = fields[12];
    device.activity = (unsigned int)std::strtoul(fields[13].c_str(), NULL, 10);
    device.risk = fields[14];
    device.riskScore = (unsigned int)std::strtoul(fields[15].c_str(), NULL, 10);
    device.frameType = (unsigned int)std::strtoul(fields[16].c_str(), NULL, 10);
    device.frameSubtype =
        (unsigned int)std::strtoul(fields[17].c_str(), NULL, 10);
    device.beaconCount = std::strtoul(fields[18].c_str(), NULL, 10);
    device.probeRequestCount = std::strtoul(fields[19].c_str(), NULL, 10);
    device.probeResponseCount = std::strtoul(fields[20].c_str(), NULL, 10);
    return true;
}

static bool loadVendorDatabase() {
    const char* configuredPath = std::getenv("NMAP_MAC_PREFIXES");
    const char* paths[] = {
        configuredPath,
        "/usr/share/nmap/nmap-mac-prefixes"
    };

    for (size_t pathIndex = 0; pathIndex < 2; pathIndex++) {
        if (paths[pathIndex] == NULL || paths[pathIndex][0] == '\0') {
            continue;
        }

        std::ifstream file(paths[pathIndex]);
        if (!file) {
            continue;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::istringstream input(line);
            std::string prefix;
            if (!(input >> prefix)) {
                continue;
            }

            std::string vendor;
            std::getline(input, vendor);
            const size_t first = vendor.find_first_not_of(" \t");
            if (first != std::string::npos) {
                vendor.erase(0, first);
                vendorByPrefix[prefix] = vendor;
            }
        }
        return true;
    }

    return false;
}

static std::string vendorHint(const std::string& mac) {
    std::string prefix;
    for (size_t i = 0; i < mac.length() && prefix.length() < 12; i++) {
        if (std::isxdigit((unsigned char)mac[i])) {
            prefix += (char)std::toupper((unsigned char)mac[i]);
        }
    }
    if (prefix.length() != 12) {
        return "Invalid MAC; no vendor lookup";
    }

    const unsigned int firstByte =
        (unsigned int)std::strtoul(prefix.substr(0, 2).c_str(), NULL, 16);
    if ((firstByte & 0x02) != 0) {
        return "Locally administered MAC; vendor may be randomized";
    }
    if ((firstByte & 0x01) != 0) {
        return "Multicast MAC; not a device vendor";
    }

    prefix.resize(6);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                   [](unsigned char value) {
                       return (char)std::toupper(value);
                   });
    const std::map<std::string, std::string>::const_iterator match =
        vendorByPrefix.find(prefix);
    if (match == vendorByPrefix.end()) {
        return "No match in local vendor database";
    }
    return match->second;
}

static DeviceGuess guessDevice(const DeviceData& device) {
    std::string name = device.name;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char value) {
                       return (char)std::tolower(value);
                   });

    std::string nameClue = "No useful network-name clue";
    if (name != device.mac) {
        if (name.find("camera") != std::string::npos ||
            name.find("doorbell") != std::string::npos ||
            name.find("ipcam") != std::string::npos) {
            nameClue = "Name contains a camera/doorbell keyword";
        } else if (name.find("printer") != std::string::npos) {
            nameClue = "Name contains a printer keyword";
        } else if (name.find("roku") != std::string::npos ||
                   name.find("chromecast") != std::string::npos ||
                   name.find("smart tv") != std::string::npos ||
                   name.find("fire tv") != std::string::npos) {
            nameClue = "Name contains a TV/streaming keyword";
        } else if (name.find("speaker") != std::string::npos ||
                   name.find("sonos") != std::string::npos ||
                   name.find("alexa") != std::string::npos) {
            nameClue = "Name contains a speaker keyword";
        } else if (name.find("phone") != std::string::npos ||
                   name.find("iphone") != std::string::npos ||
                   name.find("android") != std::string::npos) {
            nameClue = "Name contains a phone keyword";
        } else if (name.find("laptop") != std::string::npos ||
                   name.find("notebook") != std::string::npos ||
                   name.find("computer") != std::string::npos) {
            nameClue = "Name contains a computer keyword";
        } else if (name.find("bulb") != std::string::npos ||
                   name.find("light") != std::string::npos ||
                   name.find("smart plug") != std::string::npos ||
                   name.find("thermostat") != std::string::npos ||
                   name.find("sensor") != std::string::npos) {
            nameClue = "Name contains a smart-home keyword";
        }
    }

    const std::string vendor = vendorHint(device.mac);

    if (device.beaconCount > 0 && device.probeRequestCount > 0) {
        return {"Mixed AP/client activity", "Low",
                "This MAC sent both beacon and client-probe frames",
                nameClue, vendor};
    }
    if (device.beaconCount >= 3 || device.probeResponseCount >= 2) {
        std::string evidence = "Observed " +
                               std::to_string(device.beaconCount) +
                               " beacons and " +
                               std::to_string(device.probeResponseCount) +
                               " probe responses";
        return {"Likely access point?", "High", evidence, nameClue, vendor};
    }
    if (device.beaconCount > 0 || device.probeResponseCount > 0) {
        return {"Possible access point?", "Medium",
                "One beacon or probe response observed so far",
                nameClue, vendor};
    }
    if (device.probeRequestCount >= 3) {
        return {"Likely Wi-Fi client?", "Medium",
                "Repeated probe requests; client MAC may be randomized",
                nameClue, vendor};
    }
    if (device.probeRequestCount > 0) {
        return {"Possible Wi-Fi client?", "Low",
                "One client probe request observed",
                nameClue, vendor};
    }
    if (device.frameType == 2 || device.frameType == 3) {
        return {"Wi-Fi device", "Low",
                "Data/control frames do not reveal a specific role",
                nameClue, vendor};
    }
    return {"Unknown", "Low", "No identifying frame pattern observed",
            nameClue, vendor};
}

static void printList(const ScreenData& screen) {
    std::cout << "+------------------- IOT SENTINEL --------------------+\n";
    if (screen.devices.empty()) {
        std::cout << "|                    Scanning...                      |\n";
    }

    size_t start = 0;
    if (screen.selected >= 3) {
        start = screen.selected - 2;
    }

    for (size_t row = 0;
         row < 3 && start + row < screen.devices.size();
         row++) {
        const DeviceData& device = screen.devices[start + row];
        std::string name = device.name;
        if (name == device.mac && name.length() > 9) {
            name = name.substr(9);
        }
        if (name.length() > 15) {
            name.resize(15);
        }
        const DeviceGuess guess = guessDevice(device);
        std::cout << "| " << (start + row == screen.selected ? ">" : " ")
                  << (device.known ? "KNOWN " : "UNKNOWN ")
                  << name << " [" << guess.label << "]\n";
        std::cout << "|    " << device.mac << "  RSSI "
                  << device.rawRssi << " dBm\n";
    }
    std::cout << "+------------------------------------------------------+\n";
}

static void printDetails(const ScreenData& screen) {
    if (screen.devices.empty() || screen.selected >= screen.devices.size()) {
        printList(screen);
        return;
    }

    const DeviceData& device = screen.devices[screen.selected];
    const DeviceGuess guess = guessDevice(device);
    std::cout << "+-------------------- DEVICE DETAILS --------------------+\n";
    std::cout << "| " << (device.known ? "KNOWN DEVICE" : "UNKNOWN DEVICE")
              << "  " << device.name << "\n";
    std::cout << "| ROLE GUESS: " << guess.label << "\n";
    std::cout << "| CONFIDENCE: " << guess.confidence << "\n";
    std::cout << "| EVIDENCE: " << guess.reason << "\n";
    std::cout << "| NAME CLUE: " << guess.nameClue << "\n";
    std::cout << "| MANUFACTURER HINT: " << guess.vendor << "\n";
    std::cout << "| FRAMES: beacon " << device.beaconCount
              << ", probe request " << device.probeRequestCount
              << ", response " << device.probeResponseCount << "\n";
    std::cout << "| MAC: " << device.mac << "\n";
    std::cout << "| RSSI: " << device.rawRssi << " raw, "
              << device.averageRssi << " average dBm\n";
    if (device.distance >= 0.0f) {
        std::cout << "| DISTANCE: about " << device.distance << " m\n";
    } else {
        std::cout << "| DISTANCE: unavailable\n";
    }
    std::cout << "| CHANNEL: " << device.channel
              << "  PACKETS: " << device.packets << "\n";
    std::cout << "| SEEN: " << device.seen << "  AGE: "
              << device.age << " s\n";
    std::cout << "| TREND: " << device.trend
              << "  ACTIVITY: " << device.activity << "/min\n";
    std::cout << "| RISK: " << device.risk << " ("
              << device.riskScore << "/100)\n";
    std::cout << "| Guess is only a clue, not a confirmed device identity.\n";
    std::cout << "+--------------------------------------------------------+\n";
}

static void printHelpScreen() {
    std::cout << "\033[2J\033[H";
    std::cout << "ESP32 terminal test program\n"
              << "This program displays live data received from the board.\n"
              << "Device guesses use Wi-Fi frame clues or network-name keywords;\n"
              << "roles use accumulated frame counts; names are weak clues.\n"
              << "Manufacturer hints use a local Nmap OUI database only.\n"
              << "Guesses are not guaranteed to identify the real device.\n"
              << "Type a key and press Enter:\n"
              << "  1  Request a fresh screen report\n"
              << "  2  Switch between list and details\n"
              << "  3  Select the next detected device\n"
              << "  4  Select the previous detected device\n"
              << "  b  Return to the device list\n"
              << "  5  Mark the selected device known or unknown\n"
              << "  h  Show this help\n"
              << "  q  Quit\n\n"
              << "Press b to return to the device list.\n"
              << std::flush;
}

static void printScreen(const ScreenData& screen) {
    std::cout << "\033[2J\033[H";
    if (helpMode) {
        printHelpScreen();
        return;
    }

    if (screen.detailMode) {
        printDetails(screen);
    } else {
        printList(screen);
    }
    std::cout << "ESP32 connected | Uptime: " << screen.timeMs / 1000
              << " s | Devices: " << screen.devices.size() << "\n\n";
    std::cout << "Keys (press Enter): 1 refresh, 2 list/details, "
                 "3 next, 4 previous, b list, 5 known/unknown, h help, q quit\n> "
              << std::flush;
}

static bool sendCommand(int serialFd, char command) {
    const char message[] = {command, '\n'};
    const ssize_t sent = write(serialFd, message, sizeof(message));
    if (sent != (ssize_t)sizeof(message)) {
        std::cerr << "Could not send command to ESP32: "
                  << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

static bool processLine(const std::string& line, ScreenData& screen) {
    const std::vector<std::string> fields = splitLine(line);
    if (fields.empty()) {
        return false;
    }

    if (fields[0] == "@SCREEN" && fields.size() == 5) {
        screen.timeMs = std::strtoul(fields[1].c_str(), NULL, 10);
        screen.selected = (size_t)std::strtoul(fields[3].c_str(), NULL, 10);
        screen.detailMode = std::atoi(fields[4].c_str()) != 0;
        screen.devices.clear();
    } else if (fields[0] == "@DEVICE") {
        DeviceData device{};
        if (readDevice(fields, device)) {
            screen.devices.push_back(device);
        } else {
            std::cerr << "Ignoring malformed ESP32 device record ("
                      << fields.size() << " fields).\n";
        }
    } else if (fields[0] == "@END") {
        printScreen(screen);
        return true;
    }
    return false;
}

static bool configureSerial(int serialFd) {
    struct termios options;
    if (tcgetattr(serialFd, &options) != 0) {
        return false;
    }

    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
                         IGNCR | ICRNL | IXON);
    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    options.c_cflag &= ~(CSIZE | PARENB);
    options.c_cflag |= CS8 | CLOCAL | CREAD;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;
    if (cfsetispeed(&options, B115200) != 0 ||
        cfsetospeed(&options, B115200) != 0 ||
        tcsetattr(serialFd, TCSANOW, &options) != 0) {
        return false;
    }
    tcflush(serialFd, TCIFLUSH);
    return true;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: make simulator PORT=/dev/ttyUSB0\n"
                  << "Find the port with: make ports\n";
        return 1;
    }

    const int serialFd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serialFd < 0) {
        std::cerr << "Could not open " << argv[1] << ": "
                  << std::strerror(errno) << "\n";
        return 1;
    }
    if (!configureSerial(serialFd)) {
        std::cerr << "Could not configure " << argv[1] << ": "
                  << std::strerror(errno) << "\n";
        close(serialFd);
        return 1;
    }

    if (!loadVendorDatabase()) {
        std::cerr << "No local Nmap MAC-prefix database found; manufacturer "
                     "hints will be unavailable. Set NMAP_MAC_PREFIXES to "
                     "a local nmap-mac-prefixes file.\n";
    }

    printHelpScreen();
    std::cout << "Waiting for ESP32 data on " << argv[1] << "...\n"
              << std::flush;

    ScreenData screen{};
    std::string inputLine;
    std::string serialLine;
    const time_t startTime = time(NULL);
    bool gotReport = false;
    bool waitingMessageShown = false;
    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(STDIN_FILENO, &readSet);
        FD_SET(serialFd, &readSet);
        const int maxFd = serialFd > STDIN_FILENO ? serialFd : STDIN_FILENO;

        struct timeval timeout = {1, 0};
        const int ready = select(maxFd + 1, &readSet, NULL, NULL, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Serial wait failed: " << std::strerror(errno) << "\n";
            break;
        }

        if (FD_ISSET(serialFd, &readSet)) {
            char buffer[256];
            const ssize_t bytesRead = read(serialFd, buffer, sizeof(buffer));
            if (bytesRead > 0) {
                for (ssize_t i = 0; i < bytesRead; i++) {
                    if (buffer[i] == '\n') {
                        if (processLine(serialLine, screen)) {
                            gotReport = true;
                        }
                        serialLine.clear();
                    } else if (buffer[i] != '\r' && serialLine.length() < 512) {
                        serialLine += buffer[i];
                    }
                }
            } else if (bytesRead < 0 && errno != EAGAIN && errno != EINTR) {
                std::cerr << "Could not read from ESP32: "
                          << std::strerror(errno) << "\n";
                break;
            }
        }

        if (!gotReport && !waitingMessageShown &&
            time(NULL) - startTime >= 5) {
            std::cout << "\nStill waiting for ESP32 data. Check that the "
                         "updated firmware is uploaded and no serial monitor "
                         "is using this port.\n"
                      << std::flush;
            waitingMessageShown = true;
        }

        if (FD_ISSET(STDIN_FILENO, &readSet)) {
            char input[64];
            const ssize_t bytesRead = read(STDIN_FILENO, input, sizeof(input));
            if (bytesRead <= 0) {
                break;
            }
            for (ssize_t i = 0; i < bytesRead; i++) {
                if (input[i] == '\n' || input[i] == '\r') {
                    if (inputLine.empty()) {
                        continue;
                    }
                    const char command = inputLine[0];
                    inputLine.clear();
                    if (command == 'q' || command == 'Q') {
                        close(serialFd);
                        std::cout << "\nDisconnected from ESP32.\n";
                        return 0;
                    } else if (command == 'h' || command == 'H') {
                        helpMode = true;
                        printHelpScreen();
                    } else if (command == 'b' || command == 'B') {
                        if (sendCommand(serialFd, 'L')) {
                            helpMode = false;
                            screen.detailMode = false;
                            printScreen(screen);
                        }
                    } else if (command == '4') {
                        helpMode = false;
                        sendCommand(serialFd, 'P');
                    } else if (command == '1') {
                        sendCommand(serialFd, 'R');
                    } else if (command == '2') {
                        sendCommand(serialFd, 'M');
                    } else if (command == '3') {
                        sendCommand(serialFd, 'N');
                    } else if (command == '5') {
                        sendCommand(serialFd, 'K');
                    } else {
                        std::cout << "Unknown key. Press h for help.\n";
                    }
                } else if (inputLine.length() < 16) {
                    inputLine += input[i];
                }
            }
        }
    }

    close(serialFd);
    return 0;
}
