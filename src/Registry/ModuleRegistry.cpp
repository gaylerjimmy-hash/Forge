#include "automation_core/Registry/ModuleRegistry.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace automation_core {
namespace {

bool has_required_fields(const Module& module)
{
    return
        !module.module_id.empty() &&
        !module.module_type.empty() &&
        !module.firmware_version.empty() &&
        !module.protocol_version.empty() &&
        !module.session_id.empty() &&
        !module.connection_id.empty();
}

bool metadata_matches(const Module& left, const Module& right)
{
    return
        left.module_type == right.module_type &&
        left.firmware_version == right.firmware_version &&
        left.protocol_version == right.protocol_version;
}

std::optional<MeasurementValue> convert_value(const std::string& text, CapabilityDataType type) {
    if(type==CapabilityDataType::String||type==CapabilityDataType::Enumeration)return MeasurementValue{text};
    if(type==CapabilityDataType::Boolean){if(text=="true")return MeasurementValue{true};if(text=="false")return MeasurementValue{false};return std::nullopt;}
    if(type==CapabilityDataType::Integer){std::int64_t v{};auto r=std::from_chars(text.data(),text.data()+text.size(),v);if(r.ec==std::errc{}&&r.ptr==text.data()+text.size())return MeasurementValue{v};return std::nullopt;}
    if(type==CapabilityDataType::UnsignedInteger){std::uint64_t v{};auto r=std::from_chars(text.data(),text.data()+text.size(),v);if(r.ec==std::errc{}&&r.ptr==text.data()+text.size())return MeasurementValue{v};return std::nullopt;}
    char* end=nullptr;double v=std::strtod(text.c_str(),&end);if(end==text.c_str()+text.size()&&std::isfinite(v))return MeasurementValue{v};return std::nullopt;
}

std::optional<double> numeric_value(const MeasurementValue& value) {
    if(auto v=std::get_if<std::int64_t>(&value))return static_cast<double>(*v);
    if(auto v=std::get_if<std::uint64_t>(&value))return static_cast<double>(*v);
    if(auto v=std::get_if<double>(&value))return *v;
    return std::nullopt;
}

} // namespace

MeasurementPublishResult ModuleRegistry::publish_measurement(const std::string& connection_id,const MeasurementMessage& message,const TimePoint now) {
    auto it=modules_.find(message.module_id);
    if(it==modules_.end())return {MeasurementPublishStatus::UnknownModule,"UNKNOWN_MODULE","Measurement module is not registered"};
    Module& module=it->second;
    if(module.status==ModuleStatus::Quarantined)return {MeasurementPublishStatus::Quarantined,"QUARANTINED","Module identity is quarantined"};
    if(module.status==ModuleStatus::Offline)return {MeasurementPublishStatus::Offline,"MODULE_OFFLINE","Offline module must rediscover"};
    if(module.connection_id!=connection_id)return {MeasurementPublishStatus::ConnectionMismatch,"CONNECTION_MISMATCH","Measurement arrived on a non-authoritative connection"};
    if(module.session_id!=message.session_id)return {MeasurementPublishStatus::SessionMismatch,"SESSION_MISMATCH","Measurement session does not match registration"};
    if(!module.capabilities_available)return {MeasurementPublishStatus::Unavailable,"CAPABILITIES_UNAVAILABLE","Accepted capabilities are required"};
    const auto cap=std::find_if(module.capabilities.begin(),module.capabilities.end(),[&](const Capability& c){return c.name==message.capability;});
    if(cap==module.capabilities.end())return {MeasurementPublishStatus::UnknownCapability,"UNKNOWN_CAPABILITY","Measurement capability is not declared"};
    if(cap->type!=CapabilityType::Measurement||!cap->data_type||cap->access==CapabilityAccess::Write||cap->access==CapabilityAccess::Command)return {MeasurementPublishStatus::UnknownCapability,"CAPABILITY_NOT_READABLE","Capability does not publish measurements"};
    if(cap->unit!=message.unit)return {MeasurementPublishStatus::InvalidValue,"UNIT_MISMATCH","Measurement unit does not match capability"};
    auto value=convert_value(message.value_text,*cap->data_type);
    if(!value)return {MeasurementPublishStatus::InvalidValue,"VALUE_TYPE","Measurement value does not match capability data type"};
    if(auto s=std::get_if<std::string>(&*value);s&&s->size()>(*cap->data_type==CapabilityDataType::Enumeration?64U:256U))return {MeasurementPublishStatus::InvalidValue,"VALUE_LENGTH","Measurement value is too long"};
    if(auto number=numeric_value(*value);number&&((cap->minimum&&*number<*cap->minimum)||(cap->maximum&&*number>*cap->maximum)))return {MeasurementPublishStatus::InvalidValue,"VALUE_RANGE","Measurement value is outside capability range"};
    auto prior=module.measurements.find(message.capability);
    const bool recovered=prior!=module.measurements.end()&&(!prior->second.operational||prior->second.effective_quality==MeasurementQuality::Stale);
    if(prior!=module.measurements.end()){const auto distance=message.sequence-prior->second.sequence;if(distance==0)return {MeasurementPublishStatus::Duplicate,"","Duplicate measurement ignored"};if(distance>=0x80000000U)return {MeasurementPublishStatus::OutOfOrder,"MEASUREMENT_ORDER","Measurement sequence is stale or out of order"};}
    module.measurements[message.capability]=MeasurementRecord{*value,message.value_text,message.quality,message.quality,message.sequence,message.unit,message.uncertainty,message.raw,message.calibration_revision,now,true};
    return {MeasurementPublishStatus::Accepted,"",recovered?"Measurement recovered":"Measurement accepted"};
}

