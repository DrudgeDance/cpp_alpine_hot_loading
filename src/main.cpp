#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <functional>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// This function produces an HTTP response for the given request
template<class Body, class Allocator>
http::response<http::string_body>
handle_request(http::request<Body, http::basic_fields<Allocator>>&& req)
{
    // Returns a bad request response
    auto const bad_request =
    [&req](beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if(req.method() != http::verb::get)
        return bad_request("Unknown HTTP-method");

    // Response
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(req.keep_alive());
    res.body() = "Hello, World!";
    res.prepare_payload();
    return res;
}

// This is the C++17 equivalent of a generic lambda
struct handle_session : public std::enable_shared_from_this<handle_session>
{
    beast::tcp_stream stream;
    beast::flat_buffer buffer;

    explicit
    handle_session(tcp::socket&& socket)
        : stream(std::move(socket))
    {
    }

    void run()
    {
        do_read();
    }

    void do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req = {};

        // Set the timeout.
        stream.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream, buffer, req,
            beast::bind_front_handler(
                &handle_session::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return do_close();

        if(ec)
            return;

        // Send the response
        send_response(handle_request(std::move(req)));
    }

    void send_response(http::response<http::string_body>&& msg)
    {
        auto sp = std::make_shared<http::response<http::string_body>>(std::move(msg));
        res = sp;

        // Write the response
        http::async_write(
            stream,
            *sp,
            beast::bind_front_handler(
                &handle_session::on_write,
                shared_from_this(),
                sp->need_eof()));
    }

    void on_write(bool close, beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return;

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // Read another request
        do_read();
    }

    void do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    // The parser is stored in a std::optional because it needs
    // to be constructed after the stream is created
    http::request<http::string_body> req;
    std::shared_ptr<void> res;
};

int main()
{
    try
    {
        auto const address = net::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(3000);

        // The io_context is required for all I/O
        net::io_context ioc{1};

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};

        std::cout << boost::format("Server starting on port %1%...\n") % port;

        // Start accepting connections asynchronously
        std::function<void()> do_accept;
        do_accept = [&]()
        {
            acceptor.async_accept(
                [&](beast::error_code ec, tcp::socket socket)
                {
                    if (!ec)
                    {
                        std::make_shared<handle_session>(std::move(socket))->run();
                    }
                    do_accept(); // Accept the next connection
                });
        };

        do_accept(); // Start accepting
        
        // Run the I/O service
        ioc.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << boost::format("Error: %1%\n") % e.what();
        return 1;
    }

    return 0;
} 