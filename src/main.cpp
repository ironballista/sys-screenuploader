#include <iostream>
#include <queue>
#include "upload.hpp"
#include "utils.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "project.h"
#include <netinet/in.h>
#include <switch.h>
#include <dirent.h>

#define INNER_HEAP_SIZE 0x50000

extern "C" {
    extern u32 __start__;

    u32 __nx_applet_type = AppletType_None;

    size_t nx_inner_heap_size = INNER_HEAP_SIZE;
    char nx_inner_heap[INNER_HEAP_SIZE];

    void __libnx_init_time(void);
    void __libnx_initheap(void);
    void __appInit(void);
    void __appExit(void);

    // we override libnx internals to do a minimal init
    void __libnx_initheap(void) {
        void*  addr = nx_inner_heap;
        size_t size = nx_inner_heap_size;

        extern char *fake_heap_start;
        extern char *fake_heap_end;

        // setup newlib fake heap
        fake_heap_start = (char*)addr;
        fake_heap_end = (char*)addr + size;
    }

    void __appInit(void) {
        Result rc;
        rc = smInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);
        rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
            SetSysFirmwareVersion fw;
            rc = setsysGetFirmwareVersion(&fw);
            if (R_SUCCEEDED(rc))
                hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
            else
                fatalThrow(rc);
            setsysExit();
        }
        else
            fatalThrow(rc);
        rc = pmdmntInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);
        rc = nsInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        SocketInitConfig sockConf = {
            .tcp_tx_buf_size = 0x800,
            .tcp_rx_buf_size = 0x1000,
            .tcp_tx_buf_max_size = 0x2EE0,
            .tcp_rx_buf_max_size = 0x2EE0,

            .udp_tx_buf_size = 0x0,
            .udp_rx_buf_size = 0x0,

            .sb_efficiency = 4,
        };
        rc = socketInitialize(&sockConf);
        if (R_FAILED(rc))
            fatalThrow(rc);
        rc = pminfoInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        rc = capsaInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        rc = fsInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        rc = timeInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        fsdevMountSdmc();
    }

    void __appExit(void) {
        fsdevUnmountAll();
        timeExit();
        fsExit();
        capsaExit();
        pminfoExit();
        pmdmntExit();
        nsExit();
        socketExit();
        smExit();
    }
}

void initLogger(bool truncate) {
    if (truncate)
        Logger::get().get().truncate();

    Logger::get().none() << "=============================" << std::endl;
    Logger::get().none() << "=============================" << std::endl;
    Logger::get().none() << "=============================" << std::endl;
    Logger::get().none() << "ScreenUploader v" << APP_VERSION << " is starting..." << std::endl;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main(int argc, char **argv) {
    mkdir("sdmc:/config", 0700);
    mkdir("sdmc:/config/sys-screenuploader", 0700);

    initLogger(false);
    Config::get().refresh();
    if (Config::get().error)
        return 0;

    if (!Config::get().keepLogs())
        initLogger(true);

    Result rc;
    CapsAlbumStorage storage;
    FsFileSystem imageFs;
    rc = capsaGetAutoSavingStorage(&storage);
    if (!R_SUCCEEDED(rc)) {
        Logger::get().error() << "capsaGetAutoSavingStorage() failed: " << rc << ", exiting..." << std::endl;
        return 0;
    }
    rc = fsOpenImageDirectoryFileSystem(&imageFs, (FsImageDirectoryId)storage);
    if (!R_SUCCEEDED(rc)) {
        Logger::get().error() << "fsOpenImageDirectoryFileSystem() failed: " << rc << ", exiting..." << std::endl;
        return 0;
    }
    int mountRes = fsdevMountDevice("img", imageFs);
    if (mountRes < 0) {
        Logger::get().error() << "fsdevMountDevice() failed, exiting..." << std::endl;
        return 0;
    }
    Logger::get().info() << "Mounted " << (storage ? "SD" : "NAND") << " storage" << std::endl;

    std::string lastItem = getLastAlbumItem();
    Logger::get().info() << "Current last item: " << lastItem << std::endl;
    Logger::get().close();

    auto last_time = std::filesystem::file_time_type::clock::now();
    auto send_queue = std::queue<std::string>{};

    while (true) {
        auto items = getAlbumItemsPastTimestamp(last_time);
        for (const auto &tmpItem : items) {
            const size_t fs = filesize(tmpItem.string());
            if (fs > 0) {
                Logger::get().info() << "=============================" << std::endl;
                Logger::get().info() << "New item found: " << tmpItem << std::endl;
                Logger::get().info() << "Filesize: " << fs << std::endl;

                send_queue.push(tmpItem.string());
            }
        }

        if(!send_queue.empty()) {
            const auto filename = send_queue.front();
            const size_t fs = filesize(filename);
            bool sent = false;

            for (int i = 0; i < 3; i++) {
                sent = sendFileToServer(filename, fs);
                if (sent)
                    break;
            }
            
            if (!sent) {
                Logger::get().error() << "Unable to send file after 3 retries" << std::endl;
            } else {
                last_time = std::max(last_time, std::filesystem::last_write_time(filename));
                send_queue.pop();
            }
        }

        Logger::get().close();
		svcSleepThread(1e+9);
    }
}
#pragma clang diagnostic pop