HeartbeatResult ModuleRegistry::update_heartbeat(
    const std::string& connection_id,
    const HeartbeatMessage& heartbeat,
    const TimePoint now
) {
    const auto iterator = modules_.find(heartbeat.module_id);

    if (iterator == modules_.end()) {
        return {
            HeartbeatStatus::UnknownModule,
            "UNKNOWN_MODULE",
            "Heartbeat module is not registered"
        };
    }

    Module& module = iterator->second;

    if (module.status == ModuleStatus::Quarantined) {
        return {
            HeartbeatStatus::Quarantined,
            "QUARANTINED",
            "Module identity is quarantined"
        };
    }

    if (module.status == ModuleStatus::Offline) {
        return {
            HeartbeatStatus::Offline,
            "MODULE_OFFLINE",
            "Offline module must rediscover before heartbeat"
        };
    }

    if (module.connection_id != connection_id) {
        return {
            HeartbeatStatus::ConnectionMismatch,
            "CONNECTION_MISMATCH",
            "Heartbeat arrived on a non-authoritative connection"
        };
    }

    if (module.session_id != heartbeat.session_id) {
        return {
            HeartbeatStatus::SessionMismatch,
            "SESSION_MISMATCH",
            "Heartbeat session does not match registration"
        };
    }

    if (module.has_heartbeat) {
        const std::uint32_t distance =
            heartbeat.sequence - module.last_heartbeat_sequence;

        if (distance == 0) {
            return {
                HeartbeatStatus::Duplicate,
                "",
                "Duplicate heartbeat ignored"
            };
        }

        if (distance >= 0x80000000U) {
            return {
                HeartbeatStatus::OutOfOrder,
                "HEARTBEAT_ORDER",
                "Heartbeat sequence is stale or out of order"
            };
        }

        if (heartbeat.uptime_ms < module.uptime_ms) {
            return {
                HeartbeatStatus::UptimeRegression,
                "UPTIME_REGRESSION",
                "Heartbeat uptime decreased within the active session"
            };
        }
    }

    module.state = heartbeat.state;
    module.has_heartbeat = true;
    module.last_heartbeat_sequence = heartbeat.sequence;
    module.uptime_ms = heartbeat.uptime_ms;
    module.active_fault_count = heartbeat.active_fault_count;
    module.last_heartbeat_at = now;
    module.last_transition_reason = "heartbeat accepted";

    return {
        HeartbeatStatus::Accepted,
        "",
        "Heartbeat accepted"
    };
}

