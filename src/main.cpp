#include "network.hpp"
#include "versioning.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: p2p_sync <folder> <peer1:port>[,peer2:port,...]\n";
        return 1;
    }
    std::string folder = argv[1];
    std::string peers_arg = argv[2];

    // Разбор списка peers
    std::vector<std::string> peers;
    std::stringstream ss(peers_arg);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty())
            peers.push_back(item);
    }

    // Инициализация и версионирование
    VersionManager ver_mgr(folder);
    ver_mgr.update_manifest();

    // Запуск P2P-узла
    PeerNode node(folder, std::move(peers));
    node.run();

    return 0;
}
