#include "automation_core/Registry/ModuleRegistry.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

using namespace automation_core;

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "ModuleRegistry: " << message << '\n';
        ++failures;
    }
}

Module make_module(
    std::string module_id = "scale-01",
    std::string session_id = "81A9C5D2",
    std::string connection_id = "connection-1"
)
{
    return {
        std::move(module_id),
        "Scale",
        "1.0.0",
        "1",
        std::move(session_id),
        std::move(connection_id),
        ModuleStatus::Active
    };
}

void test_adds_new_module()
{
    ModuleRegistry registry;

    const auto result = registry.register_module(make_module());

    expect(result.status == RegistrationStatus::Added, "new module not added");
    expect(registry.find("scale-01").has_value(), "added module not found");
}

void test_rebinds_same_session()
{
    ModuleRegistry registry;
    registry.register_module(make_module());

    const auto result = registry.register_module(
        make_module("scale-01", "81A9C5D2", "connection-2")
    );

    const auto module = registry.find("scale-01");

    expect(
        result.status == RegistrationStatus::Reconnected,
        "same session was not treated as reconnect"
    );
    expect(
        module && module->connection_id == "connection-2",
        "reconnect did not update connection binding"
    );
}

void test_rejects_active_duplicate_session()
{
    ModuleRegistry registry;
    registry.register_module(make_module());

    const auto result = registry.register_module(
        make_module("scale-01", "AAAAAAAA", "connection-2")
    );

    const auto module = registry.find("scale-01");

    expect(
        result.status == RegistrationStatus::DuplicateIdentity,
        "active conflicting session was not rejected as duplicate"
    );
    expect(
        module && module->connection_id == "connection-1",
        "duplicate identity replaced authoritative binding"
    );
}


void test_rejects_connection_claimed_by_another_module()
{
    ModuleRegistry registry;
    const auto first = registry.register_module(make_module());

    const auto result = registry.register_module(
        make_module("scale-02", "AAAAAAAA", "connection-1")
    );

    const auto original = registry.find("scale-01");

    expect(
        first.status == RegistrationStatus::Added,
        "test setup failed to add original module"
    );
    expect(
        result.status == RegistrationStatus::Rejected,
        "connection was assigned to two active modules"
    );
    expect(
        original &&
        original->connection_id == "connection-1" &&
        original->status == ModuleStatus::Active,
        "connection conflict changed original registration"
    );
    expect(
        !registry.find("scale-02").has_value(),
        "connection conflict inserted rejected module"
    );
}

void test_offline_module_releases_connection()
{
    ModuleRegistry registry;
    const auto first = registry.register_module(make_module());

    expect(
        first.status == RegistrationStatus::Added,
        "test setup failed to add original module"
    );
    expect(
        registry.mark_offline_by_connection("connection-1"),
        "active connection was not marked offline"
    );

    const auto offline = registry.find("scale-01");

    expect(
        offline &&
        offline->status == ModuleStatus::Offline &&
        offline->connection_id.empty(),
        "offline module retained a stale connection binding"
    );
    expect(
        !registry.find_by_connection("connection-1").has_value(),
        "released connection still resolved to offline module"
    );

    const auto result = registry.register_module(
        make_module("scale-02", "AAAAAAAA", "connection-1")
    );
    const auto rebound = registry.find_by_connection("connection-1");

    expect(
        result.status == RegistrationStatus::Added,
        "released connection could not be assigned to another module"
    );
    expect(
        rebound && rebound->module_id == "scale-02",
        "connection lookup did not resolve to new active owner"
    );
}

void test_accepts_new_session_after_offline()
{
    ModuleRegistry registry;
    registry.register_module(make_module());

    expect(
        registry.mark_offline_by_connection("connection-1"),
        "active module was not marked offline"
    );

    const auto result = registry.register_module(
        make_module("scale-01", "AAAAAAAA", "connection-2")
    );

    const auto module = registry.find("scale-01");

    expect(
        result.status == RegistrationStatus::Reconnected,
        "offline module did not accept new session"
    );
    expect(
        module &&
        module->session_id == "AAAAAAAA" &&
        module->connection_id == "connection-2" &&
        module->status == ModuleStatus::Active,
        "offline reconnect did not replace stale registration"
    );
}