bool capability_matches(const Capability& a,const Capability& b){
 return a.name==b.name&&a.type==b.type&&a.data_type==b.data_type&&a.access==b.access&&a.unit==b.unit&&a.minimum==b.minimum&&a.maximum==b.maximum&&a.supports_quality==b.supports_quality&&a.supports_calibration==b.supports_calibration&&a.description==b.description;
}
bool capabilities_match(const std::vector<Capability>& a,const std::vector<Capability>& b){
 if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!capability_matches(a[i],b[i]))return false;return true;
}

CapabilityPublishResult ModuleRegistry::publish_capabilities(
    const std::string& connection_id,
    const CapabilitiesMessage& document,
    const TimePoint now
) {
    const auto it=modules_.find(document.module_id);
    if(it==modules_.end())return {CapabilityPublishStatus::UnknownModule,"UNKNOWN_MODULE","Capability module is not registered"};
    Module& module=it->second;
    if(module.status==ModuleStatus::Quarantined)return {CapabilityPublishStatus::Quarantined,"QUARANTINED","Module identity is quarantined"};
    if(module.status==ModuleStatus::Offline)return {CapabilityPublishStatus::Offline,"MODULE_OFFLINE","Offline module must rediscover"};
    if(module.connection_id!=connection_id)return {CapabilityPublishStatus::ConnectionMismatch,"CONNECTION_MISMATCH","Capabilities arrived on a non-authoritative connection"};
    if(module.session_id!=document.session_id)return {CapabilityPublishStatus::SessionMismatch,"SESSION_MISMATCH","Capability session does not match registration"};
    if(module.has_capabilities){
        if(document.revision<module.capability_revision)return {CapabilityPublishStatus::StaleRevision,"CAPABILITY_REVISION","Capability revision is stale"};
        if(document.revision==module.capability_revision){
            if(capabilities_match(module.capabilities,document.items))return {CapabilityPublishStatus::Idempotent,"","Identical capabilities already accepted"};
            return {CapabilityPublishStatus::RevisionConflict,"CAPABILITY_REVISION_CONFLICT","Capability content changed without revision"};
        }
    }
    module.has_capabilities=true;
    module.capabilities_available=true;
    module.capability_revision=document.revision;
    module.capabilities=document.items;
    module.measurements.clear();
    module.capabilities_published_at=now;
    module.last_transition_reason="capabilities accepted";
    return {CapabilityPublishStatus::Accepted,"","Capabilities accepted"};
}

RegistrationResult ModuleRegistry::register_module(
    const Module& module,
    const TimePoint now
)
{
    if (!has_required_fields(module))
    {
        return {
            RegistrationStatus::Rejected,
            "Module registration contains an empty required field"
        };
    }

    const auto conflicting_connection = std::find_if(
        modules_.begin(),
        modules_.end(),
        [&module](const auto& entry) {
            const Module& existing = entry.second;
            return
                existing.module_id != module.module_id &&
                existing.status != ModuleStatus::Offline &&
                existing.connection_id == module.connection_id;
        }
    );

    if (conflicting_connection != modules_.end())
    {
        return {
            RegistrationStatus::Rejected,
            "Connection is already bound to another module"
        };
    }

    const auto existing_it = modules_.find(module.module_id);

    if (existing_it == modules_.end())
    {
        Module registered = module;
        registered.status = ModuleStatus::Active;
        registered.last_transition_at = now;
        registered.last_transition_reason = "module registered";
        modules_.emplace(registered.module_id, std::move(registered));

        return {
            RegistrationStatus::Added,
            "Module registered"
        };
    }

    Module& existing = existing_it->second;

    if (existing.status == ModuleStatus::Quarantined)
    {
        return {
            RegistrationStatus::Quarantined,
            "Module identity is quarantined"
        };
    }

    if (existing.status == ModuleStatus::Active)
    {
        if (existing.session_id != module.session_id)
        {
            return {
                RegistrationStatus::DuplicateIdentity,
                "Module identity is already active on another session"
            };
        }

        if (!metadata_matches(existing, module))
        {
            return {
                RegistrationStatus::Rejected,
                "Module metadata changed within the same session"
            };
        }

        existing.connection_id = module.connection_id;
        existing.status = ModuleStatus::Active;
        existing.last_transition_at = now;
        existing.last_transition_reason = "session rebound to connection";

        if (existing.has_heartbeat)
        {
            existing.last_heartbeat_at = now;
        }

        return {
            RegistrationStatus::Reconnected,
            "Existing session rebound to connection"
        };
    }

    existing = module;
    existing.status = ModuleStatus::Active;
    existing.last_transition_at = now;
    existing.last_transition_reason = "offline module rediscovered";

    return {
        RegistrationStatus::Reconnected,
        "Offline module registered on a new connection"
    };
}

