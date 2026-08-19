#include "automation_core/Protocol/Parser.h"

#include "automation_core/ModuleState.h"

#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace automation_core {
namespace {

std::vector<std::string> split_lines(const std::string& payload) {
    std::vector<std::string> lines;
    std::istringstream stream(payload);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }

    return lines;
}

ParseResult failure(ParseError error, std::string detail) {
    return {std::nullopt, error, std::move(detail)};
}

template <typename Integer>
std::optional<Integer> parse_unsigned_integer(
    const std::string& value
);

std::optional<double> parse_double(const std::string& value) {
    char* end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (end != value.c_str() + value.size()) return std::nullopt;
    return parsed;
}

ParseResult parse_capabilities(const std::vector<std::string>& lines) {
    std::unordered_map<std::string,std::string> fields;
    bool ended=false;
    for(std::size_t i=1;i<lines.size();++i){
        const auto& line=lines[i];
        if(line=="END"){ended=true;for(std::size_t j=i+1;j<lines.size();++j)if(!lines[j].empty())return failure(ParseError::TrailingData,"Data appears after END.");break;}
        const auto pos=line.find('=');
        if(pos==std::string::npos||pos==0||pos==line.size()-1||line.find('=',pos+1)!=std::string::npos)return failure(ParseError::MalformedField,"Malformed field: "+line);
        if(!fields.emplace(line.substr(0,pos),line.substr(pos+1)).second)return failure(ParseError::DuplicateField,"Duplicate field: "+line.substr(0,pos));
    }
    if(!ended)return failure(ParseError::MissingTerminator,"Message is missing END.");
    for(const auto* key:{"MSG","ID","SESSION","REV","COUNT"})if(fields.find(key)==fields.end())return failure(ParseError::MissingField,"Missing required field: "+std::string(key));
    const auto rev=parse_unsigned_integer<std::uint32_t>(fields["REV"]);
    const auto count=parse_unsigned_integer<std::uint32_t>(fields["COUNT"]);
    if(!rev||!count||*count>64)return failure(ParseError::InvalidValue,"Invalid capability revision or count.");
    std::unordered_set<std::string> used{"MSG","ID","SESSION","REV","COUNT"};
    std::vector<Capability> items;
    for(std::uint32_t i=0;i<*count;++i){
        const std::string p="ITEM."+std::to_string(i)+".";
        for(const auto* suffix:{"NAME","TYPE","ACCESS"})if(fields.find(p+suffix)==fields.end())return failure(ParseError::MissingField,"Missing required field: "+p+suffix);
        auto type=parse_capability_type(fields[p+"TYPE"]);
        auto access=parse_capability_access(fields[p+"ACCESS"]);
        if(!type||!access)return failure(ParseError::InvalidValue,"Invalid capability type or access.");
        Capability item;item.name=fields[p+"NAME"];item.type=*type;item.access=*access;
        used.insert(p+"NAME");used.insert(p+"TYPE");used.insert(p+"ACCESS");
        auto take=[&](const char* suffix)->std::optional<std::string>{auto k=p+suffix;auto it=fields.find(k);if(it==fields.end())return std::nullopt;used.insert(k);return it->second;};
        if(auto v=take("DATA_TYPE")){item.data_type=parse_capability_data_type(*v);if(!item.data_type)return failure(ParseError::InvalidValue,"Invalid capability data type.");}
        if(auto v=take("UNIT"))item.unit=*v;
        if(auto v=take("MIN")){item.minimum=parse_double(*v);if(!item.minimum)return failure(ParseError::InvalidValue,"Invalid capability minimum.");}
        if(auto v=take("MAX")){item.maximum=parse_double(*v);if(!item.maximum)return failure(ParseError::InvalidValue,"Invalid capability maximum.");}
        if(auto v=take("QUALITY")){if(*v!="true"&&*v!="false")return failure(ParseError::InvalidValue,"Invalid QUALITY.");item.supports_quality=*v=="true";}
        if(auto v=take("CALIBRATION")){if(*v!="true"&&*v!="false")return failure(ParseError::InvalidValue,"Invalid CALIBRATION.");item.supports_calibration=*v=="true";}
        if(auto v=take("DESCRIPTION"))item.description=*v;
        items.push_back(std::move(item));
    }
    for(const auto& field:fields)if(used.find(field.first)==used.end())return failure(ParseError::UnknownField,"Unknown field: "+field.first);
    return {Message{CapabilitiesMessage{fields["MSG"],fields["ID"],fields["SESSION"],*rev,std::move(items)}},ParseError::None,{}};
}