void test_quarantine_blocks_registration()
{
    ModuleRegistry registry;
    registry.register_module(make_module());

    expect(registry.quarantine("scale-01"), "module was not quarantined");

    const auto result = registry.register_module(
        make_module("scale-01", "81A9C5D2", "connection-2")
    );

    expect(
        result.status == RegistrationStatus::Quarantined,
        "quarantined identity accepted registration"
    );
}

void test_rejects_incomplete_module()
{
    ModuleRegistry registry;
    Module module = make_module();
    module.connection_id.clear();

    const auto result = registry.register_module(module);

    expect(
        result.status == RegistrationStatus::Rejected,
        "incomplete module was accepted"
    );
    expect(registry.list().empty(), "rejected module entered registry");
}

void test_lookup_and_sorted_list()
{
    ModuleRegistry registry;
    registry.register_module(make_module("zeta"));
    registry.register_module(make_module("alpha", "BBBBBBBB", "connection-2"));

    const auto by_connection = registry.find_by_connection("connection-2");
    const auto modules = registry.list();

    expect(
        by_connection && by_connection->module_id == "alpha",
        "connection lookup returned wrong module"
    );
    expect(
        modules.size() == 2 &&
        modules[0].module_id == "alpha" &&
        modules[1].module_id == "zeta",
        "module list is not deterministic"
    );
}

HeartbeatMessage make_heartbeat(
    std::uint32_t sequence,
    std::uint64_t uptime_ms,
    ModuleState state = ModuleState::Ready
) {
    return {
        "heartbeat-1",
        "scale-01",
        "81A9C5D2",
        sequence,
        uptime_ms,
        state,
        0
    };
}

void test_updates_heartbeat_health()
{
    ModuleRegistry registry;
    registry.register_module(make_module());
    const auto now = ModuleRegistry::TimePoint{} +
        std::chrono::seconds{5};

    const auto result = registry.update_heartbeat(
        "connection-1",
        make_heartbeat(42, 5000, ModuleState::Degraded),
        now
    );
    const auto module = registry.find("scale-01");

    expect(result.changed(), "valid heartbeat did not update registry");
    expect(
        module &&
        module->has_heartbeat &&
        module->last_heartbeat_sequence == 42 &&
        module->uptime_ms == 5000 &&
        module->state == ModuleState::Degraded &&
        module->last_heartbeat_at == now,
        "heartbeat health fields were not retained"
    );
}

void test_heartbeat_identity_invariants()
{
    ModuleRegistry registry;
    registry.register_module(make_module());

    auto unknown = make_heartbeat(1, 1000);
    unknown.module_id = "missing";
    expect(
        registry.update_heartbeat("connection-1", unknown).status ==
            HeartbeatStatus::UnknownModule,
        "unknown module heartbeat was accepted"
    );

    expect(
        registry.update_heartbeat(
            "connection-2",
            make_heartbeat(1, 1000)
        ).status == HeartbeatStatus::ConnectionMismatch,
        "wrong-connection heartbeat was accepted"
    );

    auto wrong_session = make_heartbeat(1, 1000);
    wrong_session.session_id = "AAAAAAAA";
    expect(
        registry.update_heartbeat(
            "connection-1",
            wrong_session
        ).status == HeartbeatStatus::SessionMismatch,
        "wrong-session heartbeat was accepted"
    );
}

void test_heartbeat_ordering_and_rollover()
{
    ModuleRegistry registry;
    registry.register_module(make_module());
    const auto start = ModuleRegistry::TimePoint{};

    registry.update_heartbeat(
        "connection-1",
        make_heartbeat(10, 1000),
        start
    );

    expect(
        registry.update_heartbeat(
            "connection-1",
            make_heartbeat(10, 1000),
            start + std::chrono::seconds{1}
        ).status == HeartbeatStatus::Duplicate,
        "duplicate heartbeat was not identified"
    );

    expect(
        registry.update_heartbeat(
            "connection-1",
            make_heartbeat(9, 1100),
            start + std::chrono::seconds{1}
        ).status == HeartbeatStatus::OutOfOrder,
        "out-of-order heartbeat was accepted"
    );

    ModuleRegistry rollover;
    rollover.register_module(make_module());
    rollover.update_heartbeat(
        "connection-1",
        make_heartbeat(UINT32_MAX, 1000),
        start
    );
    expect(
        rollover.update_heartbeat(
            "connection-1",
            make_heartbeat(0, 1100),
            start + std::chrono::seconds{1}
        ).status == HeartbeatStatus::Accepted,
        "heartbeat rollover was rejected"
    );
}

