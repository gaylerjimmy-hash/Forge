#include "automation_core/Registry/ModuleRegistry.h"

#include <algorithm>

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

} // namespace

RegistrationResult ModuleRegistry::register_module(const Module& module)
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

        return {
            RegistrationStatus::Reconnected,
            "Existing session rebound to connection"
        };
    }

    existing = module;
    existing.status = ModuleStatus::Active;

    return {
        RegistrationStatus::Reconnected,
        "Offline module registered on a new connection"
    };
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