ParseResult parse_command_message(const std::vector<std::string>& lines, const std::string& type) {
    const bool command = type == "COMMAND";
    const bool ack = type == "COMMAND_ACK";
    const std::unordered_set<std::string> required = command
        ? std::unordered_set<std::string>{"MSG","TX","ID","SESSION","CAP","CAP_REV","PAYLOAD"}
        : std::unordered_set<std::string>{"MSG","TX","ID","SESSION","STATUS"};
    const std::unordered_set<std::string> allowed = command
        ? required : std::unordered_set<std::string>{"MSG","TX","ID","SESSION","STATUS","CODE","DETAIL","RESULT"};
    std::unordered_map<std::string, std::string> fields;
    bool ended = false;
    for (std::size_t i=1; i<lines.size(); ++i) {
        if (lines[i] == "END") { ended=true; for (++i;i<lines.size();++i) if (!lines[i].empty()) return failure(ParseError::TrailingData,"Data appears after END."); break; }
        const auto p=lines[i].find('=');
        if (p==std::string::npos || p==0 || p==lines[i].size()-1 || lines[i].find('=',p+1)!=std::string::npos) return failure(ParseError::MalformedField,"Malformed field: "+lines[i]);
        const auto key=lines[i].substr(0,p);
        if (!allowed.count(key)) return failure(ParseError::UnknownField,"Unknown field: "+key);
        if (!fields.emplace(key,lines[i].substr(p+1)).second) return failure(ParseError::DuplicateField,"Duplicate field: "+key);
    }
    if (!ended) return failure(ParseError::MissingTerminator,"Message is missing END.");
    for (const auto& key:required) if (!fields.count(key)) return failure(ParseError::MissingField,"Missing required field: "+key);
    if (command) { auto rev=parse_unsigned_integer<std::uint32_t>(fields["CAP_REV"]); if(!rev) return failure(ParseError::InvalidValue,"Invalid CAP_REV."); return {Message{CommandMessage{fields["MSG"],fields["TX"],fields["ID"],fields["SESSION"],fields["CAP"],*rev,fields["PAYLOAD"]}},ParseError::None,{}}; }
    const bool accepted=fields["STATUS"]=="ACCEPTED"; const bool rejected=fields["STATUS"]=="REJECTED";
    if (!accepted && !rejected && !(type=="COMMAND_RESULT" && fields["STATUS"]=="SUCCESS") && !(type=="COMMAND_RESULT" && fields["STATUS"]=="FAILURE")) return failure(ParseError::InvalidValue,"Invalid STATUS.");
    if (ack && rejected && !fields.count("CODE")) return failure(ParseError::MissingField,"Rejected COMMAND_ACK requires CODE.");
    if (ack) return {Message{CommandAckMessage{fields["MSG"],fields["TX"],fields["ID"],fields["SESSION"],accepted,fields["CODE"],fields["DETAIL"]}},ParseError::None,{}};
    const bool success=fields["STATUS"]=="SUCCESS";
    return {Message{CommandResultMessage{fields["MSG"],fields["TX"],fields["ID"],fields["SESSION"],success,fields["RESULT"],fields["CODE"],fields["DETAIL"]}},ParseError::None,{}};
}

ParseResult parse_measurement(const std::vector<std::string>& lines) {
    const std::unordered_set<std::string> required{
        "MSG", "ID", "SESSION", "CAP", "SEQ", "VALUE", "QUALITY"
    };
    const std::unordered_set<std::string> optional{
        "UNIT", "UNCERTAINTY", "RAW", "CAL_REV"
    };
    std::unordered_map<std::string, std::string> fields;
    bool ended = false;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        const auto& line = lines[i];
        if (line == "END") {
            ended = true;
            for (++i; i < lines.size(); ++i)
                if (!lines[i].empty()) return failure(ParseError::TrailingData, "Data appears after END.");
            break;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos || pos == 0 || pos == line.size() - 1 ||
            line.find('=', pos + 1) != std::string::npos)
            return failure(ParseError::MalformedField, "Malformed field: " + line);
        const auto key = line.substr(0, pos);
        if (required.count(key) == 0 && optional.count(key) == 0)
            return failure(ParseError::UnknownField, "Unknown field: " + key);
        if (!fields.emplace(key, line.substr(pos + 1)).second)
            return failure(ParseError::DuplicateField, "Duplicate field: " + key);
    }
    if (!ended) return failure(ParseError::MissingTerminator, "Message is missing END.");
    for (const auto& key : required)
        if (fields.count(key) == 0) return failure(ParseError::MissingField, "Missing required field: " + key);
    const auto sequence = parse_unsigned_integer<std::uint32_t>(fields["SEQ"]);
    const auto quality = parse_measurement_quality(fields["QUALITY"]);
    if (!sequence || !quality) return failure(ParseError::InvalidValue, "MEASUREMENT contains an invalid typed value.");
    MeasurementMessage message;
    message.message_id=fields["MSG"]; message.module_id=fields["ID"]; message.session_id=fields["SESSION"];
    message.capability=fields["CAP"]; message.sequence=*sequence; message.value_text=fields["VALUE"]; message.quality=*quality;
    if (fields.count("UNIT")) message.unit=fields["UNIT"];
    if (fields.count("RAW")) message.raw=fields["RAW"];
    if (fields.count("UNCERTAINTY")) {
        message.uncertainty=parse_double(fields["UNCERTAINTY"]);
        if (!message.uncertainty) return failure(ParseError::InvalidValue, "Invalid measurement uncertainty.");
    }
    if (fields.count("CAL_REV")) {
        message.calibration_revision=parse_unsigned_integer<std::uint32_t>(fields["CAL_REV"]);
        if (!message.calibration_revision) return failure(ParseError::InvalidValue, "Invalid calibration revision.");
    }
    return {Message{std::move(message)}, ParseError::None, {}};
}

