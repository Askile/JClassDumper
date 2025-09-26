#include "Utils.h"
#include "Dumper.h"
#include <string>
#include <Windows.h>
#include <atomic>
#include <thread>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <json.hpp>

#define P_UTF8 1
#define P_INT 3
#define P_FLOAT 4
#define P_LONG 5
#define P_DOUBLE 6
#define P_CLASS 7
#define P_STRING 8
#define P_FIELD_REF 9
#define P_METHOD_REF 10
#define P_INTERFACE_METHOD_REF 11
#define P_NAME_AND_TYPE 12
#define P_METHOD_HANDLE 15
#define P_METHOD_TYPE 16
#define P_DYNAMIC 17
#define P_INVOKE_DYNAMIC 18
#define P_MODULE 19
#define P_PACKAGE 20

HWND g_hWnd = nullptr;

const std::string basePath = Utils::GetLocalAppDataPath() + "\\JClassDumper\\dump";

static std::atomic_bool g_scanning(false);
static std::thread g_scanThread;

struct AttributeInfo {
    std::string name;
    std::vector<uint8_t> info;
};

struct FieldInfo {
    uint16_t accessFlags;
    uint16_t nameIndex;
    uint16_t descriptorIndex;
    std::vector<AttributeInfo> attributes;
};

struct MethodInfo {
    uint16_t accessFlags;
    uint16_t nameIndex;
    uint16_t descriptorIndex;
    std::vector<AttributeInfo> attributes;
};

struct ConstantPoolEntry {
    uint8_t tag;
    std::vector<uint8_t> rawData;
    uint16_t nameIndex;
    uint16_t descriptorIndex;
    uint16_t classIndex;
    uint16_t nameAndTypeIndex;
    uint16_t refIndex;
    uint8_t refKind;
};

struct ClassInfo {
    uintptr_t address;
    uint16_t minorVersion;
    uint16_t majorVersion;
    uint16_t constantPoolCount;
    std::vector<ConstantPoolEntry> constantPool;
    uint16_t accessFlags;
    uint16_t thisClass;
    uint16_t superClass;
    std::vector<uint16_t> interfaces;
    std::vector<FieldInfo> fields;
    std::vector<MethodInfo> methods;
    std::vector<AttributeInfo> attributes;
};

bool readU2Safe(const BYTE* buf, SIZE_T bufSize, SIZE_T offset, uint16_t& out) {
    if (offset + 2 > bufSize) return false;
    out = (buf[offset] << 8) | buf[offset + 1];
    return true;
}

bool readU4Safe(const BYTE* buf, SIZE_T bufSize, SIZE_T offset, uint32_t& out) {
    if (offset + 4 > bufSize) return false;
    out = (buf[offset] << 24) | (buf[offset + 1] << 16) | (buf[offset + 2] << 8) | buf[offset + 3];
    return true;
}

void parseConstantPool(const BYTE* buf, SIZE_T bufSize, SIZE_T& offset, uint16_t cpCount, std::vector<ConstantPoolEntry>& pool) {
    pool.resize(cpCount);
    for (uint16_t i = 1; i < cpCount; i++) {
        if (offset >= bufSize) break;

        uint8_t tag = buf[offset++];
        ConstantPoolEntry entry;
        entry.tag = tag;

        switch (tag) {
        case P_UTF8: {
            uint16_t len;
            if (!readU2Safe(buf, bufSize, offset, len)) return;
            offset += 2;
            if (offset + len > bufSize) return;
            entry.rawData.insert(entry.rawData.end(), buf + offset, buf + offset + len);
            offset += len;
            break;
        }
        case P_INT: case P_FLOAT:
            if (offset + 4 > bufSize) return;
            entry.rawData.insert(entry.rawData.end(), buf + offset, buf + offset + 4);
            offset += 4;
            break;
        case P_LONG: case P_DOUBLE:
            if (offset + 8 > bufSize) return;
            entry.rawData.insert(entry.rawData.end(), buf + offset, buf + offset + 8);
            offset += 8; i++;
            break;
        case P_CLASS: case P_STRING: case P_METHOD_TYPE: case P_MODULE: case P_PACKAGE:
            if (!readU2Safe(buf, bufSize, offset, entry.nameIndex)) return;
            offset += 2;
            break;
        case P_FIELD_REF: case P_METHOD_REF: case P_INTERFACE_METHOD_REF:
        case P_NAME_AND_TYPE: case P_DYNAMIC: case P_INVOKE_DYNAMIC:
            if (!readU2Safe(buf, bufSize, offset, entry.classIndex)) return;
            if (!readU2Safe(buf, bufSize, offset + 2, entry.nameAndTypeIndex)) return;
            offset += 4;
            break;
        case P_METHOD_HANDLE:
            if (offset + 3 > bufSize) return;
            entry.refKind = buf[offset++];
            if (!readU2Safe(buf, bufSize, offset, entry.refIndex)) return;
            offset += 2;
            break;
        default:
            break;
        }

        pool[i] = entry;
    }
}

