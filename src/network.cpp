#include "network.hpp"
#include "crypto.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using boost::asio::ip::tcp;
using json = nlohmann::json;

PeerNode::PeerNode(const std::string& folder, std::vector<std::string> peers)
    : folder_(folder)
    , peers_(std::move(peers))
    , ver_mgr_(folder)
    , io_ctx_()
    , ssl_ctx_(boost::asio::ssl::context::tlsv12)
{
    ssl_ctx_.use_certificate_chain_file("cert.pem");
    ssl_ctx_.use_private_key_file("key.pem", boost::asio::ssl::context::pem);
}

void PeerNode::run() {
    start_server();
    connect_to_peers();
    io_ctx_.run();
}

void PeerNode::start_server(unsigned short port) {
    auto acceptor = std::make_shared<tcp::acceptor>(io_ctx_, tcp::endpoint(tcp::v4(), port));
    std::function<void()> do_accept;
    do_accept = [this, acceptor, &do_accept]() {
        auto sock = std::make_shared<tcp::socket>(io_ctx_);
        acceptor->async_accept(*sock, [this, sock, &do_accept](auto ec) {
            if (!ec) {
                auto ssl_sock = std::make_shared<ssl_stream>(std::move(*sock), ssl_ctx_);
                ssl_sock->async_handshake(boost::asio::ssl::stream_base::server,
                    [this, ssl_sock](auto ec2) {
                        if (!ec2) handle_session(ssl_sock);
                    });
            }
            do_accept();
        });
    };
    do_accept();
}

void PeerNode::connect_to_peers() {
    for (auto& addr : peers_) {
        auto pos = addr.find(':');
        std::string host = addr.substr(0, pos);
        std::string port = addr.substr(pos+1);
        auto resolver = std::make_shared<tcp::resolver>(io_ctx_);
        resolver->async_resolve(host, port,
            [this, resolver](auto ec, auto results) {
                if (!ec) {
                    auto sock = std::make_shared<tcp::socket>(io_ctx_);
                    boost::asio::async_connect(*sock, results,
                        [this, sock](auto ec2, auto) {
                            if (!ec2) {
                                auto ssl_sock = std::make_shared<ssl_stream>(std::move(*sock), ssl_ctx_);
                                ssl_sock->async_handshake(boost::asio::ssl::stream_base::client,
                                    [this, ssl_sock](auto ec3) {
                                        if (!ec3) handle_session(ssl_sock);
                                    });
                            }
                        });
                }
            });
    }
}

void PeerNode::handle_session(std::shared_ptr<ssl_stream> sock) {
    // Отправляем свой манифест
    std::ifstream in(folder_ + "/.manifest.json");
    std::string manifest((std::istreambuf_iterator<char>(in)), {});
    manifest += "\n\n";
    boost::asio::async_write(*sock, boost::asio::buffer(manifest),
        [this, sock, manifest](auto ec, auto) {
            if (!ec) {
                auto buf = std::make_shared<boost::asio::streambuf>();
                boost::asio::async_read_until(*sock, *buf, "\n\n",
                    [this, sock, buf, manifest](auto ec2, auto) {
                        if (!ec2) {
                            std::istream is(buf.get());
                            std::string peer_manifest((std::istreambuf_iterator<char>(is)), {});
                            process_manifests(manifest, peer_manifest, sock);
                        }
                    });
            }
        });
}

void PeerNode::process_manifests(const std::string& my_manifest,
                                 const std::string& peer_manifest,
                                 std::shared_ptr<ssl_stream> sock)
{
    json my_j = json::parse(my_manifest);
    json peer_j = json::parse(peer_manifest);

    std::vector<std::string> to_send, to_req;
    for (auto& [path, obj] : my_j.items()) {
        std::string my_ver = obj["version_id"];
        if (!peer_j.contains(path) || peer_j[path]["version_id"].get<std::string>() < my_ver)
            to_send.push_back(path);
    }
    for (auto& [path, obj] : peer_j.items()) {
        std::string peer_ver = obj["version_id"];
        if (!my_j.contains(path) || my_j[path]["version_id"].get<std::string>() < peer_ver)
            to_req.push_back(path);
    }

    request_files(sock, to_req);
    for (auto& p : to_send) send_file(p, sock);
}

void PeerNode::request_files(std::shared_ptr<ssl_stream> sock,
                             const std::vector<std::string>& paths)
{
    // Для простоты: отправка JSON-запроса списка файлов
    json req = { {"request", paths} };
    std::string s = req.dump() + "\n\n";
    boost::asio::write(*sock, boost::asio::buffer(s));
    // Ожидаем, что пир пошлёт файлы сразу после
    for (size_t i = 0; i < paths.size(); ++i)
        receive_file(sock);
}

void PeerNode::send_file(const std::string& path,
                         std::shared_ptr<ssl_stream> sock)
{
    // Читаем файл
    std::ifstream in(folder_ + "/" + path, std::ios::binary);
    std::vector<char> data((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

    // Заголовок
    const auto& meta = ver_mgr_.manifest().at(path);
    json hdr = {
        {"path", path},
        {"size", data.size()},
        {"version_id", meta.version_id},
        {"hash", meta.hash}
    };
    std::string h = hdr.dump() + "\n\n";
    boost::asio::write(*sock, boost::asio::buffer(h));
    // Данные
    boost::asio::write(*sock, boost::asio::buffer(data));
}

void PeerNode::receive_file(std::shared_ptr<ssl_stream> sock) {
    boost::asio::streambuf buf;
    boost::asio::read_until(*sock, buf, "\n\n");
    std::istream is(&buf);
    std::string hdr_str((std::istreambuf_iterator<char>(is)), {});
    auto hdr = json::parse(hdr_str);

    std::string path = hdr["path"];
    size_t size = hdr["size"];
    std::string expected_hash = hdr["hash"];
    std::string version_id = hdr["version_id"];

    // Читаем данные
    std::vector<char> data(size);
    boost::asio::read(*sock, boost::asio::buffer(data));

    // Проверка хеша
    auto actual = hex_encode(sha256(
        std::vector<unsigned char>(data.begin(), data.end())));
    if (actual != expected_hash) {
        std::cerr << "Hash mismatch for " << path << "\n";
        return;
    }

    // Сохранение
    std::ofstream out(folder_ + "/" + path, std::ios::binary);
    out.write(data.data(), data.size());

    // Обновление манифеста
    ver_mgr_.load_manifest(); // перезагрузка
    ver_mgr_.manifest().at(path).version_id = version_id;
    ver_mgr_.manifest().at(path).hash       = expected_hash;
    // (timestamp можно обновить через filesystem)
    ver_mgr_.save_manifest();
}