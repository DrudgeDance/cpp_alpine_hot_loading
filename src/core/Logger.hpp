#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <filesystem>

namespace core {

// Forward declare Logger to define macros
class Logger;

// Convenience macros for logging
#define LOG_TRACE   BOOST_LOG_SEV(core::Logger::get(), boost::log::trivial::trace)
#define LOG_DEBUG   BOOST_LOG_SEV(core::Logger::get(), boost::log::trivial::debug)
#define LOG_INFO    BOOST_LOG_SEV(core::Logger::get(), boost::log::trivial::info)
#define LOG_WARNING BOOST_LOG_SEV(core::Logger::get(), boost::log::trivial::warning)
#define LOG_ERROR   BOOST_LOG_SEV(core::Logger::get(), boost::log::trivial::error)
#define LOG_FATAL   BOOST_LOG_SEV(core::Logger::get(), boost::log::trivial::fatal)

class Logger {
public:
    static void init(const std::string& log_file = "webserver.log") {
        namespace logging = boost::log;
        namespace keywords = boost::log::keywords;
        namespace expr = boost::log::expressions;

        // Create logs directory if it doesn't exist
        std::filesystem::create_directories("logs");
        
        // Construct full log path
        auto log_path = std::filesystem::path("logs") / log_file;

        // Setup file sink
        logging::add_file_log(
            keywords::file_name = log_path.string(),
            keywords::rotation_size = 10 * 1024 * 1024,  // 10MB
            keywords::time_based_rotation = logging::sinks::file::rotation_at_time_point(0, 0, 0),
            keywords::format = (
                expr::stream
                    << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                    << " [" << expr::attr<logging::trivial::severity_level>("Severity") << "]"
                    << " [" << expr::attr<logging::attributes::current_thread_id::value_type>("ThreadID") << "]"
                    << " " << expr::smessage
            )
        );

        // Add common attributes
        logging::add_common_attributes();
        logging::core::get()->add_global_attribute("ThreadID", logging::attributes::current_thread_id());

        // Set the minimum severity level
        logging::core::get()->set_filter(
            logging::trivial::severity >= logging::trivial::info
        );

        // Use direct logging instead of macro
        BOOST_LOG_SEV(get(), boost::log::trivial::info) 
            << "Logger initialized. Log file: " << log_path.string();
    }

    static boost::log::sources::severity_logger<boost::log::trivial::severity_level>& get() {
        static boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;
        return logger;
    }
};

} // namespace core 