void parseAttributes(const BYTE* buf, SIZE_T bufSize, SIZE_T& offset, std::vector<AttributeInfo>& attributes, const std::vector<ConstantPoolEntry>& cp) {
    uint16_t attrCount;
    if (!readU2Safe(buf, bufSize, offset, attrCount)) return;
    offset += 2;

    attributes.resize(attrCount);
    for (uint16_t i = 0; i < attrCount; i++) {
        uint16_t nameIndex;
        uint32_t length;
        if (!readU2Safe(buf, bufSize, offset, nameIndex)) return;
        offset += 2;
        if (!readU4Safe(buf, bufSize, offset, length)) return;
        offset += 4;
        if (offset + length > bufSize) return;

        std::vector<uint8_t> info(buf + offset, buf + offset + length);
        offset += length;

        std::string name;
        if (nameIndex < cp.size() && cp[nameIndex].tag == P_UTF8) {
            name = std::string(cp[nameIndex].rawData.begin(), cp[nameIndex].rawData.end());
        }

        attributes[i] = { name, info };
    }
}

void parseFields(const BYTE* buf, SIZE_T bufSize, SIZE_T& offset, std::vector<FieldInfo>& fields, const std::vector<ConstantPoolEntry>& cp) {
    uint16_t fieldCount;
    if (!readU2Safe(buf, bufSize, offset, fieldCount)) return;
    offset += 2;

    fields.resize(fieldCount);
    for (uint16_t i = 0; i < fieldCount; i++) {
        if (!readU2Safe(buf, bufSize, offset, fields[i].accessFlags)) return;
        offset += 2;
        if (!readU2Safe(buf, bufSize, offset, fields[i].nameIndex)) return;
        offset += 2;
        if (!readU2Safe(buf, bufSize, offset, fields[i].descriptorIndex)) return;
        offset += 2;

        parseAttributes(buf, bufSize, offset, fields[i].attributes, cp);
    }
}

void parseMethods(const BYTE* buf, SIZE_T bufSize, SIZE_T& offset, std::vector<MethodInfo>& methods, const std::vector<ConstantPoolEntry>& cp) {
    uint16_t methodCount;
    if (!readU2Safe(buf, bufSize, offset, methodCount)) return;
    offset += 2;

    methods.resize(methodCount);
    for (uint16_t i = 0; i < methodCount; i++) {
        if (!readU2Safe(buf, bufSize, offset, methods[i].accessFlags)) return;
        offset += 2;
        if (!readU2Safe(buf, bufSize, offset, methods[i].nameIndex)) return;
        offset += 2;
        if (!readU2Safe(buf, bufSize, offset, methods[i].descriptorIndex)) return;
        offset += 2;

        parseAttributes(buf, bufSize, offset, methods[i].attributes, cp);
    }
}

SIZE_T getClassSize(const BYTE* buf, SIZE_T bufSize, const BYTE* pattern, int header_size) {
    SIZE_T offset = header_size;

    if (memcmp(buf, pattern, header_size) != 0) return 0;

    uint16_t minor, major;
    if (!readU2Safe(buf, bufSize, offset, minor)) return 0;
    offset += 2;
    if (!readU2Safe(buf, bufSize, offset, major)) return 0;
    offset += 2;

    uint16_t cpCount;
    if (!readU2Safe(buf, bufSize, offset, cpCount)) return 0;
    offset += 2;

    std::vector<ConstantPoolEntry> cp;
    parseConstantPool(buf, bufSize, offset, cpCount, cp);

    uint16_t accessFlags, thisClass, superClass;
    if (!readU2Safe(buf, bufSize, offset, accessFlags)) return 0;
    offset += 2;
    if (!readU2Safe(buf, bufSize, offset, thisClass)) return 0;
    offset += 2;
    if (!readU2Safe(buf, bufSize, offset, superClass)) return 0;
    offset += 2;

    uint16_t interfaceCount;
    if (!readU2Safe(buf, bufSize, offset, interfaceCount)) return 0;
    offset += 2 + 2 * interfaceCount;

    std::vector<FieldInfo> fields;
    parseFields(buf, bufSize, offset, fields, cp);
    std::vector<MethodInfo> methods;
    parseMethods(buf, bufSize, offset, methods, cp);
    std::vector<AttributeInfo> attributes;
    parseAttributes(buf, bufSize, offset, attributes, cp);

    return offset;
}

