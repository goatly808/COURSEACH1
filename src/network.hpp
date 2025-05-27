#pragma once
#include "versioning.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <vector>
#include <memory>

using ssl_stream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

class PeerNode {
public:
    PeerNode(const std::string& folder, std::vector<std::string> peers);
    void run();

private:
    void start_server(unsigned short port = 9000);
    void connect_to_peers();
    void handle_session(std::shared_ptr<ssl_stream> sock);

    void process_manifests(const std::string& my_manifest,
                           const std::string& peer_manifest,
                           std::shared_ptr<ssl_stream> sock);

    void send_file(const std::string& path, std::shared_ptr<ssl_stream> sock);
    void request_files(std::shared_ptr<ssl_stream> sock,
                       const std::vector<std::string>& paths);
    void receive_file(std::shared_ptr<ssl_stream> sock);

    std::string folder_;
    std::vector<std::string> peers_;
    VersionManager ver_mgr_;
    boost::asio::io_context io_ctx_;
    boost::asio::ssl::context ssl_ctx_;
};