std::vector<std::string> ModuleRegistry::expire_heartbeats(
    const std::chrono::milliseconds timeout,
    const TimePoint now
)
{
    if (timeout.count() <= 0)
    {
        throw std::invalid_argument(
            "heartbeat timeout must be positive"
        );
    }

    std::vector<std::string> expired;

    for (auto& entry : modules_)
    {
        Module& module = entry.second;

        if (module.status != ModuleStatus::Active)
        {
            continue;
        }

        const TimePoint freshness = module.has_heartbeat
            ? module.last_heartbeat_at
            : module.last_transition_at;

        if (now - freshness < timeout)
        {
            continue;
        }

        module.status = ModuleStatus::Offline;
        module.connection_id.clear();
        module.capabilities_available = false;
        for(auto& reading:module.measurements){reading.second.operational=false;reading.second.effective_quality=MeasurementQuality::Unavailable;}
        module.last_transition_at = now;
        module.last_transition_reason = "heartbeat timeout";
        expired.push_back(module.module_id);
    }

    std::sort(expired.begin(), expired.end());
    return expired;
}

std::vector<std::string> ModuleRegistry::expire_measurements(const std::chrono::milliseconds timeout,const TimePoint now) {
    if(timeout.count()<=0)throw std::invalid_argument("measurement timeout must be positive");
    std::vector<std::string> expired;
    for(auto& entry:modules_)for(auto& reading:entry.second.measurements)
        if(reading.second.operational&&reading.second.effective_quality!=MeasurementQuality::Stale&&now-reading.second.accepted_at>=timeout){
            reading.second.effective_quality=MeasurementQuality::Stale;
            expired.push_back(entry.first+"."+reading.first);
        }
    std::sort(expired.begin(),expired.end());return expired;
}

bool ModuleRegistry::mark_offline_by_connection(
    const std::string& connection_id
)
{
    for (auto& entry : modules_)
    {
        Module& module = entry.second;

        if (
            module.connection_id == connection_id &&
            module.status == ModuleStatus::Active
        )
        {
            module.status = ModuleStatus::Offline;
            module.connection_id.clear();
            module.capabilities_available = false;
            for(auto& reading:module.measurements){reading.second.operational=false;reading.second.effective_quality=MeasurementQuality::Unavailable;}
            return true;
        }
    }

    return false;
}

bool ModuleRegistry::quarantine(const std::string& module_id)
{
    const auto it = modules_.find(module_id);

    if (it == modules_.end())
    {
        return false;
    }

    it->second.status = ModuleStatus::Quarantined;
    it->second.capabilities_available = false;
    for(auto& reading:it->second.measurements){reading.second.operational=false;reading.second.effective_quality=MeasurementQuality::Unavailable;}
    return true;
}

std::optional<Module> ModuleRegistry::find(
    const std::string& module_id
) const
{
    const auto it = modules_.find(module_id);

    if (it == modules_.end())
    {
        return std::nullopt;
    }

    return it->second;
}

std::optional<Module> ModuleRegistry::find_by_connection(
    const std::string& connection_id
) const
{
    for (const auto& entry : modules_)
    {
        if (entry.second.connection_id == connection_id)
        {
            return entry.second;
        }
    }

    return std::nullopt;
}

std::vector<Module> ModuleRegistry::list() const
{
    std::vector<Module> modules;
    modules.reserve(modules_.size());

    for (const auto& entry : modules_)
    {
        modules.push_back(entry.second);
    }

    std::sort(
        modules.begin(),
        modules.end(),
        [](const Module& left, const Module& right) {
            return left.module_id < right.module_id;
        }
    );

    return modules;
}

} // namespace automation_core