bool saveClassToFile(const BYTE* buf, SIZE_T size, int index) {
    if (size == 0 || size > 8192 * 1024) return false;
    std::filesystem::create_directories(basePath);
    std::ostringstream path;
    path << basePath + "\\" << index << ".class";
    std::ofstream out(path.str(), std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(buf), size);
    out.close();
    return true;
}

void Dumper::findSignatures(const BYTE* pattern, size_t pattern_size, int header_size) {
    MEMORY_BASIC_INFORMATION mbi;
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    uintptr_t address = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    uintptr_t endAddress = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
    const SIZE_T maxClassSize = 8192 * 1024;
    int classIndex = 0;

    std::vector<BYTE> readBuf;
    std::vector<BYTE> workingBuf;
    std::vector<BYTE> carry;

    while (address < endAddress && VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi))) {
        uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        uintptr_t regionEnd = regionBase + mbi.RegionSize;

        bool readable = (mbi.State == MEM_COMMIT) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
            !(mbi.Protect & PAGE_GUARD);

        if (readable) {
            for (uintptr_t base = regionBase; base < regionEnd; ) {
                SIZE_T toRead = static_cast<SIZE_T>(std::min<uintptr_t>(maxClassSize, regionEnd - base));
                readBuf.resize(toRead);
                SIZE_T bytesRead = 0;

                if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(base), readBuf.data(), toRead, &bytesRead) || bytesRead < 4) {
                    base += toRead;
                    continue;
                }

                readBuf.resize(bytesRead);

                workingBuf.clear();
                if (!carry.empty()) {
                    workingBuf.insert(workingBuf.end(), carry.begin(), carry.end());
                    carry.clear();
                }
                workingBuf.insert(workingBuf.end(), readBuf.begin(), readBuf.end());

                SIZE_T offset = 0;
                while (offset + pattern_size <= workingBuf.size()) {
                    if (memcmp(workingBuf.data() + offset, pattern, pattern_size) == 0) {
                        SIZE_T available = workingBuf.size() - offset;
                        SIZE_T classSize = getClassSize(workingBuf.data() + offset, available, pattern, header_size);
                        if (classSize == 0 || classSize > maxClassSize) {
                            if (available > 0) {
                                SIZE_T carrySize = std::min<SIZE_T>(available, maxClassSize);
                                carry.assign(workingBuf.begin() + offset, workingBuf.begin() + offset + carrySize);
                            }
                            break;
                        }
                        if (offset + classSize > workingBuf.size()) {
                            SIZE_T available2 = workingBuf.size() - offset;
                            SIZE_T carrySize = std::min<SIZE_T>(available2, maxClassSize);
                            carry.assign(workingBuf.begin() + offset, workingBuf.begin() + offset + carrySize);
                            break;
                        }

                        saveClassToFile(workingBuf.data() + offset, classSize, classIndex++);

                        offset += classSize;
                        continue;
                    }
                    offset++;
                    if ((offset & 0xFFF) == 0) Sleep(0);
                }

                if (offset < workingBuf.size()) {
                    SIZE_T tailSize = workingBuf.size() - offset;
                    if (tailSize > 0) {
                        SIZE_T tailKeep = std::min<SIZE_T>(tailSize, maxClassSize);
                        carry.assign(workingBuf.begin() + offset, workingBuf.begin() + offset + tailKeep);
                    }
                }

                base += toRead;
                if (!g_scanning.load()) return;
            }
        }

        address = regionEnd;
        if (address == 0) break;
        if (!g_scanning.load()) return;

        Sleep(1);
    }
}

void Dumper::findSignaturesAsync(const BYTE* pattern, size_t pattern_size, int header_size) {
    bool expected = false;
    if (!g_scanning.compare_exchange_strong(expected, true)) {
        return;
    }

    if (g_scanThread.joinable()) {
        g_scanThread.join();
    }

    g_scanThread = std::thread([pattern, pattern_size, header_size]() {
        Dumper::findSignatures(pattern, pattern_size, header_size);
        g_scanning.store(false);
    });
}