void test_rejects_uptime_regression()
{
    ModuleRegistry registry;
    registry.register_module(make_module());
    registry.update_heartbeat(
        "connection-1",
        make_heartbeat(1, 1000)
    );

    expect(
        registry.update_heartbeat(
            "connection-1",
            make_heartbeat(2, 999)
        ).status == HeartbeatStatus::UptimeRegression,
        "uptime regression was accepted"
    );
}

void test_expires_heartbeat_and_registration()
{
    const auto start = ModuleRegistry::TimePoint{};

    ModuleRegistry with_heartbeat;
    with_heartbeat.register_module(make_module(), start);
    with_heartbeat.update_heartbeat(
        "connection-1",
        make_heartbeat(1, 1000),
        start + std::chrono::seconds{1}
    );

    expect(
        with_heartbeat.expire_heartbeats(
            std::chrono::seconds{3},
            start + std::chrono::milliseconds{3999}
        ).empty(),
        "module expired before heartbeat timeout"
    );

    const auto expired = with_heartbeat.expire_heartbeats(
        std::chrono::seconds{3},
        start + std::chrono::seconds{4}
    );
    const auto offline = with_heartbeat.find("scale-01");

    expect(
        expired.size() == 1 && expired.front() == "scale-01",
        "heartbeat timeout did not report module"
    );
    expect(
        offline &&
        offline->status == ModuleStatus::Offline &&
        offline->connection_id.empty(),
        "heartbeat timeout did not release connection"
    );

    ModuleRegistry without_heartbeat;
    without_heartbeat.register_module(make_module(), start);
    expect(
        without_heartbeat.expire_heartbeats(
            std::chrono::seconds{3},
            start + std::chrono::seconds{3}
        ).size() == 1,
        "module without heartbeat did not expire from registration time"
    );
}

void test_duplicate_does_not_refresh_health()
{
    const auto start = ModuleRegistry::TimePoint{};
    ModuleRegistry registry;
    registry.register_module(make_module(), start);
    registry.update_heartbeat(
        "connection-1",
        make_heartbeat(1, 1000),
        start + std::chrono::seconds{1}
    );
    registry.update_heartbeat(
        "connection-1",
        make_heartbeat(1, 1000),
        start + std::chrono::seconds{3}
    );

    expect(
        registry.expire_heartbeats(
            std::chrono::seconds{3},
            start + std::chrono::seconds{4}
        ).size() == 1,
        "duplicate heartbeat incorrectly refreshed module health"
    );
}

void test_multiple_modules_expire_independently()
{
    const auto start = ModuleRegistry::TimePoint{};
    ModuleRegistry registry;
    registry.register_module(
        make_module("scale-01", "81A9C5D2", "connection-1"),
        start
    );
    registry.register_module(
        make_module("flow-01", "AAAAAAAA", "connection-2"),
        start
    );
    registry.update_heartbeat(
        "connection-1",
        make_heartbeat(1, 2000),
        start + std::chrono::seconds{2}
    );

    const auto expired = registry.expire_heartbeats(
        std::chrono::seconds{3},
        start + std::chrono::seconds{3}
    );

    expect(
        expired.size() == 1 && expired.front() == "flow-01",
        "independent heartbeat timelines expired the wrong modules"
    );
    expect(
        registry.find("scale-01") &&
        registry.find("scale-01")->status == ModuleStatus::Active,
        "fresh module expired with stale peer"
    );
}

} // namespace

int run_module_registry_tests()
{
    failures = 0;

    test_adds_new_module();
    test_rebinds_same_session();
    test_rejects_active_duplicate_session();
    test_rejects_connection_claimed_by_another_module();
    test_offline_module_releases_connection();
    test_accepts_new_session_after_offline();
    test_quarantine_blocks_registration();
    test_rejects_incomplete_module();
    test_lookup_and_sorted_list();
    test_updates_heartbeat_health();
    test_heartbeat_identity_invariants();
    test_heartbeat_ordering_and_rollover();
    test_rejects_uptime_regression();
    test_expires_heartbeat_and_registration();
    test_duplicate_does_not_refresh_health();
    test_multiple_modules_expire_independently();

    return failures;
}