template <typename Integer>
std::optional<Integer> parse_unsigned_integer(
    const std::string& value
) {
    Integer parsed{};
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);

    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }

    return parsed;
}

} // namespace

ParseResult Parser::parse(const Frame& frame) const {
    if (frame.payload.empty()) {
        return failure(ParseError::EmptyFrame, "Frame payload is empty.");
    }

    const auto lines = split_lines(frame.payload);

    if (lines.empty() || lines.front().empty()) {
        return failure(ParseError::EmptyFrame, "Message type is missing.");
    }

    if (lines.front() == "CAPABILITIES") {
        return parse_capabilities(lines);
    }
    if (lines.front() == "MEASUREMENT") {
        return parse_measurement(lines);
    }
    if (lines.front() == "COMMAND" || lines.front() == "COMMAND_ACK" || lines.front() == "COMMAND_RESULT") {
        return parse_command_message(lines, lines.front());
    }

    const bool is_hello = lines.front() == "HELLO";
    const bool is_heartbeat = lines.front() == "HEARTBEAT";

    if (!is_hello && !is_heartbeat) {
        return failure(
            ParseError::UnknownMessageType,
            "Unsupported message type: " + lines.front()
        );
    }

    const std::vector<std::string> required_fields = is_hello
        ? std::vector<std::string>{
            "MSG", "TYPE", "ID", "FW", "PROTO", "SESSION"
        }
        : std::vector<std::string>{
            "MSG", "ID", "SESSION", "SEQ",
            "UPTIME_MS", "STATE", "FAULTS"
        };

    const std::unordered_set<std::string> allowed_fields{
        required_fields.begin(), required_fields.end()
    };

    std::unordered_map<std::string, std::string> fields;
    bool found_end = false;

    for (std::size_t index = 1; index < lines.size(); ++index) {
        const std::string& line = lines[index];

        if (line == "END") {
            found_end = true;

            for (std::size_t trailing = index + 1;
                 trailing < lines.size();
                 ++trailing) {
                if (!lines[trailing].empty()) {
                    return failure(
                        ParseError::TrailingData,
                        "Data appears after END."
                    );
                }
            }

            break;
        }

        if (line.empty()) {
            return failure(
                ParseError::MalformedField,
                "Blank lines are not allowed inside a message."
            );
        }

        const auto separator = line.find('=');

        if (separator == std::string::npos ||
            separator == 0 ||
            separator == line.size() - 1 ||
            line.find('=', separator + 1) != std::string::npos) {
            return failure(
                ParseError::MalformedField,
                "Malformed field: " + line
            );
        }

        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);

        if (allowed_fields.find(key) == allowed_fields.end()) {
            return failure(
                ParseError::UnknownField,
                "Unknown field: " + key
            );
        }

        if (value.empty()) {
            return failure(
                ParseError::EmptyValue,
                "Field has an empty value: " + key
            );
        }

        if (!fields.emplace(key, value).second) {
            return failure(
                ParseError::DuplicateField,
                "Duplicate field: " + key
            );
        }
    }

    if (!found_end) {
        return failure(
            ParseError::MissingTerminator,
            "Message is missing END."
        );
    }

    for (const auto& key : required_fields) {
        if (fields.find(key) == fields.end()) {
            return failure(
                ParseError::MissingField,
                "Missing required field: " + key
            );
        }
    }

    if (is_hello) {
        HelloMessage hello{
            fields.at("MSG"),
            fields.at("TYPE"),
            fields.at("ID"),
            fields.at("FW"),
            fields.at("PROTO"),
            fields.at("SESSION")
        };

        return {
            Message{std::move(hello)},
            ParseError::None,
            {}
        };
    }

    const auto sequence =
        parse_unsigned_integer<std::uint32_t>(fields.at("SEQ"));
    const auto uptime =
        parse_unsigned_integer<std::uint64_t>(fields.at("UPTIME_MS"));
    const auto fault_count =
        parse_unsigned_integer<std::uint32_t>(fields.at("FAULTS"));
    const auto state = parse_module_state(fields.at("STATE"));

    if (!sequence || !uptime || !fault_count || !state) {
        return failure(
            ParseError::InvalidValue,
            "HEARTBEAT contains an invalid typed value."
        );
    }

    HeartbeatMessage heartbeat{
        fields.at("MSG"),
        fields.at("ID"),
        fields.at("SESSION"),
        *sequence,
        *uptime,
        *state,
        *fault_count
    };

    return {
        Message{std::move(heartbeat)},
        ParseError::None,
        {}
    };
}

} // namespace automation_core
